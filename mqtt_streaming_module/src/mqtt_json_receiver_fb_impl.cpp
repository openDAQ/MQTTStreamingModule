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
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    initBaseFunctionalBlocks();
    createSignals();
    subscribeToTopic();
}

MqttJsonReceiverFbImpl::~MqttJsonReceiverFbImpl()
{
    unsubscribeFromTopic();
}

FunctionBlockTypePtr MqttJsonReceiverFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    // {
    //     auto builder = StringPropertyBuilder(PROPERTY_NAME_SIGNAL_LIST, String(""))
    //                        .setDescription(
    //                            "JSON configuration string that defines the list of MQTT topics and corresponding signals to subscribe to.");
    //     defaultConfig.addProperty(builder.build());
    // }
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

// void MqttJsonReceiverFbImpl::readProperties()
// {
//     auto lock = std::lock_guard<std::mutex>(sync);
//     subscribedSignals.clear();
//     signalNameList.clear();
//     topicForSubscribing.clear();
//     bool isPresent = false;
//     if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST))
//     {
//         const auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IString>();
//         if (signalConfig.assigned())
//         {
//             isPresent = true;
//             jsonDataWorker.setConfig(signalConfig.toStdString());
//             const auto listSubscribedSignals = jsonDataWorker.extractDescription();
//             if (!listSubscribedSignals.empty())
//             {
//                 bool isOneTopic =
//                     std::all_of(listSubscribedSignals.cbegin(),
//                                 listSubscribedSignals.cend(),
//                                 [&listSubscribedSignals](const auto& s) { return s.first.topic == listSubscribedSignals.front().first.topic; });
//                 if (!isOneTopic)
//                 {
//                     LOG_E("The JSON config has wrong format (more then one topic found)");
//                 }
//                 else
//                 {
//                     topicForSubscribing = listSubscribedSignals.front().first.topic;
//                     LOG_I("Signal in the list for the topic \"{}\":", topicForSubscribing);
//                     for (const auto& [signalId, descriptor] : listSubscribedSignals)
//                     {
//                         subscribedSignals.emplace(signalId.signalName, descriptor);
//                         signalNameList.push_back(signalId.signalName);
//                         LOG_I("\t\"{}\"", signalId.signalName);
//                     }
//                 }
//             }
//         }
//     }
//     if (!isPresent)
//     {
//         LOG_W("{} property is missing!", PROPERTY_NAME_SIGNAL_LIST);
//     }
//     if (subscribedSignals.empty())
//     {
//         LOG_W("No signals in the list!");
//     }
// }

void MqttJsonReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    topicForSubscribing.clear();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_TOPIC))
    {
        auto topicStr = objPtr.getPropertyValue(PROPERTY_NAME_TOPIC).asPtrOrNull<IString>();
        if (topicStr.assigned())
        {
            isPresent = true;
            const auto validationStatus = mqtt::MqttDataWrapper::validateTopic(topicStr, loggerComponent);
            if (validationStatus.success)
            {
                LOG_I("An MQTT topic: {}", topicStr.toStdString());
                topicForSubscribing = topicStr.toStdString();
                setComponentStatus(ComponentStatus::Ok);
                setSubscriptionStatus(SubscriptionStatus::WaitingForData, "Subscribing to topic: " + topicForSubscribing);
            }
            else
            {
                setComponentStatus(ComponentStatus::Warning);
                setSubscriptionStatus(SubscriptionStatus::InvalidTopicName, validationStatus.msg);
            }
        }
    }
    if (!isPresent)
    {
        LOG_W("\'{}\' property is missing!", PROPERTY_NAME_TOPIC);
        setComponentStatus(ComponentStatus::Warning);
        setSubscriptionStatus(SubscriptionStatus::InvalidTopicName, "The topic property is not set!");
    }
    if (topicForSubscribing.empty())
    {
        LOG_W("No topic to subscribe to!");
    }
}

void MqttJsonReceiverFbImpl::propertyChanged()
{
    auto result = unsubscribeFromTopic();
    if (result.success == false)
    {
        LOG_W("Failed to unsubscribe from the previous topic before subscribing to a new one; reason: {}", result.msg);
        return;
    }
    readProperties();
    result = subscribeToTopic();
}

DictPtr<IString, IFunctionBlockType> MqttJsonReceiverFbImpl::onGetAvailableFunctionBlockTypes()
{
    return baseFbTypes;
}

FunctionBlockPtr MqttJsonReceiverFbImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    FunctionBlockPtr nestedFunctionBlock;
    {
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
            setComponentStatus(ComponentStatus::Ok);
        }
        else
        {
            DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
        }
    }
    nestedFunctionBlocks.push_back(nestedFunctionBlock);
    return nestedFunctionBlock;
}

void MqttJsonReceiverFbImpl::onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    auto it = std::find_if(nestedFunctionBlocks.begin(),
                           nestedFunctionBlocks.end(),
                           [&functionBlock](const FunctionBlockPtr& fb) { return fb.getObject() == functionBlock.getObject(); });

    if (it != nestedFunctionBlocks.end())
    {
        nestedFunctionBlocks.erase(it);
    }
    FunctionBlockImpl::onRemoveFunctionBlock(functionBlock);
}

void MqttJsonReceiverFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    if (topicForSubscribing == msg.getTopic())
    {
        std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
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
    auto lock = std::lock_guard<std::mutex>(sync);
    return topicForSubscribing;
}

void MqttJsonReceiverFbImpl::clearSubscribedTopic()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    topicForSubscribing.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
