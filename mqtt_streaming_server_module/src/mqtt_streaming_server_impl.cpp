#include <mqtt_streaming_server_module/mqtt_streaming_server_impl.h>
#include <coretypes/impl.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <opendaq/server_type_factory.h>
#include <opendaq/device_private.h>
#include <opendaq/reader_factory.h>
#include <opendaq/search_filter_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_info_internal_ptr.h>

#include <boost/asio/dispatch.hpp>
#include <opendaq/input_port_factory.h>
#include <opendaq/thread_name.h>


#include <rapidjson/document.h>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <mqtt_streaming_server_module/constants.h>
#include <MqttDataWrapper.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE
using namespace daq;

MqttStreamingServerImpl::MqttStreamingServerImpl(const DevicePtr &rootDevice,
                                                 const PropertyObjectPtr &config,
                                                 const ContextPtr &context)
    : Server(SERVER_ID_AND_CAPABILITY, config, rootDevice, context)
    , signals(List<ISignal>())
    , rootDeviceGlobalId(rootDevice.getGlobalId().toStdString())
    , logger(context.getLogger())
    , loggerComponent(logger.getOrAddComponent(id))
    , serverStopped(false)
    , publisher()
    , connectionSettings({DEFAULT_BROKER_ADDRESS,
                          DEFAULT_PORT,
                          DEFAULT_USERNAME,
                          DEFAULT_PASSWORD,
                          rootDevice.getGlobalId().toStdString()})
{
    auto info = rootDevice.getInfo();
    if (info.hasServerCapability(SERVER_ID_AND_CAPABILITY))
        DAQ_THROW_EXCEPTION(InvalidStateException,
                            fmt::format("Device \"{}\" already has an {} server capability.",
                                        info.getName(),
                                        SERVER_ID_AND_CAPABILITY));

    readMqttSettings();

    ServerCapabilityConfigPtr serverCapabilityStreaming = ServerCapability(SERVER_ID_AND_CAPABILITY,
                                                                           SERVER_ID_AND_CAPABILITY,
                                                                           ProtocolType::Streaming)
                                                              .setPrefix(MQTT_PREFIX)
                                                              .setConnectionType(CONNECTION_TYPE)
                                                              .setPort(connectionSettings.port);

    info.asPtr<IDeviceInfoInternal>(true).addServerCapability(serverCapabilityStreaming);

    this->context.getOnCoreEvent() += event(&MqttStreamingServerImpl::coreEventCallback);

    maxPacketReadCount = config.getPropertyValue(PROPERTY_NAME_MAX_PACKET_READ_COUNT);
    processingThreadSleepTime = std::chrono::milliseconds(
        config.getPropertyValue(PROPERTY_NAME_POLLING_PERIOD));

    buffer.data.resize(maxPacketReadCount);
    buffer.timestamps.resize(maxPacketReadCount);

    setupMqttPublisher();
    connectSignalReaders();
    startProcessingThread();
}

MqttStreamingServerImpl::~MqttStreamingServerImpl()
{
    stopServerInternal();
}

void MqttStreamingServerImpl::addSignalsOfComponent(ComponentPtr& component)
{
    if (component.supportsInterface<ISignal>())
    {
        LOG_I("Added Signal: {};", component.getGlobalId());
        //serverHandler->addSignal(component.asPtr<ISignal>(true));
    }
    else if (component.supportsInterface<IFolder>())
    {
        auto nestedComponents = component.asPtr<IFolder>().getItems(search::Recursive(search::Any()));
        for (const auto& nestedComponent : nestedComponents)
        {
            if (nestedComponent.supportsInterface<ISignal>())
            {
                LOG_I("Added Signal: {};", nestedComponent.getGlobalId());
                //serverHandler->addSignal(nestedComponent.asPtr<ISignal>(true));
            }
        }
    }
}

void MqttStreamingServerImpl::componentAdded(ComponentPtr& /*sender*/, CoreEventArgsPtr& eventArgs)
{
    ComponentPtr addedComponent = eventArgs.getParameters().get("Component");

    auto addedComponentGlobalId = addedComponent.getGlobalId().toStdString();
    if (addedComponentGlobalId.find(rootDeviceGlobalId) != 0)
        return;

    LOG_I("Added Component: {};", addedComponentGlobalId);
    addSignalsOfComponent(addedComponent);
}

void MqttStreamingServerImpl::componentRemoved(ComponentPtr& sender, CoreEventArgsPtr& eventArgs)
{
    StringPtr removedComponentLocalId = eventArgs.getParameters().get("Id");

    auto removedComponentGlobalId =
        sender.getGlobalId().toStdString() + "/" + removedComponentLocalId.toStdString();
    if (removedComponentGlobalId.find(rootDeviceGlobalId) != 0)
        return;

    LOG_I("Component: {}; is removed", removedComponentGlobalId);
    //serverHandler->removeComponentSignals(removedComponentGlobalId);
}

void MqttStreamingServerImpl::componentUpdated(ComponentPtr& updatedComponent)
{
    auto updatedComponentGlobalId = updatedComponent.getGlobalId().toStdString();
    if (updatedComponentGlobalId.find(rootDeviceGlobalId) != 0)
        return;

    LOG_I("Component: {}; is updated", updatedComponentGlobalId);

    // remove all registered signal of updated component since those might be modified or removed
    //serverHandler->removeComponentSignals(updatedComponentGlobalId);

    // add updated versions of signals
    addSignalsOfComponent(updatedComponent);
}

void MqttStreamingServerImpl::coreEventCallback(ComponentPtr& sender, CoreEventArgsPtr& eventArgs)
{
    switch (static_cast<CoreEventId>(eventArgs.getEventId()))
    {
        case CoreEventId::ComponentAdded:
            componentAdded(sender, eventArgs);
            break;
        case CoreEventId::ComponentRemoved:
            componentRemoved(sender, eventArgs);
            break;
        case CoreEventId::ComponentUpdateEnd:
            componentUpdated(sender);
            break;
        default:
            break;
    }
}

void MqttStreamingServerImpl::setupMqttPublisher()
{
    publisher.disconnect();
    publisher.setServerURL(connectionSettings.mqttUrl);
    publisher.setClientId(connectionSettings.clientId);
    publisher.setUsernamePasswrod(connectionSettings.username, connectionSettings.password);
    publisher.setConnectedCb([this]() { LOG_I("MQTT: Connection established"); });

    LOG_I("MQTT: Trying to connect to MQTT broker ({})", connectionSettings.mqttUrl);
    publisher.connect();
}

void MqttStreamingServerImpl::sendData(const std::string& topic, const ChannelData& data, SizeT readAmount)
{
    if (readAmount == 0)
        return;

    const auto jsonMessages = prepareJsonMessages(data, readAmount);
    if (publisher.isConnected() == mqtt::MqttConnectionStatus::connected) {
        for (const auto& jsonMessage : jsonMessages) {
            std::string err;
            auto status = publisher.publish(topic, (void*) jsonMessage.c_str(), jsonMessage.length(), &err);
            if (!status) {
                LOG_W("Failed to publish data to {}; reason - {}", topic, err);
            }
        }
    }
}

std::vector<std::string> MqttStreamingServerImpl::prepareJsonMessages(const ChannelData& data, SizeT dataAmount)
{
    std::vector<std::string> result;

    for (size_t i = 0; i < dataAmount; ++i) {
        result.emplace_back(mqtt::MqttDataWrapper::serializeSampleData({data.data[i], data.timestamps[i]}));
    }

    return result;
}

std::string MqttStreamingServerImpl::prepareJsonTopics()
{
    return mqtt::MqttDataWrapper::serializeSignalDescriptors(signals);
}

void MqttStreamingServerImpl::sendTopicList()
{
    std::string topic = mqtt::MqttDataWrapper::buildSignalsTopic(rootDeviceGlobalId);
    auto topicsMessage = prepareJsonTopics();
    if (publisher.isConnected() == mqtt::MqttConnectionStatus::connected) {
        bool status = publisher.publish(topic, (void*) topicsMessage.c_str(), topicsMessage.length(), nullptr, 1, nullptr, true);
        if (!status) {
            LOG_W("Failed to publish topics list to {}", topic);
        } else {
            topicsAreSent = true;
        }
    }
}

void MqttStreamingServerImpl::readMqttSettings()
{
    connectionSettings.mqttUrl = (std::string)config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS);
    connectionSettings.port = config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT);
    connectionSettings.username = (std::string)config.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME);
    connectionSettings.password = (std::string)config.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD);
    connectionSettings.clientId = rootDeviceGlobalId;
}

void MqttStreamingServerImpl::processingThreadFunc()
{
    daqNameThread("MqttC2DSread");
    LOG_I("Streaming-to-device read thread started")
    while (processingThreadRunning)
    {
        {
            std::scoped_lock lock(readersSync);
            if (!topicsAreSent)
                sendTopicList();
            bool hasPacketsToRead;
            do
            {
                hasPacketsToRead = false;
                for (size_t i = 0; i < streamReaders.size(); ++i)
                {
                    auto& reader = streamReaders[i];
                    if (reader.getAvailableCount() == 0)
                        continue;
                    daq::SizeT readAmount = maxPacketReadCount;
                    reader.readWithDomain(buffer.data.data(), buffer.timestamps.data(), &readAmount);
                    sendData(mqtt::MqttDataWrapper::buildTopicFromId(
                                 signals[i].getGlobalId().toStdString()),
                             buffer,
                             readAmount);

                    if (reader.getAvailableCount() > 0)
                        hasPacketsToRead = true;
                }
            }
            while(hasPacketsToRead);
        }

        std::this_thread::sleep_for(processingThreadSleepTime);
    }
    LOG_I("Streaming-to-device read thread stopped");
}

void MqttStreamingServerImpl::startProcessingThread()
{
    if (processingThreadRunning.exchange(true))
        return;
    processingThread = std::thread(&MqttStreamingServerImpl::processingThreadFunc, this);
}

void MqttStreamingServerImpl::stopProcessingThread()
{
    processingThreadRunning = false;
    if (processingThread.joinable())
    {
        processingThread.join();
        LOG_I("Processing thread joined");
    }
}

void MqttStreamingServerImpl::stopServerInternal()
{

    if (serverStopped.exchange(true))
        return;

    this->context.getOnCoreEvent() -= event(&MqttStreamingServerImpl::coreEventCallback);
    if (const DevicePtr rootDevice = this->rootDeviceRef.assigned() ? this->rootDeviceRef.getRef() : nullptr;
        rootDevice.assigned() && !rootDevice.isRemoved())
    {
        const auto info = rootDevice.getInfo();
        const auto infoInternal = info.asPtr<IDeviceInfoInternal>();
        if (info.hasServerCapability(SERVER_ID_AND_CAPABILITY))
            infoInternal.removeServerCapability(SERVER_ID_AND_CAPABILITY);
    }
    stopProcessingThread();
}

void MqttStreamingServerImpl::connectSignalReaders()
{

    auto allSignals = List<ISignal>();
    if (const DevicePtr rootDevice = this->rootDeviceRef.assigned() ? this->rootDeviceRef.getRef() : nullptr; rootDevice.assigned())
        allSignals = rootDevice.getSignals(search::Recursive(search::Any()));
    for (const SignalPtr& sig : allSignals)
    {
        if (!isSignalCompatible(sig))
            continue;

        LOG_I("Adding the Signal to reader: {};", sig.getGlobalId());
        addReader(sig);
    }
}

bool MqttStreamingServerImpl::isSignalCompatible(const SignalPtr& signal)
{
    if (!signal.getDomainSignal().assigned()) {
        LOG_I("Signal {} doesn't has domain signal assigned, skipping", signal.getGlobalId().toStdString());
        return false;
    }
    if (!signal.getDescriptor().assigned()) {
        LOG_I("Signal {} doesn't has descriptor assigned, skipping", signal.getGlobalId().toStdString());
        return false;
    }
    if (signal.getDescriptor().getDimensions().getCount() > 0)
    {
        LOG_I("Signal {} has uncompatible sample type, skipping", signal.getGlobalId().toStdString());
        return false;
    }
    if (const auto sampleType = signal.getDescriptor().getSampleType();
        sampleType != SampleType::Float64 &&
        sampleType != SampleType::Float32 &&
        sampleType != SampleType::Int8 &&
        sampleType != SampleType::Int16 &&
        sampleType != SampleType::Int32 &&
        sampleType != SampleType::Int64 &&
        sampleType != SampleType::UInt8 &&
        sampleType != SampleType::UInt16 &&
        sampleType != SampleType::UInt32 &&
        sampleType != SampleType::UInt64)
    {
        LOG_I("Signal {} has uncompatible sample type, skipping", signal.getGlobalId().toStdString());
        return false;
    }
    if (const auto domainSampleType = signal.getDomainSignal().getDescriptor().getSampleType();
        domainSampleType != SampleType::Int64 &&
        domainSampleType != SampleType::UInt64)
    {
        LOG_I("Signal {} has uncompatible domain signal sample type, skipping", signal.getGlobalId().toStdString());
        return false;
    }
    return true;
}

void MqttStreamingServerImpl::populateDefaultConfigFromProvider(const ContextPtr& context, const PropertyObjectPtr& config)
{
    if (!context.assigned())
        return;
    if (!config.assigned())
        return;

    auto options = context.getModuleOptions("OpenDAQMqttStreamingServerModule");
    for (const auto& [key, value] : options)
    {
        if (config.hasProperty(key))
        {
            config->setPropertyValue(key, value);
        }
    }
}

PropertyObjectPtr MqttStreamingServerImpl::createDefaultConfig(const ContextPtr& context)
{
    //auto defaultConfig = MqttStreamingServerHandler::createDefaultConfig();
    auto defaultConfig = PropertyObject();

    const auto pollingPeriodProp = IntPropertyBuilder(PROPERTY_NAME_POLLING_PERIOD, DEFAULT_POLLING_PERIOD)
                                       .setMinValue(1)
                                       .setMaxValue(65535)
                                       .setDescription("Polling period in milliseconds "
                                                       "which specifies how often the server collects and sends "
                                                       "subscribed signals' data to clients")
                                       .build();
    defaultConfig.addProperty(pollingPeriodProp);

    const auto maxPacketReadCountProp = IntPropertyBuilder(PROPERTY_NAME_MAX_PACKET_READ_COUNT, DEFAULT_MAX_PACKET_READ_COUNT)
                                                .setMinValue(1)
                                                .setDescription("Specifies the size of a pre-allocated packet buffer into "
                                                                "which packets are dequeued. The size determines the amount of "
                                                                "packets that can be read in one dequeue call. Should be greater "
                                                                "than the amount of packets generated per polling period for best "
                                                                "performance.")
                                                .build();
    defaultConfig.addProperty(maxPacketReadCountProp);

    const auto url = StringPropertyBuilder(PROPERTY_NAME_MQTT_BROKER_ADDRESS, DEFAULT_BROKER_ADDRESS)
                          .setDescription("")
                          .build();
    defaultConfig.addProperty(url);

    const auto port = IntPropertyBuilder(PROPERTY_NAME_MQTT_BROKER_PORT, DEFAULT_PORT)
                                            .setMinValue(1)
                                            .setMaxValue(65535)
                                            .setDescription("Port is not used")
                                            .build();
    defaultConfig.addProperty(port);

    const auto username = StringPropertyBuilder(PROPERTY_NAME_MQTT_USERNAME, DEFAULT_USERNAME)
                              .setDescription("")
                              .build();
    defaultConfig.addProperty(username);

    const auto password = StringPropertyBuilder(PROPERTY_NAME_MQTT_PASSWORD, DEFAULT_PASSWORD)
                         .setDescription("")
                         .build();
    defaultConfig.addProperty(password);

    populateDefaultConfigFromProvider(context, defaultConfig);
    return defaultConfig;
}

PropertyObjectPtr MqttStreamingServerImpl::populateDefaultConfig(const PropertyObjectPtr& config, const ContextPtr& context)
{
    const auto defConfig = createDefaultConfig(context);
    for (const auto& prop : defConfig.getAllProperties())
    {
        const auto name = prop.getName();
        if (config.hasProperty(name))
            defConfig.setPropertyValue(name, config.getPropertyValue(name));
    }

    return defConfig;
}

ServerTypePtr MqttStreamingServerImpl::createType(const ContextPtr& context)
{
    return ServerType(
        SERVER_ID_AND_CAPABILITY,
        "openDAQ MQTT Streaming server",
        "Streams data over MQTT",
        MqttStreamingServerImpl::createDefaultConfig(context));
}

void MqttStreamingServerImpl::onStopServer()
{
    stopServerInternal();
}

StreamingPtr MqttStreamingServerImpl::onGetStreaming()
{
    return nullptr;
}

void MqttStreamingServerImpl::addReader(SignalPtr signalToRead)
{
    std::scoped_lock lock(readersSync);
    signals.pushBack(signalToRead);
    streamReaders.emplace_back(StreamReaderBuilder()
                                    .setSignal(signalToRead)
                                    .setValueReadType(SampleType::Float64)
                                    .setDomainReadType(SampleType::Int64)
                                    .setSkipEvents(true)
                                    .build());
}

void MqttStreamingServerImpl::removeReader(SignalPtr signalToRead)
{

}

OPENDAQ_DEFINE_CLASS_FACTORY_WITH_INTERFACE(
    INTERNAL_FACTORY, MqttStreamingServer, daq::IServer,
    daq::DevicePtr, rootDevice,
    PropertyObjectPtr, config,
    const ContextPtr&, context
)

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE
