#include <mqtt_streaming_client_module/mqtt_receiver_fb_impl.h>
#include "opendaq/data_packet_ptr.h"
#include "opendaq/packet_factory.h"
#include <coreobjects/eval_value_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/data_descriptor_ptr.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/reusable_data_packet_ptr.h>
#include <opendaq/signal_factory.h>
#include <rapidjson/document.h>
#include <boost/algorithm/string.hpp>
#include "mqtt_streaming_client_module/constants.h"
#include "MqttDataWrapper.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MqttReceiverFbImpl::MqttReceiverFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const StringPtr& localId,
                                       std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, localId)
    , subscriber(subscriber)
{
    initComponentStatus();

    if (config.assigned())
        initProperties(config);
    else
        initProperties(type.createDefaultConfig());
    createSignals();

    for (const auto& topic : subscribedSignals.getKeys())
    {
        subscriber->setMessageArrivedCb(topic, std::bind(&MqttReceiverFbImpl::onSignalsMessage, this, std::placeholders::_1, std::placeholders::_2));
        auto ok = subscriber->subscribe(topic, 1);
        if (!ok)
            LOG_W("Failed to subscribe to the topic: {}", topic);
    }
    setComponentStatus(ComponentStatus::Ok);
}

MqttReceiverFbImpl::~MqttReceiverFbImpl()
{
    for (const auto& topic : subscribedSignals.getKeys())
    {
        subscriber->setMessageArrivedCb(topic, nullptr);
        subscriber->unsubscribe(topic);
    }
}
void MqttReceiverFbImpl::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg)
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
    subscribedSignals = Dict<IString, IString>();
    if (objPtr.hasProperty(PROPERTY_NAME_SIGNAL_LIST)) {
        auto prop = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtrOrNull<IDict>();
        if (prop.assigned()) {
            for (const auto& [topic, descriptor] : prop) {
                auto signalId = topic.asPtr<IString>();
                if (signalId.assigned()) {
                    LOG_I("Signal in list: {}", signalId.toStdString());
                    subscribedSignals.set(signalId, descriptor);
                }
            }
        }
    }
}

void MqttReceiverFbImpl::createDataPacket(const std::string& topic, double value, UInt timestamp)
{
    auto lock = std::lock_guard<std::mutex>(sync);
    auto signalIter = outputSignals.find(topic);
    auto dSignalIter = outputDomainSignals.find(topic);
    if (signalIter == outputSignals.end() || dSignalIter == outputDomainSignals.end()) {
        return;
    }
    auto signal = signalIter->second;
    auto dSignal = dSignalIter->second;

    DataPacketPtr outputDomainPacket = DataPacket(signal.getDomainSignal().getDescriptor(), 1);
    std::memcpy(outputDomainPacket.getRawData(), &timestamp, sizeof(timestamp));
    DataPacketPtr outputPacket = DataPacketWithDomain(outputDomainPacket, signal.getDescriptor(), 1);

    auto outputData = reinterpret_cast<Float*>(outputPacket.getRawData());
    *outputData = value;
    signal.sendPacket(outputPacket);
    dSignal.sendPacket(outputDomainPacket);
}

void MqttReceiverFbImpl::parseMessage(mqtt::MqttMessage& msg)
{
    std::string topic(msg.getTopic());
    std::string jsonObjStr(msg.getData().begin(), msg.getData().end());
    auto [status, data] = mqtt::MqttDataWrapper::parseSampleData(jsonObjStr);
    if (status.ok) {
        createDataPacket(topic, data.value, data.timestamp);
    } else {
        for (const auto& s : status.msg) {
            LOG_W("Data parsing: {}", s);
        }
    }
}

void MqttReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    for (const auto& [topic, descriptor] : subscribedSignals)
    {
        LOG_I("Subscribing to topic: {}", topic);
        std::string signalName = topic;
        boost::replace_all(signalName, "/", "_");

        auto signalDsc = JsonDeserializer().deserialize(descriptor).asPtrOrNull<IDataDescriptor>();

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

        auto refS = outputSignals.emplace(std::make_pair(topic, createAndAddSignal(buildSignalNameFromTopic(topic), signalDsc))).first;
        auto refSD = outputDomainSignals.emplace(std::make_pair(topic, createAndAddSignal(buildDomainSignalNameFromTopic(topic), domainSignalDsc, false))).first;
        refS->second->setDomainSignal(refSD->second);
    }
}

std::string MqttReceiverFbImpl::buildSignalNameFromTopic(std::string topic) const
{
    boost::replace_all(topic, "/", "_");
    topic += "_Mqtt";
    return topic;
}

std::string MqttReceiverFbImpl::buildDomainSignalNameFromTopic(std::string topic) const
{
    boost::replace_all(topic, "/", "_");
    topic += std::string("_Mqtt") + "_domain";
    return topic;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
