#include <mqtt_streaming_module/mqtt_base_fb.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_FB_UNSUBSCRIBE_TOUT = 3000;

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
}


void MqttBaseFb::removed()
{
    FunctionBlock::removed();
    unsubscribeFromTopic();
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
            }
            else
            {
                // subscriber->subscribe(...) is asynchronous. It puts command in queue and returns immediately.
                LOG_D("Trying to subscribe to the topic: {}", topic);
            }
        }
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "MQTT subscriber client is not set!");
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
        LOG_W("Failed to unsubscribe from the topic \'{}\'; reason: {}", topic, result.msg);
    }
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
