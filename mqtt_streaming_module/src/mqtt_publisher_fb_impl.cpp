#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/handler_factory.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_publisher_fb_impl.h>
#include <opendaq/binary_data_packet_factory.h>
#include <opendaq/event_packet_params.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

std::atomic<int> MqttPublisherFbImpl::localIndex = 0;

MqttPublisherFbImpl::MqttPublisherFbImpl(const ContextPtr& ctx,
                                         const ComponentPtr& parent,
                                         const FunctionBlockTypePtr& type,
                                         std::shared_ptr<mqtt::MqttAsyncClient> mqttClient,
                                         const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, generateLocalId()),
      mqttClient(mqttClient),
      jsonDataWorker(loggerComponent),
      inputPortCount(0),
      running(true),
      hasSignalError(false),
      hasSettingError(false),
      signalStatus(MQTT_PUB_FB_SIG_STATUS_TYPE,
                   MQTT_PUB_FB_SIG_STATUS_NAME,
                   statusContainer,
                   signalStatusMap,
                   SignalStatus::NotConnected,
                   context.getTypeManager()),
      publishingStatus(MQTT_PUB_FB_PUB_STATUS_TYPE,
                       MQTT_PUB_FB_PUB_STATUS_NAME,
                       statusContainer,
                       publishingStatusMap,
                       PublishingStatus::Ok,
                       context.getTypeManager()),
      settingStatus(MQTT_PUB_FB_SET_STATUS_TYPE,
                    MQTT_PUB_FB_SET_STATUS_NAME,
                    statusContainer,
                    settingStatusMap,
                    SettingStatus::Valid,
                    context.getTypeManager()),
      hasSkippedMsg(false)
{
    initComponentStatus();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    handler = HandlerFactory::create(this->template getWeakRefInternal<IFunctionBlock>(), this->config, globalId.toStdString());
    updatePortsAndSignals(true);
    validateInputPorts();
    updateSchema();
    updateStatuses();
    runReaderThread();
}

MqttPublisherFbImpl::~MqttPublisherFbImpl()
{
    if (readerThread.joinable())
    {
        running = false;
        readerThread.join();
    }
}

void MqttPublisherFbImpl::removed()
{
    if (readerThread.joinable())
    {
        running = false;
        readerThread.join();
    }
    {
        auto lock = this->getRecursiveConfigLock();
        auto lockProcessing = std::scoped_lock(processingMutex);
        handler = nullptr;
    }
    FunctionBlock::removed();
}

FunctionBlockTypePtr MqttPublisherFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_PUB_TOPIC_MODE, List<IString>("TopicPerSignal", "SingleTopic"), 0)
                .setDescription(
                    "Selects whether to publish all signals to separate MQTT topics (one per signal, TopicPerSignal mode) or to a single "
                    "topic (SingleTopic mode), one for all signals. Choose 0 for TopicPerSignal mode, 1 for SingleTopic mode. By "
                    "default it is set to TopicPerSignal mode.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_PUB_TOPIC_NAME, "")
                .setDescription(
                    "Topic name for publishing in SingleTopic mode. If left empty, the Publisher's Global ID is used as the topic name.")
                .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 1"));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = SelectionPropertyBuilder(PROPERTY_NAME_PUB_VALUE_FIELD_NAME, List<IString>("GlobalID", "LocalID", "Name"), 0)
        .setDescription("Describes how to name a JSON value field. By default it is set to GlobalID.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            BoolPropertyBuilder(PROPERTY_NAME_PUB_GROUP_VALUES, False)
                .setDescription(
                    "Enables the use of a sample pack for a signal. By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = IntPropertyBuilder(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, DEFAULT_PUB_PACK_SIZE)
                           .setMinValue(1)
                           .setVisible(EvalValue(std::string("($") + PROPERTY_NAME_PUB_GROUP_VALUES + ")"))
                           .setDescription(fmt::format("Set the size of the sample pack when publishing grouped values. "
                                                       "By default it is set to {}.",
                                                       DEFAULT_PUB_PACK_SIZE));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_PUB_QOS, List<IInteger>(0, 1, 2), DEFAULT_PUB_QOS)
                .setDescription(
                    fmt::format("MQTT Quality of Service level for published messages. It can be 0 (at most once), 1 (at least once), or 2 "
                                "(exactly once). By default it is set to {}.",
                                DEFAULT_PUB_QOS));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = BoolPropertyBuilder(PROPERTY_NAME_PUB_PREVIEW_SIGNAL, False)
        .setDescription("Enables previewing of the publishing data in the function block's output signal. "
                        "By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_PUB_READ_PERIOD, DEFAULT_PUB_READ_PERIOD)
                .setMinValue(0)
                .setUnit(Unit("ms"))
                .setDescription(fmt::format("Polling period in milliseconds, which specifies how often the server calls an internal reader to "
                                            "collect and publish the connected signals’ data to an MQTT broker. By default it is set to {} ms.",
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
    auto lockProcessing = std::scoped_lock(processingMutex);

    updatePortsAndSignals(true);
    LOG_T("Connected to port {}", inputPort.getLocalId());
    validateInputPorts();
    updateTopics();
    updateStatuses();
}

void MqttPublisherFbImpl::onDisconnected(const InputPortPtr& inputPort)
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    updatePortsAndSignals(true);
    LOG_T("Disconnected from port {}", inputPort.getLocalId());
    validateInputPorts();
    updateTopics();
    updateStatuses();
}

void MqttPublisherFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    readProperties();
    handler = HandlerFactory::create(this->template getWeakRefInternal<IFunctionBlock>(), this->config, globalId.toStdString());
    updatePortsAndSignals(false);
    validateInputPorts();
    updateTopics();
    updateSchema();
    updateStatuses();
}

void MqttPublisherFbImpl::updatePortsAndSignals(bool reassignPorts)
{
    if (reassignPorts)
    {
        for (auto it = signalContexts.begin(); it != signalContexts.end();)
        {
            if (!it->inputPort.getSignal().assigned())
            {
                removeInputPort(it->inputPort);
                if (it->previewSignal.assigned() && (!commonPreviewSignal.assigned() || commonPreviewSignal != it->previewSignal))
                {
                    removeSignal(it->previewSignal);
                }
                it = signalContexts.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    if (config.enablePreview)
    {
        if (config.topicMode == TopicMode::Single && !commonPreviewSignal.assigned())
        {
            const auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Binary).build();
            commonPreviewSignal = createAndAddSignal(fmt::format("{}{}", PUB_PREVIEW_SIGNAL_NAME, "Common"), signalDsc);
        }
    }

    for (auto it = signalContexts.begin(); it != signalContexts.end();)
    {
        it->inputPort.setListener(this->template borrowPtr<InputPortNotificationsPtr>());
        if (!it->inputPort.getSignal().assigned())
        {
            ++it;
            continue;
        }
        if (config.enablePreview)
        {
            if (config.topicMode == TopicMode::Single)
            {
                if (it->previewSignal != commonPreviewSignal)
                {
                    if (it->previewSignal.assigned())
                        removeSignal(it->previewSignal);
                    it->previewSignal = commonPreviewSignal;
                }
            }
            else if (config.topicMode == TopicMode::PerSignal)
            {
                if (!it->previewSignal.assigned() || (commonPreviewSignal.assigned() && commonPreviewSignal == it->previewSignal))
                {
                    const auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Binary).build();
                    it->previewSignal = createAndAddSignal(fmt::format("{}{}", PUB_PREVIEW_SIGNAL_NAME, size_t(it->index)), signalDsc);
                }
            }
        }
        else
        {
            if (it->previewSignal.assigned())
            {
                if (!commonPreviewSignal.assigned() || commonPreviewSignal != it->previewSignal)
                {
                    removeSignal(it->previewSignal);
                }
                it->previewSignal = nullptr;
            }
        }
        ++it;
    }
    if (commonPreviewSignal.assigned() && (!config.enablePreview || config.topicMode == TopicMode::PerSignal))
    {
        removeSignal(commonPreviewSignal);
        commonPreviewSignal = nullptr;
    }
    if (reassignPorts)
    {
        const auto inputPort = createAndAddInputPort(fmt::format("Input{}", size_t(inputPortCount)), PacketReadyNotification::SameThread);
        signalContexts.emplace_back(SignalContext{size_t(inputPortCount++), inputPort, {}, 0, 0, nullptr});
    }
}

void MqttPublisherFbImpl::updateStatuses()
{
    auto buildErrorString = [this](const std::vector<std::string>& errors)
    {
        std::string allMessages;
        for (const auto& msg : errors)
        {
            allMessages += msg + "; ";
        }
        return allMessages;
    };

    if (hasSignalError)
    {
        signalStatus.setStatus(SignalStatus::Invalid, buildErrorString(signalErrors));
    }
    else if (signalContexts.size() == 1)        // no one input port is connected
    {
        signalStatus.setStatus(SignalStatus::NotConnected);
    }
    else
    {
        signalStatus.setStatus(SignalStatus::Valid);
    }

    if (hasSettingError)
    {
        settingStatus.setStatus(SettingStatus::Invalid, buildErrorString(settingErrors));
    }
    else
    {
        settingStatus.setStatus(SettingStatus::Valid);
    }

    if (hasSkippedMsg)
    {
        publishingStatus.setStatus(PublishingStatus::SampleSkipped, buildPublishingStatusMessage());
    }
    else
    {
        publishingStatus.setStatus(PublishingStatus::Ok);
    }
    updateComponentStatus();
}

void MqttPublisherFbImpl::validateInputPorts()
{
    hasSkippedMsg = false;
    if (signalContexts.size() == 1)     // no one input port is connected
    {
        hasSignalError = false;
    }
    else
    {
        const auto status = handler->validateSignalContexts(signalContexts);
        hasSignalError = !status.success;
        signalErrors = std::move(status.messages);
        if (status.success)
            handler->signalListChanged(signalContexts);
    }
}

void MqttPublisherFbImpl::updateTopics()
{
    const auto topics = handler->getTopics(signalContexts);
    objPtr.getProperty(PROPERTY_NAME_PUB_TOPICS).asPtr<IPropertyInternal>().setValueProtected(topics);
}

void MqttPublisherFbImpl::updateSchema()
{
    const auto schema = handler->getSchema();
    objPtr.getProperty(PROPERTY_NAME_PUB_SCHEMA).asPtr<IPropertyInternal>().setValueProtected(String(schema));
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
                objPtr.setPropertyValue(propName, prop.getValue());
                objPtr.getOnPropertyValueWrite(prop.getName()) += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args)
                { propertyChanged(); };
            }
        }
        else
        {
            objPtr.setPropertyValue(propName, prop.getValue());
        }

        if (propName == PROPERTY_NAME_PUB_TOPIC_NAME)
        {
            auto builder = ListPropertyBuilder(PROPERTY_NAME_PUB_TOPICS, List<IString>())
            .setReadOnly(true)
                .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_PUB_TOPIC_MODE + " == 0"))
                .setDescription("List of currently used MQTT topics for publishing in TopicPerSignal mode.");

            objPtr.addProperty(builder.build());
        }
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_PUB_SCHEMA, "").setReadOnly(true).setDescription("Publishing JSON schema.");

        objPtr.addProperty(builder.build());
    }
    readProperties();
}

void MqttPublisherFbImpl::readProperties()
{
    int tmpTopicMode = readProperty<int, IInteger>(PROPERTY_NAME_PUB_TOPIC_MODE, 0);

    config.groupValues = readProperty<bool, IBoolean>(PROPERTY_NAME_PUB_GROUP_VALUES, false);
    int tmpValueFieldName = (readProperty<int, IInteger>(PROPERTY_NAME_PUB_VALUE_FIELD_NAME, 0));
    config.groupValuesPackSize = readProperty<size_t, IInteger>(PROPERTY_NAME_PUB_GROUP_VALUES_PACK_SIZE, DEFAULT_PUB_PACK_SIZE);
    config.qos = readProperty<int, IInteger>(PROPERTY_NAME_PUB_QOS, DEFAULT_PUB_QOS);
    config.periodMs = readProperty<int, IInteger>(PROPERTY_NAME_PUB_READ_PERIOD, DEFAULT_PUB_READ_PERIOD);
    config.topicName = readProperty<std::string, IString>(PROPERTY_NAME_PUB_TOPIC_NAME, globalId.toStdString());
    config.enablePreview = readProperty<bool, IBoolean>(PROPERTY_NAME_PUB_PREVIEW_SIGNAL, false);
    settingErrors.clear();
    hasSettingError = false;

    if (tmpValueFieldName < static_cast<int>(SignalValueJSONKey::_count) && tmpValueFieldName >= 0)
    {
        config.valueFieldName = static_cast<SignalValueJSONKey>(tmpValueFieldName);
    }
    else
    {
        config.valueFieldName = SignalValueJSONKey::GlobalID;
        hasSettingError = true;
        settingErrors.push_back(fmt::format("{} property has invalid value.", PROPERTY_NAME_PUB_VALUE_FIELD_NAME));
    }

    if (tmpTopicMode < static_cast<int>(TopicMode::_count) && tmpTopicMode >= 0)
    {
        config.topicMode = static_cast<TopicMode>(tmpTopicMode);
    }
    else
    {
        config.topicMode = TopicMode::PerSignal;
        hasSettingError = true;
        settingErrors.push_back("Topic mode has invalid value.");
    }

    if (config.topicName.empty())
    {
        config.topicName = globalId.toStdString();
        hasEmptyTopic = (config.topicMode == TopicMode::Single);
    }
    else
    {
        hasEmptyTopic = false;
    }

    if (config.topicMode == TopicMode::Single)
    {
        auto result = mqtt::MqttDataWrapper::validateTopic(config.topicName, loggerComponent);
        hasSettingError = !result.success;
        settingErrors.push_back(std::move(result.msg));
    }
    if (config.qos < 0 || config.qos > 2)
    {
        config.qos = DEFAULT_PUB_QOS;
        hasSettingError = true;
        settingErrors.push_back("QoS level must be 0, 1, or 2.");
    }
    if (config.periodMs < 0)
    {
        config.periodMs = DEFAULT_PUB_READ_PERIOD;
        hasSettingError = true;
        settingErrors.push_back("Reader period must be non-negative.");
    }
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
        {
            MqttData msgs;
            auto lockProcessing = std::scoped_lock(processingMutex);
            if (hasSignalError == false && hasSettingError == false)
            {
                msgs = handler->processSignalContexts(signalContexts);
            }
            sendMessages(msgs);
            updatePublishingStatus();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config.periodMs));
    }
}

void MqttPublisherFbImpl::sendMessages(const MqttData& data)
{
    for (const auto& [signal, topic, msg] : data)
    {
        if (signal.assigned())
        {
            const auto outputPacket = BinaryDataPacket(nullptr, signal.getDescriptor(), msg.size());
            memcpy(outputPacket.getData(), msg.data(), msg.size());
            signal.sendPacket(outputPacket);
        }
        auto status = mqttClient->publish(topic, (void*)msg.c_str(), msg.length(), config.qos);
        if (!status.success)
        {
            hasSkippedMsg = true;
            lastSkippedReason = std::move(status.msg);
        }
    }
}

std::string MqttPublisherFbImpl::generateLocalId()
{
    return std::string(MQTT_LOCAL_PUB_FB_ID_PREFIX + std::to_string(localIndex++));
}

void MqttPublisherFbImpl::updateComponentStatus()
{
    std::string componentMsg;
    if (hasSignalError)
    {
        componentMsg.append("Signal status is invalid: ");
        for (const auto& m : signalErrors)
            componentMsg.append(m + "; ");
    }
    else if (signalContexts.size() == 1)        // no one input port is connected
    {
        componentMsg.append("No input ports are connected! ");
    }

    if (hasSettingError)
    {
        componentMsg.append("Settings are invalid: ");
        for (const auto& m : settingErrors)
            componentMsg.append(m + "; ");
    }

    if (hasEmptyTopic)
    {
        componentMsg.append("Topic property is empty! Using FB Global ID as topic name. ");
    }

    if (hasSkippedMsg)
    {
        componentMsg.append("Publishing warning: ");
        componentMsg.append(buildPublishingStatusMessage());
    }

    if (hasSignalError || hasSettingError)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, componentMsg);
    }
    else if (signalContexts.size() == 1 || hasEmptyTopic || hasSkippedMsg)
    {
        setComponentStatusWithMessage(ComponentStatus::Warning, componentMsg);
    }
    else
    {
        setComponentStatus(ComponentStatus::Ok);
    }
}

void MqttPublisherFbImpl::updatePublishingStatus()
{
    bool changed = false;
    if (hasSkippedMsg)
    {
        changed = publishingStatus.setStatus(PublishingStatus::SampleSkipped, buildPublishingStatusMessage());
    }
    else
    {
        changed = publishingStatus.setStatus(PublishingStatus::Ok);
    }
    if (changed)
        updateComponentStatus();
}

inline std::string MqttPublisherFbImpl::buildPublishingStatusMessage()
{
    return fmt::format("Some sample were not published! Last reason is \"{}\"", lastSkippedReason);
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
