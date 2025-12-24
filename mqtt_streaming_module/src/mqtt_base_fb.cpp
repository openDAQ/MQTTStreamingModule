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
                objPtr.setPropertyValue(propName, prop.getValue());
                objPtr.getOnPropertyValueWrite(prop.getName()) +=
                    [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { propertyChanged(); };
            }
        }
        else
        {
            objPtr.setPropertyValue(propName, prop.getValue());
        }
    }
    readProperties();
}

MqttBaseFb::CmdResult MqttBaseFb::subscribeToTopic()
{
    MqttBaseFb::CmdResult result{false};
    if (subscriber)
    {
        auto lambda = [this](const mqtt::MqttAsyncClient &client, mqtt::MqttMessage &msg){this->onSignalsMessage(client, msg);};
        const auto topic = getSubscribedTopic();
        if (!topic.empty())
        {
            LOG_I("Trying to subscribe to the topic : {}", topic);
            subscriber->setMessageArrivedCb(topic, lambda);
            if (auto subRes = subscriber->subscribe(topic, 1); subRes.success == false)
            {
                LOG_W("Failed to subscribe to the topic: \"{}\"; reason: {}", topic, subRes.msg);
                setComponentStatusWithMessage(ComponentStatus::Warning, "Some topics failed to subscribe!");
                setSubscriptionStatus(SubscriptionStatus::SubscribingError, "The reason: " + subRes.msg);
                result = {false, "Failed to subscribe to the topic: \"" + topic + "\"; reason: " + subRes.msg};
            }
            else
            {
                // subscriber->subscribe(...) is asynchronous. It puts command in queue and returns immediately.
                LOG_D("Trying to subscribe to the topic: {}", topic);
                setComponentStatus(ComponentStatus::Ok);
                setSubscriptionStatus(SubscriptionStatus::WaitingForData, "Topic: " + topic);
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
        setSubscriptionStatus(SubscriptionStatus::SubscribingError, msg);
        result = {false, msg};
    }
    return result;
}

MqttBaseFb::CmdResult MqttBaseFb::unsubscribeFromTopic()
{
    MqttBaseFb::CmdResult result{true};
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
                setComponentStatus(ComponentStatus::Warning);
                setSubscriptionStatus(SubscriptionStatus::SubscribingError, msg);
                result = {false, msg};
            }
        }
    }
    else
    {
        const std::string msg = "MQTT subscriber client is not set!";
        setComponentStatusWithMessage(ComponentStatus::Error, msg);
        setSubscriptionStatus(SubscriptionStatus::SubscribingError, msg);
        result = {false, msg};
    }
    return result;
}

void MqttBaseFb::initSubscriptionStatus()
{
    if (!context.getTypeManager().hasType(MQTT_FB_SUB_STATUS_TYPE))
    {
        auto list = List<IString>();
        for (const auto& [_, st] : subscriptionStatusMap)
            list.pushBack(st);

        context.getTypeManager().addType(EnumerationType(MQTT_FB_SUB_STATUS_TYPE, list));
    }

    subscriptionStatus = EnumerationWithIntValue(MQTT_FB_SUB_STATUS_TYPE,
                                                 static_cast<Int>(SubscriptionStatus::InvalidTopicName),
                                                 this->context.getTypeManager());
    statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).addStatus(MQTT_FB_SUB_STATUS_NAME,
                                                                                     subscriptionStatus);
}

void MqttBaseFb::setSubscriptionStatus(const SubscriptionStatus status, std::string message)
{
    subscriptionStatus = EnumerationWithIntValue(MQTT_FB_SUB_STATUS_TYPE, static_cast<Int>(status), this->context.getTypeManager());
    statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).setStatusWithMessage(MQTT_FB_SUB_STATUS_NAME,
                                                                                                subscriptionStatus,
                                                                                                message);
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
