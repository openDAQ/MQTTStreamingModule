#include "MqttDataWrapper.h"
#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/helper.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/mqtt_raw_receiver_fb_impl.h>
#include <opendaq/binary_data_packet_factory.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_RAW_FB_UNSUBSCRIBE_TOUT = 3000;

std::atomic<int> MqttRawReceiverFbImpl::localIndex = 0;

MqttRawReceiverFbImpl::MqttRawReceiverFbImpl(const ContextPtr& ctx,
                                             const ComponentPtr& parent,
                                             const FunctionBlockTypePtr& type,
                                             std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                             const PropertyObjectPtr& config)
    : MqttBaseFb(ctx, parent, type, generateLocalId(), subscriber, config)
{
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    createSignals();
    subscribeToTopic();
}

MqttRawReceiverFbImpl::~MqttRawReceiverFbImpl()
{
    unsubscribeFromTopic();
}

FunctionBlockTypePtr MqttRawReceiverFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
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
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_TOPIC, "").setDescription("An MQTT topic to subscribe to for receiving raw binary data.");
        defaultConfig.addProperty(builder.build());
    }
    const auto fbType =
        FunctionBlockType(RAW_FB_NAME,
                          RAW_FB_NAME,
                          "The raw MQTT function block allows subscribing to an MQTT topic and converting MQTT payloads into "
                          "openDAQ signal binary data samples.",
                          defaultConfig);
    return fbType;
}

std::string MqttRawReceiverFbImpl::generateLocalId()
{
    return std::string(MQTT_LOCAL_RAW_FB_ID_PREFIX + std::to_string(localIndex++));
}

void MqttRawReceiverFbImpl::readProperties()
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
                subscriptionStatus.setStatus(SubscriptionStatus::WaitingForData, "Subscribing to topic: " + topicForSubscribing);
            }
            else
            {
                setComponentStatus(ComponentStatus::Warning);
                subscriptionStatus.setStatus(SubscriptionStatus::InvalidTopicName, validationStatus.msg);
            }
        }
    }

    if (objPtr.hasProperty(PROPERTY_NAME_SUB_QOS))
    {
        auto qosProp = objPtr.getPropertyValue(PROPERTY_NAME_SUB_QOS).asPtrOrNull<IInteger>();
        if (qosProp.assigned())
        {
            const auto qos = qosProp.getValue(DEFAULT_SUB_QOS);
            this->qos = (qos < 0 || qos > 2) ? DEFAULT_SUB_QOS : qos;
        }
    }

    if (!isPresent)
    {
        LOG_W("\'{}\' property is missing!", PROPERTY_NAME_TOPIC);
        setComponentStatus(ComponentStatus::Warning);
        subscriptionStatus.setStatus(SubscriptionStatus::InvalidTopicName, "The topic property is not set!");
    }
    if (topicForSubscribing.empty())
    {
        LOG_W("No topic to subscribe to!");
    }
}

void MqttRawReceiverFbImpl::propertyChanged()
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

void MqttRawReceiverFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    const std::string topic(msg.getTopic());

    auto lock = std::lock_guard<std::mutex>(sync);
    if (topicForSubscribing == topic)
    {
        if (subscriptionStatus.getStatus() == SubscriptionStatus::WaitingForData)
        {
            subscriptionStatus.setStatus(SubscriptionStatus::HasData);
        }
        const auto outputPacket = BinaryDataPacket(nullptr, outputSignal.getDescriptor(), msg.getData().size());
        memcpy(outputPacket.getData(), msg.getData().data(), msg.getData().size());
        outputSignal.sendPacket(outputPacket);
    }
}

void MqttRawReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    const auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Binary).build();
    outputSignal = createAndAddSignal(DEFAULT_VALUE_SIGNAL_LOCAL_ID, signalDsc);
}

std::string MqttRawReceiverFbImpl::getSubscribedTopic() const
{
    return topicForSubscribing;
}

void MqttRawReceiverFbImpl::clearSubscribedTopic()
{
    topicForSubscribing.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
