#include "mqtt_streaming_module/constants.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_json_receiver_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_JSON_FB_UNSUBSCRIBE_TOUT = 3000;

MqttJsonReceiverFbImpl::MqttJsonReceiverFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const StringPtr& localId,
                                       std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                       const PropertyObjectPtr& config)
    : MqttBaseFb(ctx, parent, type, localId, subscriber, config),
      jsonDataWorker(loggerComponent)
{
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

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
    auto builder =
        StringPropertyBuilder(PROPERTY_NAME_SIGNAL_LIST, String(""))
            .setDescription("JSON configuration string that defines the list of MQTT topics and corresponding signals to subscribe to.");
    defaultConfig.addProperty(builder.build());

    const auto fbType = FunctionBlockType(JSON_FB_NAME,
                                          JSON_FB_NAME,
                                          "The JSON MQTT function block allows subscribing to MQTT topics, extracting values and "
                                          "timestamps from MQTT JSON messages, and converting them into openDAQ signal data samples.",
                                          defaultConfig);
    return fbType;
}

void MqttJsonReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    subscribedSignals.clear();
    signalNameList.clear();
    topicForSubscribing.clear();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST))
    {
        const auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IString>();
        if (signalConfig.assigned())
        {
            isPresent = true;
            jsonDataWorker.setConfig(signalConfig.toStdString());
            const auto listSubscribedSignals = jsonDataWorker.extractDescription();
            if (!listSubscribedSignals.empty())
            {
                bool isOneTopic =
                    std::all_of(listSubscribedSignals.cbegin(),
                                listSubscribedSignals.cend(),
                                [&listSubscribedSignals](const auto& s) { return s.first.topic == listSubscribedSignals.front().first.topic; });
                if (!isOneTopic)
                {
                    LOG_E("The JSON config has wrong format (more then one topic found)");
                }
                else
                {
                    topicForSubscribing = listSubscribedSignals.front().first.topic;
                    LOG_I("Signal in the list for the topic \"{}\":", topicForSubscribing);
                    for (const auto& [signalId, descriptor] : listSubscribedSignals)
                    {
                        subscribedSignals.emplace(signalId.signalName, descriptor);
                        signalNameList.push_back(signalId.signalName);
                        LOG_I("\t\"{}\"", signalId.signalName);
                    }
                }
            }
        }
    }
    if (!isPresent)
    {
        LOG_W("{} property is missing!", PROPERTY_NAME_SIGNAL_LIST);
    }
    if (subscribedSignals.empty())
    {
        LOG_W("No signals in the list!");
    }
}

void MqttJsonReceiverFbImpl::propertyChanged()
{
}

void MqttJsonReceiverFbImpl::createDataPacket(const std::string& topic, const std::string& json)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    jsonDataWorker.createAndSendDataPacket(topic, json);
}

void MqttJsonReceiverFbImpl::processMessage(const mqtt::MqttMessage& msg)
{
    std::string topic(msg.getTopic());
    std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
    createDataPacket(topic, jsonObjStr);
}

void MqttJsonReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    if (!subscribedSignals.empty())
        LOG_I("Creating signals...");

    for (const auto& signalName : signalNameList)
    {
        auto iter = subscribedSignals.find(signalName);
        if (iter == subscribedSignals.end())
        {
            LOG_W("\tSignal \"{}\" is not in the subscribed signal list!", signalName);
            continue;
        }
        LOG_D("\tfor the signal \"{}\"", signalName);

        auto signalDsc = iter->second;
        const mqtt::SignalId signalId{topicForSubscribing, signalName};
        auto refS = outputSignals
                        .emplace(std::make_pair(signalId,
                                                createAndAddSignal(buildSignalNameFromTopic(topicForSubscribing, signalName), signalDsc)))
                        .first;
        if (jsonDataWorker.hasDomainSignal(signalId))
        {
            LOG_D("\tThe signal has a domain signal");
            auto getEpoch = []() -> std::string
            {
                const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});
                char buf[48];
                strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));
                return {buf};
            };

            const auto domainSignalDsc = DataDescriptorBuilder()
                                             .setSampleType(SampleType::UInt64)
                                             .setUnit(Unit("s", -1, "seconds", "time"))
                                             .setTickResolution(Ratio(1, 1'000'000))
                                             .setOrigin(getEpoch())
                                             .setName("Time")
                                             .build();
            refS->second->setDomainSignal(
                createAndAddSignal(buildDomainSignalNameFromTopic(topicForSubscribing, signalName), domainSignalDsc, false));
        }
        else
        {
            LOG_D("\tThe signal doesn't have a domain signal");
        }
    }
    jsonDataWorker.setOutputSignals(&outputSignals);
}

std::string MqttJsonReceiverFbImpl::getSubscribedTopic() const
{
    auto lock = std::lock_guard<std::mutex>(sync);
    return topicForSubscribing;
}

void MqttJsonReceiverFbImpl::clearSubscribedTopic()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    subscribedSignals.clear();
    signalNameList.clear();
    topicForSubscribing.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
