#include "MqttDataWrapper.h"
#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/helper.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/mqtt_raw_receiver_fb_impl.h>
#include <opendaq/binary_data_packet_factory.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_RAW_FB_UNSUBSCRIBE_TOUT = 3000;

MqttRawReceiverFbImpl::MqttRawReceiverFbImpl(const ContextPtr& ctx,
                                             const ComponentPtr& parent,
                                             const FunctionBlockTypePtr& type,
                                             const StringPtr& localId,
                                             std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                             const PropertyObjectPtr& config)
    : MqttBaseFb(ctx, parent, type, localId, subscriber, config)
{
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    createSignals();
    subscribeToTopics();
}

MqttRawReceiverFbImpl::~MqttRawReceiverFbImpl()
{
    unsubscribeFromTopics();
}

FunctionBlockTypePtr MqttRawReceiverFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    auto builder = ListPropertyBuilder(PROPERTY_NAME_SIGNAL_LIST, List<IString>())
                       .setDescription("List of MQTT topics to subscribe to for receiving raw binary data.");
    defaultConfig.addProperty(builder.build());
    const auto fbType = FunctionBlockType(RAW_FB_NAME,
                                          RAW_FB_NAME,
                                          "The raw MQTT function block allows subscribing to MQTT topics and converting MQTT payloads into "
                                          "openDAQ signal binary data samples.",
                                          defaultConfig);
    return fbType;
}

void MqttRawReceiverFbImpl::readProperties()
{
    auto lock = std::scoped_lock(sync);
    topicsForSubscribing.clear();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST))
    {
        auto prop = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IList>();
        if (prop.assigned())
        {
            isPresent = true;
            if (prop.getCount() != 0)
            {
                LOG_I("Topics in the list:");
            }
            for (const auto& topic : prop)
            {
                auto topicStr = topic.asPtr<IString>();
                if (mqtt::MqttDataWrapper::validateTopic(topicStr, loggerComponent))
                {
                    LOG_I("\t{}", topicStr.toStdString());
                    topicsForSubscribing.emplace_back(topicStr.toStdString());
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

void MqttRawReceiverFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    std::string topic(msg.getTopic());

    auto lock = std::scoped_lock(sync);
    auto signalIter = outputSignals.find(topic);
    if (signalIter == outputSignals.end())
    {
        return;
    }

    const auto& signal = signalIter->second;
    const auto outputPacket = BinaryDataPacket(nullptr, signal.getDescriptor(), msg.getData().size());
    memcpy(outputPacket.getData(), msg.getData().data(), msg.getData().size());
    signal.sendPacket(outputPacket);
}

void MqttRawReceiverFbImpl::createSignals()
{
    auto lock = std::scoped_lock(sync);
    if (!topicsForSubscribing.empty())
    {
        LOG_I("Creating signals...");
    }
    for (const auto& topic : topicsForSubscribing)
    {
        LOG_D("\tfor the topic: {}", topic);

        const auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Binary).build();
        outputSignals.emplace(std::make_pair(topic, createAndAddSignal(buildSignalNameFromTopic(topic, ""), signalDsc)));
    }
}

std::vector<std::string> MqttRawReceiverFbImpl::getSubscribedTopics() const
{
    return topicsForSubscribing;
}

void MqttRawReceiverFbImpl::clearSubscribedTopics()
{
    topicsForSubscribing.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
