#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/handler_factory.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_publisher_fb_impl.h>
#include <opendaq/event_packet_params.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

std::atomic<int> MqttPublisherFbImpl::localIndex = 0;

MqttPublisherFbImpl::MqttPublisherFbImpl(const ContextPtr& ctx,
                                         const ComponentPtr& parent,
                                         const FunctionBlockTypePtr& type,
                                         std::shared_ptr<mqtt::MqttAsyncClient> mqttClient,
                                         const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, getLocalId()),
      mqttClient(mqttClient),
      jsonDataWorker(loggerComponent),
      inputPortCount(0),
      running(true),
      hasError(false)
{
    initComponentStatus();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    handler = HandlerFactory::create(this->config, globalId.toStdString());
    updateInputPorts();
    validateInputPorts();
    runReaderThread();
}

MqttPublisherFbImpl::~MqttPublisherFbImpl()
{
    running = false;
    readerThread.join();
}

FunctionBlockTypePtr MqttPublisherFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_PUB_TOPIC_MODE, List<IString>("single-topic", "multiple-topic"), 0)
                .setDescription(
                    "Selects whether to publish all signals to separate MQTT topics (one per signal, single-topic mode) or to a single "
                    "topic (multiple-topic mode), one for all signals. Choose 0 for single-topic mode, 1 for multiple-topic mode. By "
                    "default it is set to single-topic mode.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_PUB_TOPIC_NAME, "")
                .setDescription(
                    "Topic name for publishing in multiple-topic mode. If left empty, the Publisher's Global ID is used as the topic name.")
                .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 1"));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = BoolPropertyBuilder(PROPERTY_NAME_PUB_SHARED_TS, False)
                           .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 1"))
                           .setDescription("Enables the use of a shared timestamp for all signals when publishing in multiple-topic mode. "
                                           "By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            BoolPropertyBuilder(PROPERTY_NAME_PUB_GROUP_VALUES, False)
                .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 0"))
                .setDescription(
                    "Enables the use of a sample pack for a signal when publishing in single-topic mode. By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = BoolPropertyBuilder(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, False)
                           .setDescription("Uses signal names as JSON field names instead of Global IDs. By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, DEFAULT_PUB_PACK_SIZE)
                .setMinValue(1)
                .setVisible(EvalValue(std::string("($") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 0) && " + std::string("($") +
                                      PROPERTY_NAME_PUB_GROUP_VALUES + ")"))
                .setDescription(fmt::format("Set the size of the sample pack when publishing grouped values in single-topic mode. "
                                            "By default it is set to {}.",
                                            DEFAULT_PUB_PACK_SIZE));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_PUB_QOS, DEFAULT_PUB_QOS)
                .setMinValue(0)
                .setMaxValue(2)
                .setSuggestedValues(List<IInteger>(0, 1, 2))
                .setDescription(
                    fmt::format("MQTT Quality of Service level for published messages. It can be 0 (at most once), 1 (at least once), or 2 "
                                "(exactly once). By default it is set to {}.",
                                DEFAULT_PUB_QOS));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_PUB_READ_PERIOD, DEFAULT_PUB_READ_PERIOD)
                .setMinValue(0)
                .setUnit(Unit("ms"))
                .setDescription(fmt::format("Polling period in milliseconds, which specifies how often the server collects and publishes "
                                            "the connected signals’ data to an MQTT broker. By default it is set to {} ms.",
                                            DEFAULT_PUB_READ_PERIOD));
        defaultConfig.addProperty(builder.build());
    }
    const auto fbType = FunctionBlockType(PUB_FB_NAME,
                                          PUB_FB_NAME,
                                          "The Publisher function block allows converting openDAQ signal samples into JSON messages and "
                                          "publishing them to MQTT topics in different ways.",
                                          defaultConfig);
    return fbType;
}

PublisherFbConfig MqttPublisherFbImpl::getFbConfig() const
{
    return config;
}

void MqttPublisherFbImpl::onConnected(const InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    updateInputPorts();
    LOG_T("Connected to port {}", inputPort.getLocalId());
    validateInputPorts();
}

void MqttPublisherFbImpl::onDisconnected(const InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();

    updateInputPorts();
    LOG_T("Disconnected from port {}", inputPort.getLocalId());
    validateInputPorts();
}

void MqttPublisherFbImpl::updateInputPorts()
{
    for (auto it = signalContexts.begin(); it != signalContexts.end();)
    {
        if (!it->inputPort.getSignal().assigned())
        {
            removeInputPort(it->inputPort);
            it = signalContexts.erase(it);
        }
        else
            ++it;
    }

    const auto inputPort = createAndAddInputPort(fmt::format("Input{}", inputPortCount++), PacketReadyNotification::SameThread);

    signalContexts.emplace_back(SignalContext{0, inputPort});
    for (size_t i = 0; i < signalContexts.size(); i++)
        signalContexts[i].index = i;
}

void MqttPublisherFbImpl::validateInputPorts()
{
    const auto status = handler->validateSignalContexts(signalContexts);
    hasError = !status.success;
    if (!status.success)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Some connected signals were invalidated!");
        for (const auto& msg : status.messages)
        {
            LOG_E("{}", msg);
        }
    }
    else
    {
        setComponentStatus(ComponentStatus::Ok);
        handler->signalListChanged(signalContexts);
    }
}

void MqttPublisherFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
                objPtr.getOnPropertyValueWrite(prop.getName()) +=
                    [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { propertyChanged(); };
            }
        }
        objPtr.setPropertyValue(propName, prop.getValue());
    }
    readProperties();
}

void MqttPublisherFbImpl::readProperties()
{
    auto lock = this->getRecursiveConfigLock();
    int tmpTopicMode = readProperty<int, IInteger>(PROPERTY_NAME_PUB_TOPIC_MODE, 0);
    if (tmpTopicMode < static_cast<int>(TopicMode::_count) && tmpTopicMode >= 0)
        config.topicMode = static_cast<TopicMode>(tmpTopicMode);
    else
        config.topicMode = TopicMode::Single;
    config.sharedTs = readProperty<bool, IBoolean>(PROPERTY_NAME_PUB_SHARED_TS, false);
    config.groupValues = readProperty<bool, IBoolean>(PROPERTY_NAME_PUB_GROUP_VALUES, false);
    config.useSignalNames = readProperty<bool, IBoolean>(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, false);
    config.groupValuesPackSize = readProperty<size_t, IInteger>(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, DEFAULT_PUB_PACK_SIZE);
    config.qos = readProperty<int, IInteger>(PROPERTY_NAME_PUB_QOS, DEFAULT_PUB_QOS);
    if (config.qos < 0 || config.qos > 2)
        config.qos = DEFAULT_PUB_QOS;
    config.periodMs = readProperty<int, IInteger>(PROPERTY_NAME_PUB_READ_PERIOD, DEFAULT_PUB_READ_PERIOD);
    if (config.periodMs < 0)
        config.periodMs = DEFAULT_PUB_READ_PERIOD;
    config.topicName = readProperty<std::string, IString>(PROPERTY_NAME_PUB_TOPIC_NAME, DEFAULT_PUB_TOPIC_NAME);
}

void MqttPublisherFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    readProperties();
    handler = HandlerFactory::create(this->config, globalId.toStdString());
    validateInputPorts();
}

template <typename retT, typename intfT>
retT MqttPublisherFbImpl::readProperty(const std::string& propertyName, const retT defaultValue)
{
    retT returnValue{};
    if (objPtr.hasProperty(propertyName))
    {
        auto property = objPtr.getPropertyValue(propertyName).asPtrOrNull<intfT>();
        if (property.assigned())
        {
            returnValue = property.getValue(defaultValue);
        }
    }
    return returnValue;
}

void MqttPublisherFbImpl::runReaderThread()
{
    LOGP_D("Using separate thread for rendering")

    readerThread = std::thread([this] { readerLoop(); });
}

void MqttPublisherFbImpl::readerLoop()
{
    while (running)
    {
        MqttData msgs;
        if (hasError == false)
        {
            auto lock = this->getRecursiveConfigLock();
            msgs = handler->processSignalContexts(signalContexts);
        }
        sendMessages(msgs);
        std::this_thread::sleep_for(std::chrono::milliseconds(config.periodMs));
    }
}

void MqttPublisherFbImpl::sendMessages(const MqttData& data)
{
    for (const auto& [topic, msg] : data)
    {
        auto status = mqttClient->publish(topic, (void*)msg.c_str(), msg.length(), config.qos);
        if (!status.success)
        {
            LOG_W("Failed to publish data to {}; reason - {}", topic, status.msg);
        }
    }
}

std::string MqttPublisherFbImpl::getLocalId()
{
    return std::string(MQTT_LOCAL_PUB_FB_ID_PREFIX + std::to_string(localIndex++));
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
