#include "mqtt_streaming_module/constants.h"
#include <mqtt_streaming_module/mqtt_base_fb.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_FB_UNSUBSCRIBE_TOUT = 3000;

std::vector<std::pair<MqttBaseFb::SubscriptionStatus, std::string>> MqttBaseFb::subscriptionStatusMap =
    {{SubscriptionStatus::InvalidTopicName, "InvalidTopicName"},
     {SubscriptionStatus::SubscribingError, "SubscribingError"},
     {SubscriptionStatus::WaitingForData, "WaitingForData"},
     {SubscriptionStatus::HasData, "HasData"}};

MqttBaseFb::MqttBaseFb(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const StringPtr& localId,
                                       std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, localId)
    , subscriber(subscriber)
{
    initComponentStatus();
    initSubscriptionStatus();
}

void MqttBaseFb::removed()
{
    FunctionBlock::removed();
    unsubscribeFromTopic();
    setSubscriptionStatus(SubscriptionStatus::InvalidTopicName, "Function block removed");
    setComponentStatusWithMessage(ComponentStatus::Error, "Function block removed");
}

void MqttBaseFb::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, const mqtt::MqttMessage& msg)
{
    processMessage(msg);
}

void MqttBaseFb::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
            }
        }
        objPtr.setPropertyValue(propName, prop.getValue());
    }
    readProperties();
}

void MqttBaseFb::subscribeToTopic()
{
    if (subscriber)
    {
        auto lambda = [this](const mqtt::MqttAsyncClient &client, mqtt::MqttMessage &msg){this->onSignalsMessage(client, msg);};
        const auto topic = getSubscribedTopic();
        if (!topic.empty())
        {
            LOG_I("Trying to subscribe to the topic : {}", topic);
            subscriber->setMessageArrivedCb(topic, lambda);
            auto result = subscriber->subscribe(topic, 1);
            if (!result.success)
            {
                LOG_W("Failed to subscribe to the topic: {}; reason: {}", topic, result.msg);
                setComponentStatusWithMessage(ComponentStatus::Warning, "Some topics failed to subscribe!");
                setSubscriptionStatus(SubscriptionStatus::SubscribingError, "The reason: " + result.msg);
            }
            else
            {
                // subscriber->subscribe(...) is asynchronous. It puts command in queue and returns immediately.
                LOG_D("Trying to subscribe to the topic: {}", topic);
                setComponentStatus(ComponentStatus::Ok);
                setSubscriptionStatus(SubscriptionStatus::WaitingForData, "Topic: " + topic);
            }
        }
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "MQTT subscriber client is not set!");
        setSubscriptionStatus(SubscriptionStatus::SubscribingError, "MQTT subscriber client is not set!" );
    }
}

void MqttBaseFb::unsubscribeFromTopic()
{
    if (!subscriber)
    {
        LOG_E("The subscriber is null");
        return;
    }
    const auto topic = getSubscribedTopic();
    if (topic.empty())
        return;
    subscriber->setMessageArrivedCb(topic, nullptr);
    auto result = subscriber->unsubscribe(topic);
    if (result.success)
        result = subscriber->waitForCompletion(result.token, MQTT_FB_UNSUBSCRIBE_TOUT);

    if (result.success)
    {
        clearSubscribedTopic();
        LOG_I("The topic \'{}\' has been unsubscribed successfully", topic);
    }
    else
    {
        const auto msg = fmt::format("Failed to unsubscribe from the topic \'{}\'; reason: {}", topic, result.msg);
        LOG_W("{}", msg);
        setComponentStatus(ComponentStatus::Warning);
        setSubscriptionStatus(SubscriptionStatus::SubscribingError, msg);
    }
}

void MqttBaseFb::initSubscriptionStatus()
{
    if (!context.getTypeManager().hasType(MQTT_RAW_FB_SUB_STATUS_TYPE))
    {
        auto list = List<IString>();
        for (const auto& [_, st] : subscriptionStatusMap)
            list.pushBack(st);

        context.getTypeManager().addType(EnumerationType(MQTT_RAW_FB_SUB_STATUS_TYPE, list));
    }

    subscriptionStatus = EnumerationWithIntValue(MQTT_RAW_FB_SUB_STATUS_TYPE,
                                                 static_cast<Int>(SubscriptionStatus::InvalidTopicName),
                                                 this->context.getTypeManager());
    statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).addStatus("SubscriptionStatus",
                                                                                     subscriptionStatus);
}

void MqttBaseFb::setSubscriptionStatus(const SubscriptionStatus status, std::string message)
{
    subscriptionStatus = EnumerationWithIntValue(MQTT_RAW_FB_SUB_STATUS_TYPE, static_cast<Int>(status), this->context.getTypeManager());
    statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).setStatusWithMessage("SubscriptionStatus",
                                                                                                subscriptionStatus,
                                                                                                message);
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
