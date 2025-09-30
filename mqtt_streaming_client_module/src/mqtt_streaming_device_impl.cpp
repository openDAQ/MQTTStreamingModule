#include "mqtt_streaming_client_module/mqtt_receiver_fb_impl.h"
#include <mqtt_streaming_client_module/mqtt_streaming_device_impl.h>
#include "mqtt_streaming_client_module/constants.h"

#include <opendaq/device_info_factory.h>
#include <opendaq/component_deserialize_context_factory.h>
#include <opendaq/deserialize_component_ptr.h>
#include <opendaq/component_status_container_private_ptr.h>
#include <opendaq/function_block_type_factory.h>
#include <coretypes/function_factory.h>
#include <coreobjects/property_object_protected_ptr.h>

#include <rapidjson/document.h>
#include <boost/algorithm/string.hpp>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

 std::atomic<int> MqttStreamingDeviceImpl::localIndex = 0;

MqttStreamingDeviceImpl::MqttStreamingDeviceImpl(const ContextPtr& ctx,
                                                     const ComponentPtr& parent,
                                                     const PropertyObjectPtr& config)
     : Device(ctx, parent, getLocalId())
    , connectionStatus(Enumeration("ConnectionStatusType", "Connected", this->context.getTypeManager()))
    , subscriber(std::make_shared<mqtt::MqttAsyncClient>())
{
    this->name = MQTT_DEVICE_NAME;

    connectionSettings.mqttUrl = config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS).asPtr<IString>().toStdString();
    connectionSettings.port = config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT);
    connectionSettings.username = config.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME).asPtr<IString>().toStdString();
    connectionSettings.password = config.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD).asPtr<IString>().toStdString();
    connectionSettings.clientId = globalId.toStdString();

    connectionString = std::string(DaqMqttDevicePrefix) + "://" + connectionSettings.mqttUrl + ":" + std::to_string(connectionSettings.port);

    int initTimeout = config.getPropertyValue(PROPERTY_NAME_INIT_DELAY);

    initComponentStatus();  

    setupMqttSubscriber();
    if (!waitForConnection(initTimeout))
    {
        LOG_E("MQTT: could not connect to MQTT broker within {} ms", initTimeout);
        DAQ_THROW_EXCEPTION(CreateFailedException, "could not connect to MQTT broker within {} ms", initTimeout);
    }

    LOG_I("MQTT: Connection established");
    receiveSignalTopics(initTimeout);
}

void MqttStreamingDeviceImpl::removed()
{
    Device::removed();
}

DeviceInfoPtr MqttStreamingDeviceImpl::onGetInfo()
{
    return DeviceInfo(connectionString, MQTT_DEVICE_NAME);
}

void MqttStreamingDeviceImpl::setupMqttSubscriber()
{
    subscriber->disconnect();
    subscriber->setServerURL(connectionSettings.mqttUrl);
    subscriber->setClientId(connectionSettings.clientId);
    subscriber->setUsernamePasswrod(connectionSettings.username, connectionSettings.password);

    connectedDone = false;
    connectedPromise = std::promise<bool>();
    connectedFuture = connectedPromise.get_future();

    subscriber->setConnectedCb([this] {
        bool expected = false;
        if (connectedDone.compare_exchange_strong(expected, true)) {
            connectedPromise.set_value(true);
        }
    });

    LOG_I("MQTT: Trying to connect to MQTT broker ({})", connectionSettings.mqttUrl);
    subscriber->connect();
}

bool MqttStreamingDeviceImpl::waitForConnection(const int timeoutMs)
{
    bool res = (connectedFuture.wait_for(std::chrono::milliseconds(timeoutMs))
                    == std::future_status::ready
                && connectedFuture.get() == true);
    subscriber->setConnectedCb(nullptr);
    return res;
}

void MqttStreamingDeviceImpl::receiveSignalTopics(const int timeoutMs)
{
    subscriber->setMessageArrivedCb(
        std::bind(&MqttStreamingDeviceImpl::onSignalsMessage, this, std::placeholders::_1, std::placeholders::_2)
        );
    subscriber->subscribe(TOPIC_ALL_SIGNALS, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));    // TODO : remove it!

    subscriber->unsubscribe(TOPIC_ALL_SIGNALS);
    subscriber->setMessageArrivedCb(nullptr);
}

void MqttStreamingDeviceImpl::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg)
{
    const std::string signalList(msg.getData().begin(), msg.getData().end());
    const std::string topic = msg.getTopic();
    auto [status, data] = mqtt::MqttDataWrapper::parseSignalDescriptors(topic, signalList);
    if (status.ok) {
        deviceMap.insert({std::move(data.first), std::move(data.second)});
    } else {
        for (const auto& s : status.msg)
            LOG_W("Data error: {}", s);
    }
}

DictPtr<IString, IFunctionBlockType> MqttStreamingDeviceImpl::onGetAvailableFunctionBlockTypes()
{
    fbTypes = Dict<IString, IFunctionBlockType>();
    for (const auto& device : deviceMap)
    {
        auto defaultConfig = PropertyObject();
        auto signalDict = Dict<IString, IDataDescriptor>();
        for (const auto& signal : device.second) {
            auto builder = DataDescriptorBuilder().setSampleType(SampleType::Float64);
            if (!signal.name.empty())
                builder.setName(signal.name);
            if (!signal.unit.empty()) {
                builder.setUnit(Unit(signal.unit));
            }
            auto signalDsc = builder.build();
            signalDict.set(signal.topic, signalDsc);
        }
        defaultConfig.addProperty(DictProperty(PROPERTY_NAME_SIGNAL_LIST, signalDict));

        const auto fbType = FunctionBlockType(device.first,
                                              device.first,
                                              "",
                                              defaultConfig);


        fbTypes.set(fbType.getId(), fbType);
    }
    return fbTypes;
}

FunctionBlockPtr MqttStreamingDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        auto lock = this->getAcquisitionLock();
        if (fbTypes.hasKey(typeId))
        {
            auto fbTypePtr = fbTypes.getOrDefault(typeId);
            nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttReceiverFbImpl>(context, functionBlocks, fbTypePtr, typeId, subscriber, config);
            addNestedFunctionBlock(nestedFunctionBlock);
            setComponentStatus(ComponentStatus::Ok);
        } else {
            setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

std::string MqttStreamingDeviceImpl::getLocalId()
{
    return std::string(MQTT_LOCAL_DEVICE_ID_PREFIX + std::to_string(localIndex++));
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
