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

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MqttReceiverFbImpl::MqttReceiverFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const StringPtr& localId,
                                       std::shared_ptr<mqtt::MqttAsyncSubscriber> subscriber,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(CreateType(), ctx, parent, localId)
    , subscriber(subscriber)
{
    initComponentStatus();

    if (config.assigned())
        initProperties(config);
    createSignals();

    for (const auto& topic : subscribedTopics)
    {
        subscriber->setMessageArrivedCb(topic, std::bind(&MqttReceiverFbImpl::onSignalsMessage, this, std::placeholders::_1, std::placeholders::_2));
        subscriber->subscribe(topic, 1);
    }
    setComponentStatus(ComponentStatus::Ok);
}

MqttReceiverFbImpl::~MqttReceiverFbImpl()
{
    for (const auto& topic : subscribedTopics)
    {
        subscriber->setMessageArrivedCb(topic, nullptr);
        subscriber->unsubscribe(topic);
    }
}
void MqttReceiverFbImpl::onSignalsMessage(const mqtt::IMqttSubscriber& subscriber, mqtt::MqttMessage& msg)
{
    parseMessage(msg);
}

void MqttReceiverFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
            }
    }
    readProperties();
}

void MqttReceiverFbImpl::propertyChanged(bool configure)
{
    readProperties();
    if (configure)
        this->configure();
}

void MqttReceiverFbImpl::readProperties()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    auto prop = objPtr.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtr<IList>().toVector();
    for (const auto& item : prop)
    {
        auto signalId = item.asPtr<IString>();
        if (signalId.assigned())
        {
            LOG_I("Signal in list: {}", signalId.toStdString());
            subscribedTopics.push_back(signalId.toStdString());
        }
    }
}

FunctionBlockTypePtr MqttReceiverFbImpl::CreateType()
{
    return FunctionBlockType("MqttFBModule", "MqttSubscriber", "MQTT signal receiving");
}

void MqttReceiverFbImpl::configure()
{

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
    try {
        rapidjson::Document jsonDocument;
        jsonDocument.Parse(jsonObjStr);
        if (jsonDocument.HasParseError()) {
            LOG_E("Error parsing mqtt payload as JSON");
            return;
        }

        if (jsonDocument.IsObject()) {
            double val = 0.0;
            UInt timestamp = 0;
            int successCnt = 0;
            for (auto it = jsonDocument.MemberBegin(); it != jsonDocument.MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                if (name == "value") {
                    if (jsonDocument[name].IsDouble() || jsonDocument[name].IsInt() || jsonDocument[name].IsFloat()){
                        val = jsonDocument[name].GetDouble();
                        successCnt++;
                    } else {
                        LOG_W("Value is not supported.");
                    }
                } else if (name == "timestamp") {
                    if (jsonDocument[name].IsInt() || jsonDocument[name].IsUint64() || jsonDocument[name].IsInt64()){
                        timestamp = jsonDocument[name].GetUint64();
                        successCnt++;
                    } else {
                        LOG_W("Value is not supported.");
                    }
                } else {
                    LOG_W("Field \"{}\" is not supported.", name);
                }
            }
            if (successCnt == 2) {
                createDataPacket(topic, val, timestamp);
            } else {
                LOG_W("Not all required fields are present.");
            }
        }
    }
    catch (DeserializeException ex) {
        LOG_E("Error deserializing mqtt payload");
    }
}

void MqttReceiverFbImpl::createSignals()
{
    auto lock = std::lock_guard<std::mutex>(sync);
    for (const auto& topic : subscribedTopics)
    {
        LOG_I("Subscribing to topic: {}", topic);
        std::string signalName = topic;
        boost::replace_all(signalName, "/", "_");

        auto signalDsc = DataDescriptorBuilder().setSampleType(SampleType::Float64).build();
        auto domainSignalDsc = DataDescriptorBuilder().setSampleType(SampleType::UInt64).build();

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
