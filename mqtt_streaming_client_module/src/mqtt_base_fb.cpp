#include <mqtt_streaming_client_module/mqtt_base_fb.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

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
    unsubscribeFromTopics();
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

void MqttBaseFb::subscribeToTopics()
{
    if (subscriber)
    {
        bool success = true;
        auto lambda = [this](const mqtt::MqttAsyncClient &client, mqtt::MqttMessage &msg){this->onSignalsMessage(client, msg);};
        if (!getSubscribedTopics().empty())
            LOG_I("Trying to subscribe to the topics");
        for (const auto& topic : getSubscribedTopics())
        {
            subscriber->setMessageArrivedCb(topic, lambda);
            auto result = subscriber->subscribe(topic, 1);
            success &= result.success;
            if (!result.success)
            {
                LOG_W("Failed to subscribe to the topic: {}; reason: {}", topic, result.msg);
            }
            else
            {
                // subscriber->subscribe(...) is asynchronous. It puts command in queue and returns immediately.
                LOG_D("Trying to subscribe to the topic: {}", topic);
            }
        }
        if (!success)
            setComponentStatusWithMessage(ComponentStatus::Warning, "Some topics failed to subscribe!");
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "MQTT subscriber client is not set!");
    }
}

void MqttBaseFb::unsubscribeFromTopics()
{
    if (!subscriber)
    {
        LOG_E("The subscriber is null");
        return;
    }
    const auto topics = getSubscribedTopics();
    if (topics.empty())
        return;
    subscriber->setMessageArrivedCb(topics, nullptr);
    auto result = subscriber->unsubscribe(topics);
    if (result.success)
        result = subscriber->waitForCompletion(result.token, MQTT_FB_UNSUBSCRIBE_TOUT);

    if (result.success)
    {
        clearSubscribedTopics();
        LOG_I("All topics have been unsubscribed successfully");
    }
    else
    {
        LOG_W("Failed to unsubscribe from all topics; reason: {}", result.msg);
    }
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
