#include "mqtt_streaming_client_module/constants.h"
#include <boost/algorithm/string.hpp>
#include <mqtt_streaming_client_module/helper.h>
#include <mqtt_streaming_client_module/mqtt_receiver_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

constexpr int MQTT_JSON_FB_UNSUBSCRIBE_TOUT = 3000;

MqttReceiverFbImpl::MqttReceiverFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const StringPtr& localId,
                                       std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, localId)
    , jsonDataWorker(loggerComponent)
    , subscriber(subscriber)
{
    initComponentStatus();

    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    createSignals();

    if (subscriber)
    {
        for (const auto& topic : getSubscribedTopics())
        {
            subscriber
                ->setMessageArrivedCb(topic,
                                      std::bind(&MqttReceiverFbImpl::onSignalsMessage, this, std::placeholders::_1, std::placeholders::_2));
            auto result = subscriber->subscribe(topic, 1);
            if (!result.success)
                LOG_W("Failed to subscribe to the topic: {}; reason: {}", topic, result.msg);
        }
        setComponentStatus(ComponentStatus::Ok);
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "MQTT subscriber client is not set.");
    }
}

MqttReceiverFbImpl::~MqttReceiverFbImpl()
{
    unsubscribeFromTopics();
}

void MqttReceiverFbImpl::removed()
{
    FunctionBlock::removed();
    unsubscribeFromTopics();
}

void MqttReceiverFbImpl::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, const mqtt::MqttMessage& msg)
{
    parseMessage(msg);
}

void MqttReceiverFbImpl::initProperties(const PropertyObjectPtr& config)
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

void MqttReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    subscribedSignals.clear();
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST)) {
        auto signalConfig = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IString>();
        if (signalConfig.assigned()) {
            jsonDataWorker.setConfig(signalConfig.toStdString());
            subscribedSignals = jsonDataWorker.extractDescription();
            LOG_I("Signal in list:");
            for (const auto& [signalId, descriptor] : subscribedSignals) {
                LOG_I("{} | {}", signalId.topic, signalId.signalName);
            }
        }
    }
}

void MqttReceiverFbImpl::createDataPacket(const std::string& topic, const std::string& json)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    jsonDataWorker.createAndSendDataPacket(topic, json);
}

void MqttReceiverFbImpl::parseMessage(const mqtt::MqttMessage& msg)
{
    std::string topic(msg.getTopic());
    std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
    createDataPacket(topic, jsonObjStr);
}

void MqttReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    for (const auto& [signalId, descriptor] : subscribedSignals)
    {
        LOG_I("Creating signal \"{}\" for topic \"{}\"", signalId.signalName, signalId.topic);
        const std::string& topic = signalId.topic;

        auto signalDsc = descriptor;

        auto refS = outputSignals.emplace(std::make_pair(signalId, createAndAddSignal(buildSignalNameFromTopic(topic, signalId.signalName), signalDsc))).first;
        if (jsonDataWorker.hasDomainSignal(signalId))
        {
            auto getEpoch = []()  ->std::string {
                const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});
                char buf[48];
                strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));
                return { buf };
            };

            const auto domainSignalDsc =
                DataDescriptorBuilder()
                    .setSampleType(SampleType::UInt64)
                    .setUnit(Unit("s", -1, "seconds", "time"))
                    .setTickResolution(Ratio(1, 1'000'000))
                    .setOrigin(getEpoch())
                    .setName("Time").build();
            refS->second->setDomainSignal(createAndAddSignal(buildDomainSignalNameFromTopic(topic, signalId.signalName), domainSignalDsc, false));
        }
    }
    jsonDataWorker.setOutputSignals(&outputSignals);
}

std::vector<std::string> MqttReceiverFbImpl::getSubscribedTopics() const
{
    auto lock = std::lock_guard<std::mutex>(sync);
    std::set<std::string> topicsSet;
    for (const auto& [signalId, _] : subscribedSignals)
    {
        topicsSet.emplace(signalId.topic);
    }
    return std::vector<std::string>(topicsSet.cbegin(), topicsSet.cend());
}

void MqttReceiverFbImpl::unsubscribeFromTopics()
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
        result = subscriber->waitForCompletion(result.token, MQTT_JSON_FB_UNSUBSCRIBE_TOUT);

    if (result.success)
    {
        subscribedSignals.clear();
        LOG_I("All topics have been unsubscribed successfully");
    }
    else
    {
        LOG_W("Failed to unsubscribe from all topics; reason: {}", result.msg);
    }
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
