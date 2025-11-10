#include "mqtt_streaming_client_module/constants.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_client_module/helper.h>
#include <mqtt_streaming_client_module/mqtt_json_receiver_fb_impl.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

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
    subscribeToTopics();
}

MqttJsonReceiverFbImpl::~MqttJsonReceiverFbImpl()
{
    unsubscribeFromTopics();
}


FunctionBlockTypePtr MqttJsonReceiverFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_SIGNAL_LIST, String("")));

    const auto fbType = FunctionBlockType(JSON_FB_NAME,
                                          JSON_FB_NAME,
                                          "",
                                          defaultConfig);
    return fbType;
}

void MqttJsonReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    subscribedSignals.clear();
    signalIdList.clear();
    bool isPresent = false;
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST))
    {
        auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IString>();
        if (signalConfig.assigned())
        {
            isPresent = true;
            jsonDataWorker.setConfig(signalConfig.toStdString());
            auto listSubscribedSignals = jsonDataWorker.extractDescription();
            LOG_I("Signal in the list (topic | signal name):");
            for (const auto& [signalId, descriptor] : listSubscribedSignals)
            {
                subscribedSignals.emplace(signalId, descriptor);
                signalIdList.push_back(signalId);
                LOG_I("\t\"{}\" | \"{}\"", signalId.topic, signalId.signalName);
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

    for (const auto& signalId : signalIdList)
    {
        auto iter = subscribedSignals.find(signalId);
        if (iter == subscribedSignals.end())
        {
            LOG_W("\tSignal \"{}\" on topic \"{}\" is not in the subscribed signal list!", signalId.signalName, signalId.topic);
            continue;
        }
        LOG_D("\tfor the topic \"{}\"", signalId.signalName, signalId.topic);
        const std::string& topic = signalId.topic;

        auto signalDsc = iter->second;

        auto refS =
            outputSignals
                .emplace(std::make_pair(signalId, createAndAddSignal(buildSignalNameFromTopic(topic, signalId.signalName), signalDsc)))
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
                createAndAddSignal(buildDomainSignalNameFromTopic(topic, signalId.signalName), domainSignalDsc, false));
        }
        else
        {
            LOG_D("\tThe signal doesn't have a domain signal");
        }
    }
    jsonDataWorker.setOutputSignals(&outputSignals);
}

std::vector<std::string> MqttJsonReceiverFbImpl::getSubscribedTopics() const
{
    auto lock = std::lock_guard<std::mutex>(sync);
    std::set<std::string> topicsSet;
    for (const auto& [signalId, _] : subscribedSignals)
    {
        topicsSet.emplace(signalId.topic);
    }
    return std::vector<std::string>(topicsSet.cbegin(), topicsSet.cend());
}

void MqttJsonReceiverFbImpl::clearSubscribedTopics()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    subscribedSignals.clear();
    signalIdList.clear();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
