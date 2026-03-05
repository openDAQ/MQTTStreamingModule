#include "mqtt_streaming_module/constants.h"
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_json_decoder_fb_impl.h>
#include <mqtt_streaming_module/mqtt_subscriber_fb_impl.h>
#include <opendaq/binary_data_packet_factory.h>
#include <sstream>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_FB_UNSUBSCRIBE_TOUT = 3000;

std::atomic<int> MqttSubscriberFbImpl::localIndex = 0;

MqttSubscriberFbImpl::MqttSubscriberFbImpl(const ContextPtr& ctx,
                                           const ComponentPtr& parent,
                                           const FunctionBlockTypePtr& type,
                                           std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                           const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, generateLocalId()),
      subscriber(subscriber),
      jsonDataWorker(loggerComponent),
      topicForSubscribing(""),
      nestedFbTypes(nullptr),
      enablePreview(false),
      previewIsString(false),
      waitingForData(false)
{
    initComponentStatus();
    initNestedFbTypes();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());
    if (topicForSubscribing.empty())
    {
        readJsonConfig();
    }
    createSignals();
    subscribeToTopic();
}

MqttSubscriberFbImpl::~MqttSubscriberFbImpl()
{
    unsubscribeFromTopic();
}

void MqttSubscriberFbImpl::removed()
{
    FunctionBlock::removed();
    unsubscribeFromTopic();
}

void MqttSubscriberFbImpl::onSignalsMessage(const mqtt::MqttAsyncClient&, const mqtt::MqttMessage& msg)
{
    processMessage(msg);
}

void MqttSubscriberFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (propName == PROPERTY_NAME_SUB_JSON_CONFIG || propName == PROPERTY_NAME_SUB_JSON_CONFIG_FILE)
        {
            if (!objPtr.hasProperty(propName))
            {
                auto propClone = PropertyBuilder(prop.getName())
                                     .setValueType(prop.getValueType())
                                     .setDescription(prop.getDescription())
                                     .setDefaultValue(prop.getValue())
                                     .setVisible(false)
                                     .setReadOnly(true)
                                     .build();
                objPtr.addProperty(propClone);
            }
        }
        else
        {
            if (!objPtr.hasProperty(propName))
            {
                if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
                {
                    objPtr.addProperty(internalProp.clone());
                    objPtr.setPropertyValue(propName, prop.getValue());
                    objPtr.getOnPropertyValueWrite(prop.getName()) += [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&)
                    { propertyChanged(); };
                }
            }
            else
            {
                objPtr.setPropertyValue(propName, prop.getValue());
            }
        }
    }
    readProperties();
}

FunctionBlockTypePtr MqttSubscriberFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_SUB_TOPIC, String("")).setDescription("An MQTT topic to subscribe to for receiving data.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_PUB_QOS, List<IInteger>(0, 1, 2), DEFAULT_PUB_QOS)
                .setDescription(
                    fmt::format("MQTT Quality of Service level for subscribing. It can be 0 (at most once), 1 (at least once), or 2 "
                                "(exactly once). By default it is set to {}.",
                                DEFAULT_SUB_QOS));
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = BoolPropertyBuilder(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, False)
            .setDescription("Enables previewing of the subscribed signal data in the function block's output signal. "
                            "By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = BoolPropertyBuilder(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING, False)
                           .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_SUB_PREVIEW_SIGNAL))
                           .setDescription("Specifies whether the preview signal data type is string. "
                                           "By default it is set to false.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_SUB_JSON_CONFIG, String(""))
                .setDescription("JSON configuration string that defines an MQTT topic and corresponding signals to subscribe to.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_SUB_JSON_CONFIG_FILE, String(""))
                           .setDescription("Path to file where the JSON configuration string is stored.");
        defaultConfig.addProperty(builder.build());
    }
    const auto fbType =
        FunctionBlockType(SUB_FB_NAME,
                          SUB_FB_NAME,
                          "The subscriber MQTT function block allows subscribing to an MQTT topic and converting MQTT payloads into "
                          "openDAQ signal binary data samples.",
                          defaultConfig);
    return fbType;
}

std::string MqttSubscriberFbImpl::generateLocalId()
{
    return std::string(MQTT_LOCAL_SUB_FB_ID_PREFIX + std::to_string(localIndex++));
}

void MqttSubscriberFbImpl::initNestedFbTypes()
{
    nestedFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for manual JSON configuration
    {
        const auto fbType = MqttJsonDecoderFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
}

void MqttSubscriberFbImpl::readProperties()
{
    auto lock = this->getRecursiveConfigLock();
    topicForSubscribing.clear();
    std::string topic;
    if (objPtr.hasProperty(PROPERTY_NAME_SUB_TOPIC))
    {
        auto topicStr = objPtr.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtrOrNull<IString>();
        if (topicStr.assigned())
            topic = topicStr.toStdString();
    }
    setTopic(topic);

    if (objPtr.hasProperty(PROPERTY_NAME_SUB_QOS))
    {
        auto qosProp = objPtr.getPropertyValue(PROPERTY_NAME_SUB_QOS).asPtrOrNull<IInteger>();
        if (qosProp.assigned())
        {
            const uint32_t qos = qosProp.getValue(DEFAULT_SUB_QOS);
            this->qos = (qos > 2) ? DEFAULT_SUB_QOS : qos;
        }
    }
    if (objPtr.hasProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL))
    {
        auto previewProp = objPtr.getPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL).asPtrOrNull<IBoolean>();
        if (previewProp.assigned())
        {
            this->enablePreview = previewProp.getValue(False);
        }
    }

    if (objPtr.hasProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING))
    {
        auto isStringProp = objPtr.getPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).asPtrOrNull<IBoolean>();
        if (isStringProp.assigned())
        {
            this->previewIsString = isStringProp.getValue(False);
        }
    }
}

void MqttSubscriberFbImpl::readJsonConfig()
{
    bool hasJsonConfig = false;
    if (objPtr.hasProperty(PROPERTY_NAME_SUB_JSON_CONFIG))
    {
        const auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG).asPtrOrNull<IString>();
        if (signalConfig.assigned())
        {
            if (!signalConfig.toStdString().empty())
            {
                hasJsonConfig = true;
                setJsonConfig(signalConfig.toStdString());
            }
        }
    }
    if (hasJsonConfig == false && objPtr.hasProperty(PROPERTY_NAME_SUB_JSON_CONFIG_FILE))
    {
        const auto configPath = objPtr.getPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG_FILE).asPtrOrNull<IString>();
        if (configPath.assigned())
        {
            if (!configPath.toStdString().empty())
            {
                auto res = readFileToString(configPath.toStdString());
                if (res.first)
                {
                    hasJsonConfig = true;
                    setJsonConfig(res.second);
                }
                else
                {
                    auto msg = fmt::format("Failed to read JSON config from file: {}", configPath.toStdString());
                    LOG_W("{}", msg);
                    setComponentStatusWithMessage(ComponentStatus::Error, msg);
                }
            }
        }
    }
}

std::pair<bool, std::string> MqttSubscriberFbImpl::readFileToString(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file)
        return std::pair(false, "");

    std::ostringstream buffer;
    buffer << file.rdbuf(); // Read the entire file buffer
    return std::pair(true, buffer.str());
}

void MqttSubscriberFbImpl::setJsonConfig(const std::string config)
{
    jsonDataWorker.setConfig(config);
    auto result = jsonDataWorker.isJsonValid();
    if (result.success)
    {
        auto topic = jsonDataWorker.extractTopic();
        result.success = setTopic(topic);
        if (result.success)
        {
            {
                auto event = objPtr.getOnPropertyValueWrite(PROPERTY_NAME_SUB_TOPIC);
                event.mute();
                objPtr.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, String(topic));
                event.unmute();
            }
            if (const auto signalDscs = jsonDataWorker.extractDescription(); !signalDscs.empty())
            {
                auto fbConfig = MqttJsonDecoderFbImpl::CreateType().createDefaultConfig();
                for (const auto& [signalName, descriptor] : signalDscs)
                {
                    LOG_I("Creating a decoder FB for the signal \"{}\":", signalName);
                    fbConfig.setPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME, descriptor.valueFieldName);
                    fbConfig.setPropertyValue(PROPERTY_NAME_DEC_TS_NAME, descriptor.tsFieldName);
                    if (descriptor.unit.assigned())
                        fbConfig.setPropertyValue(PROPERTY_NAME_DEC_UNIT, descriptor.unit.getSymbol());
                    MqttSubscriberFbImpl::onAddFunctionBlock(JSON_DECODER_FB_NAME, fbConfig);
                }
            }
        }
        else
        {
            result.msg = "Failed to set topic from JSON config.";
        }
    }
    if (!result.success)
    {
        result.msg = fmt::format("JSON config is wrong! {}", result.msg);
        LOG_W("{}", result.msg);
        setComponentStatusWithMessage(ComponentStatus::Error, result.msg);
    }
}

void MqttSubscriberFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto result = unsubscribeFromTopic();
    if (result.success == false)
    {
        LOG_W("Failed to unsubscribe from the previous topic before subscribing to a new one; reason: {}", result.msg);
        return;
    }
    readProperties();
    if (enablePreview)
    {
        if (!outputSignal.assigned())
        {
            createSignals();
        }
        else if ((outputSignal.getDescriptor().getSampleType() == SampleType::String) != previewIsString)
        {
            outputSignal.setDescriptor(DataDescriptorBuilderCopy(outputSignal.getDescriptor())
                                           .setSampleType(previewIsString ? SampleType::String : SampleType::Binary)
                                           .build());
        }
    }
    else
    {
        if (outputSignal.assigned())
        {
            removeSignal(outputSignal);
            outputSignal = nullptr;
        }
    }
    result = subscribeToTopic();
}

bool MqttSubscriberFbImpl::setTopic(std::string topic)
{
    const auto validationStatus = mqtt::MqttDataWrapper::validateTopic(topic, loggerComponent);
    if (validationStatus.success)
    {
        LOG_I("An MQTT topic: {}", topic);
        topicForSubscribing = std::move(topic);
        setComponentStatusWithMessage(ComponentStatus::Ok, "Waiting for data for the topic: " + topicForSubscribing);
        waitingForData = true;
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Invalid topic name: " + validationStatus.msg);
        waitingForData = false;
    }
    return validationStatus.success;
}

DictPtr<IString, IFunctionBlockType> MqttSubscriberFbImpl::onGetAvailableFunctionBlockTypes()
{
    return nestedFbTypes;
}

FunctionBlockPtr MqttSubscriberFbImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{

    FunctionBlockPtr nestedFunctionBlock;
    if (nestedFbTypes.hasKey(typeId))
    {
        auto fbTypePtr = nestedFbTypes.getOrDefault(typeId);
        if (fbTypePtr.getName() == JSON_DECODER_FB_NAME)
        {
            nestedFunctionBlock =
                createWithImplementation<IFunctionBlock, MqttJsonDecoderFbImpl>(context, functionBlocks, fbTypePtr, config);
        }
    }
    if (nestedFunctionBlock.assigned())
    {
        addNestedFunctionBlock(nestedFunctionBlock);
        {
            auto lock = this->getAcquisitionLock2();
            nestedFunctionBlocks.push_back(nestedFunctionBlock);
        }
    }
    else
    {
        DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
    }
    return nestedFunctionBlock;
}

void MqttSubscriberFbImpl::onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock)
{
    {
        auto lock = this->getAcquisitionLock2();
        auto it = std::find_if(nestedFunctionBlocks.begin(),
                               nestedFunctionBlocks.end(),
                               [&functionBlock](const FunctionBlockPtr& fb) { return fb.getObject() == functionBlock.getObject(); });

        if (it != nestedFunctionBlocks.end())
        {
            nestedFunctionBlocks.erase(it);
        }
    }
    FunctionBlockImpl::onRemoveFunctionBlock(functionBlock);
}

void MqttSubscriberFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    if (topicForSubscribing == msg.getTopic())
    {
        if (waitingForData)
        {
            setComponentStatusWithMessage(ComponentStatus::Ok, "Data has been received");
            waitingForData = false;
        }

        std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
        auto acqlock = this->getAcquisitionLock2();

        if (enablePreview)
        {
            const auto outputPacket = BinaryDataPacket(nullptr, outputSignal.getDescriptor(), msg.getData().size());
            memcpy(outputPacket.getData(), msg.getData().data(), msg.getData().size());
            outputSignal.sendPacket(outputPacket);
        }

        for (const auto& fb : nestedFunctionBlocks)
        {
            if (fb.assigned())
            {
                auto decoderFb = reinterpret_cast<MqttJsonDecoderFbImpl*>(*fb);
                decoderFb->processMessage(jsonObjStr);
            }
        }
    }
}

void MqttSubscriberFbImpl::createSignals()
{
    auto lock = this->getRecursiveConfigLock(); // ???
    if (enablePreview)
    {
        const auto signalDsc = DataDescriptorBuilder().setSampleType(previewIsString ? SampleType::String : SampleType::Binary).build();
        outputSignal = createAndAddSignal(DEFAULT_VALUE_SIGNAL_LOCAL_ID, signalDsc);
    }
}

std::string MqttSubscriberFbImpl::getSubscribedTopic() const
{
    return topicForSubscribing;
}

void MqttSubscriberFbImpl::clearSubscribedTopic()
{
    topicForSubscribing.clear();
}

MqttSubscriberFbImpl::CmdResult MqttSubscriberFbImpl::subscribeToTopic()
{
    CmdResult result{false};
    if (subscriber)
    {
        auto lambda = [this](const mqtt::MqttAsyncClient& client, mqtt::MqttMessage& msg) { this->onSignalsMessage(client, msg); };
        const auto topic = getSubscribedTopic();
        if (!topic.empty())
        {
            LOG_I("Trying to subscribe to the topic : {}", topic);
            subscriber->setMessageArrivedCb(topic, lambda);
            if (auto subRes = subscriber->subscribe(topic, qos); subRes.success == false)
            {
                LOG_W("Failed to subscribe to the topic: \"{}\"; reason: {}", topic, subRes.msg);
                setComponentStatusWithMessage(ComponentStatus::Error, "Some topics failed to subscribe! The reason: " + subRes.msg);
                waitingForData = false;
                result = {false, "Failed to subscribe to the topic: \"" + topic + "\"; reason: " + subRes.msg};
            }
            else
            {
                // subscriber->subscribe(...) is asynchronous. It puts command in queue and returns immediately.
                LOG_D("Trying to subscribe to the topic: {}", topic);
                setComponentStatusWithMessage(ComponentStatus::Ok, "Waiting for data for the topic: " + topicForSubscribing);
                waitingForData = true;
                result = {true, "", result.token};
            }
        }
        else
        {
            result = {false, "Couldn't subscribe to an empty topic"};
            LOG_W("{}", result.msg);
        }
    }
    else
    {
        const std::string msg = "MQTT subscriber client is not set!";
        setComponentStatusWithMessage(ComponentStatus::Error, msg);
        result = {false, msg};
    }
    return result;
}

MqttSubscriberFbImpl::CmdResult MqttSubscriberFbImpl::unsubscribeFromTopic()
{
    CmdResult result{true};
    if (subscriber)
    {
        const auto topic = getSubscribedTopic();
        if (!topic.empty())
        {
            subscriber->setMessageArrivedCb(topic, nullptr);
            mqtt::CmdResult unsubRes = subscriber->unsubscribe(topic);
            if (unsubRes.success)
                unsubRes = subscriber->waitForCompletion(unsubRes.token, MQTT_FB_UNSUBSCRIBE_TOUT);

            if (unsubRes.success)
            {
                clearSubscribedTopic();
                LOG_I("The topic \'{}\' has been unsubscribed successfully", topic);
                result = {true};
            }
            else
            {
                const auto msg = fmt::format("Failed to unsubscribe from the topic \'{}\'; reason: {}", topic, unsubRes.msg);
                LOG_W("{}", msg);
                setComponentStatusWithMessage(ComponentStatus::Error, msg);
                result = {false, msg};
            }
        }
    }
    else
    {
        const std::string msg = "MQTT subscriber client is not set!";
        setComponentStatusWithMessage(ComponentStatus::Error, msg);
        result = {false, msg};
    }
    return result;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
