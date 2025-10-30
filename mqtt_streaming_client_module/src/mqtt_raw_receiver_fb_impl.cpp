#include "mqtt_streaming_client_module/constants.h"
#include "mqtt_streaming_client_module/helper.h"
#include "MqttDataWrapper.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_client_module/mqtt_raw_receiver_fb_impl.h>
#include <opendaq/binary_data_packet_factory.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MqttRawReceiverFbImpl::MqttRawReceiverFbImpl(const ContextPtr& ctx,
                                             const ComponentPtr& parent,
                                             const FunctionBlockTypePtr& type,
                                             const StringPtr& localId,
                                             std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                             const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, localId), subscriber(subscriber)
{
    initComponentStatus();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    createSignals();
    subscribeToTopics();

    setComponentStatus(ComponentStatus::Ok);
}

MqttRawReceiverFbImpl::~MqttRawReceiverFbImpl()
{
    unsubscribeFromTopics();
}

void MqttRawReceiverFbImpl::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber,
                                             mqtt::MqttMessage& msg)
{
    createAndSendDataPacket(msg);
}

void MqttRawReceiverFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true);
                internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
            }
        }
        objPtr.setPropertyValue(propName, prop.getValue());
    }
    readProperties();
}

void MqttRawReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    topicsForSubscribing = List<IString>();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST))
    {
        auto prop = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IList>();
        if (prop.assigned())
        {
            isPresent = true;
            for (const auto& topic : prop)
            {
                auto topicStr = topic.asPtr<IString>();
                if (mqtt::MqttDataWrapper::validateTopic(topicStr, loggerComponent))
                {
                    LOG_I("Topic in list: {}", topicStr.toStdString());
                    topicsForSubscribing.pushBack(topicStr);
                }
            }
        }
    }
    if (!isPresent)
    {
        LOG_W("{} property is missing!", PROPERTY_NAME_SIGNAL_LIST);
    }
    if (topicsForSubscribing.empty())
    {
        LOG_W("No topics to subscribe to!");
    }
}

void MqttRawReceiverFbImpl::createAndSendDataPacket(mqtt::MqttMessage& msg)
{
    std::string topic(msg.getTopic());

    auto lock = std::lock_guard<std::mutex>(sync);
    auto signalIter = outputSignals.find(topic);
    if (signalIter == outputSignals.end())
    {
        return;
    }

    const auto& signal = signalIter->second;
    const auto outputPacket =
        BinaryDataPacket(nullptr, signal.getDescriptor(), msg.getData().size());
    memcpy(outputPacket.getData(), msg.getData().data(), msg.getData().size());
    signal.sendPacket(outputPacket);
}

void MqttRawReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    for (const auto& topic : topicsForSubscribing)
    {
        LOG_I("Subscribing to topic: {}", topic);

        const auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Binary).build();
        outputSignals.emplace(
            std::make_pair(topic, createAndAddSignal(buildSignalNameFromTopic(topic, ""), signalDsc)));
    }
}

void MqttRawReceiverFbImpl::subscribeToTopics()
{
    if (!subscriber)
    {
        LOG_E("The subscriber is null");
        return;
    }
    for (const auto& topic : topicsForSubscribing)
    {
        subscriber->setMessageArrivedCb(topic,
                                        std::bind(&MqttRawReceiverFbImpl::onSignalsMessage,
                                                  this,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2));
        auto result = subscriber->subscribe(topic, 1);
        if (!result.success)
            LOG_W("Failed to subscribe to the topic: {}; reason: {}", topic, result.msg);
    }
}

void MqttRawReceiverFbImpl::unsubscribeFromTopics()
{
    if (!subscriber)
    {
        LOG_E("The subscriber is null");
        return;
    }
    for (const auto& topic : topicsForSubscribing)
    {
        subscriber->setMessageArrivedCb(topic, nullptr);
        auto result = subscriber->unsubscribe(topic);
        if (!result.success)
            LOG_W("Failed to unsubscribe from the topic: {}; reason: {}", topic, result.msg);
    }
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
