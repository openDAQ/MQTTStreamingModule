#include "mqtt_streaming_module/constants.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_json_receiver_fb_impl.h>
#include <mqtt_streaming_module/mqtt_json_decoder_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_JSON_FB_UNSUBSCRIBE_TOUT = 3000;

std::atomic<int> MqttJsonReceiverFbImpl::localIndex = 0;

MqttJsonReceiverFbImpl::MqttJsonReceiverFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                       const PropertyObjectPtr& config)
    : MqttBaseFb(ctx, parent, type, getLocalId(), subscriber, config),
      jsonDataWorker(loggerComponent)
{
    initBaseFunctionalBlocks();
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

MqttJsonReceiverFbImpl::~MqttJsonReceiverFbImpl()
{
    unsubscribeFromTopic();
}

void MqttJsonReceiverFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (propName == PROPERTY_NAME_JSON_CONFIG)
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
                    objPtr.getOnPropertyValueWrite(prop.getName()) += [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args)
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

FunctionBlockTypePtr MqttJsonReceiverFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_JSON_CONFIG, String(""))
                           .setDescription(
                               "JSON configuration string that defines an MQTT topic and corresponding signals to subscribe to.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_TOPIC, String("")).setDescription("An MQTT topic to subscribe to for receiving JSON data.");
        defaultConfig.addProperty(builder.build());
    }
    const auto fbType = FunctionBlockType(JSON_FB_NAME,
                                          JSON_FB_NAME,
                                          "The JSON MQTT function block allows subscribing to MQTT topics, extracting values and "
                                          "timestamps from MQTT JSON messages, and converting them into openDAQ signal data samples.",
                                          defaultConfig);
    return fbType;
}

std::string MqttJsonReceiverFbImpl::getLocalId()
{
    return std::string(MQTT_LOCAL_JSON_FB_ID_PREFIX + std::to_string(localIndex++));
}

void MqttJsonReceiverFbImpl::initBaseFunctionalBlocks()
{
    baseFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for manual JSON configuration
    {
        const auto fbType = MqttJsonDecoderFbImpl::CreateType();
        baseFbTypes.set(fbType.getId(), fbType);
    }
    if (baseFbTypes.getCount() != 0)
    {
        LOG_I("Function block types available:");
    }
    else
    {
        LOG_I("No function block types available");
    }

    for (const auto& [fbName, _] : baseFbTypes)
    {
        LOG_I("\t{}", fbName.toStdString());
    }
}

void MqttJsonReceiverFbImpl::readProperties()
{
    auto lock = this->getRecursiveConfigLock();
    topicForSubscribing.clear();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_TOPIC))
    {
        auto topicStr = objPtr.getPropertyValue(PROPERTY_NAME_TOPIC).asPtrOrNull<IString>();
        if (topicStr.assigned())
        {
            isPresent = true;
            setTopic(topicStr.toStdString());
        }
    }
    if (!isPresent)
    {
        LOG_W("\'{}\' property is missing!", PROPERTY_NAME_TOPIC);
        setComponentStatus(ComponentStatus::Warning);
        setSubscriptionStatus(SubscriptionStatus::InvalidTopicName, "The topic property is not set!");
    }
}

void MqttJsonReceiverFbImpl::readJsonConfig()
{
    if (objPtr.hasProperty(PROPERTY_NAME_JSON_CONFIG))
    {
        const auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_JSON_CONFIG).asPtrOrNull<IString>();
        if (signalConfig.assigned())
        {
            jsonDataWorker.setConfig(signalConfig.toStdString());
            auto result = jsonDataWorker.isJsonValid();
            if (result.success)
            {
                auto topic = jsonDataWorker.extractTopic();
                {
                    auto event = objPtr.getOnPropertyValueWrite(PROPERTY_NAME_TOPIC);
                    event.mute();
                    objPtr.setPropertyValue(PROPERTY_NAME_TOPIC, String(topic));
                    event.unmute();
                }
                setTopic(topic);
                if (const auto signalDscs = jsonDataWorker.extractDescription(); !signalDscs.empty())
                {
                    auto config = MqttJsonDecoderFbImpl::CreateType().createDefaultConfig();
                    for (const auto& [signalName, descriptor] : signalDscs)
                    {
                        LOG_I("Creating a decoder FB for the signal \"{}\":", signalName);
                        config.setPropertyValue(PROPERTY_NAME_VALUE_NAME, descriptor.valueFieldName);
                        config.setPropertyValue(PROPERTY_NAME_TS_NAME, descriptor.tsFieldName);
                        config.setPropertyValue(PROPERTY_NAME_SIGNAL_NAME, signalName);
                        if (descriptor.unit.assigned())
                            config.setPropertyValue(PROPERTY_NAME_UNIT, descriptor.unit.getSymbol());
                        MqttJsonReceiverFbImpl::onAddFunctionBlock(JSON_DECODER_FB_NAME, config);
                    }
                }
            }
        }
    }
}

void MqttJsonReceiverFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto result = unsubscribeFromTopic();
    if (result.success == false)
    {
        LOG_W("Failed to unsubscribe from the previous topic before subscribing to a new one; reason: {}", result.msg);
        return;
    }
    readProperties();
    result = subscribeToTopic();
}

bool MqttJsonReceiverFbImpl::setTopic(std::string topic)
{
    const auto validationStatus = mqtt::MqttDataWrapper::validateTopic(topic, loggerComponent);
    if (validationStatus.success)
    {
        LOG_I("An MQTT topic: {}", topic);
        topicForSubscribing = std::move(topic);
        setComponentStatus(ComponentStatus::Ok);
        setSubscriptionStatus(SubscriptionStatus::WaitingForData, "Subscribing to topic: " + topicForSubscribing);
    }
    else
    {
        setComponentStatus(ComponentStatus::Warning);
        setSubscriptionStatus(SubscriptionStatus::InvalidTopicName, validationStatus.msg);
    }
    return validationStatus.success;
}

DictPtr<IString, IFunctionBlockType> MqttJsonReceiverFbImpl::onGetAvailableFunctionBlockTypes()
{
    return baseFbTypes;
}

FunctionBlockPtr MqttJsonReceiverFbImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{

    FunctionBlockPtr nestedFunctionBlock;
    if (baseFbTypes.hasKey(typeId))
    {
        auto fbTypePtr = baseFbTypes.getOrDefault(typeId);
        if (fbTypePtr.getName() == JSON_DECODER_FB_NAME)
        {
            nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttJsonDecoderFbImpl>(context, functionBlocks, fbTypePtr, config);
        }
    }
    if (nestedFunctionBlock.assigned())
    {
        addNestedFunctionBlock(nestedFunctionBlock);
        {
            auto lock = this->getAcquisitionLock2();
            nestedFunctionBlocks.push_back(nestedFunctionBlock);
        }
        setComponentStatus(ComponentStatus::Ok);
    }
    else
    {
        DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
    }
    return nestedFunctionBlock;
}

void MqttJsonReceiverFbImpl::onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock)
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

void MqttJsonReceiverFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    if (topicForSubscribing == msg.getTopic())
    {
        std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
        auto acqlock = this->getAcquisitionLock2();
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

void MqttJsonReceiverFbImpl::createSignals()
{

}

std::string MqttJsonReceiverFbImpl::getSubscribedTopic() const
{
    return topicForSubscribing;
}

void MqttJsonReceiverFbImpl::clearSubscribedTopic()
{
    topicForSubscribing.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
