#pragma once

#include <cstdint>
#include <rapidjson/document.h>
#include <string>
#include <vector>

#include <coretypes/list_ptr.h>
#include <opendaq/packet_factory.h>
#include <opendaq/signal.h>
#include <opendaq/signal_config_ptr.h>
#include <opendaq/signal_ptr.h>

namespace mqtt
{

struct SampleData
{
    double value;
    uint64_t timestamp;
};

struct SignalId
{
    std::string topic;
    std::string signalName;

    bool operator==(const SignalId& other) const noexcept
    {
        return topic == other.topic && signalName == other.signalName;
    }
};

struct DataPackets
{
    daq::DataPacketPtr dataPacket;
    daq::DataPacketPtr domainDataPacket;
};

struct MqttMsgDescriptor
{
    std::string signalName;
    std::string valueFieldName; // Value
    std::string tsFieldName;    // Timestamp
};

class MqttDataWrapper final
{
public:
    MqttDataWrapper(daq::LoggerComponentPtr loggerComponent);

    static std::string extractDeviceName(const std::string& topic);

    static std::string serializeSampleData(const SampleData& data);
    static std::string serializeSignalDescriptors(
        daq::ListObjectPtr<daq::IList, daq::ISignal, daq::GenericSignalPtr<daq::ISignal>> signals);

    static std::string buildTopicFromId(const std::string& globalId);
    static std::string buildSignalsTopic(const std::string& deviceId);
    static bool validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent = nullptr);

    void setConfig(const std::string& config);
    std::unordered_map<mqtt::SignalId, daq::DataDescriptorPtr> extractDescription();
    void setOutputSignals(std::unordered_map<SignalId, daq::SignalConfigPtr>* const outputSignals);
    void createAndSendDataPacket(const std::string& topic, const std::string& json);
    bool hasDomainSignal(const SignalId& signalId) const;

private:
    std::string config;

    daq::LoggerComponentPtr loggerComponent;
    // {topic, signalName} : daq::signal
    std::unordered_map<SignalId, daq::SignalConfigPtr>* outputSignals = nullptr;
    // topic : MqttMsgDescriptor; used for description how to extract data from sample json\
    // each topic message can contain multiple signals
    std::unordered_map<std::string, std::vector<MqttMsgDescriptor>> topicDescriptors;

    std::vector<std::pair<SignalId, DataPackets>> extractDataSamples(
        const std::string& topic, const MqttMsgDescriptor& msgDescriptor, const std::string& json);
    void sendDataSamples(const SignalId& signalId, const DataPackets& dataPackets);
    template <typename T>
    DataPackets buildDataPackets(const SignalId& signalId, T value, uint64_t timestamp);
    template <typename T>
    DataPackets buildDataPackets(const SignalId& signalId, T value);
    daq::DataPacketPtr buildDomainDataPacket(daq::GenericSignalConfigPtr<> signalConfig, uint64_t timestamp);
    template<typename T>
    daq::DataPacketPtr buildDataPacket(daq::GenericSignalConfigPtr<> signalConfig,
                                       T value,
                                       const daq::DataPacketPtr domainPacket);
    template<typename T>
    daq::DataPacketPtr createEmptyDataPacket(const daq::GenericSignalConfigPtr<> signalConfig,
                                       const daq::DataPacketPtr domainPacket, T value);
    template <typename T> void copyDataIntoPacket(daq::DataPacketPtr dataPacket, T value);
    daq::UnitPtr extractSignalUnit(const rapidjson::Value& signalObj);
    std::string extractValueFieldName(const rapidjson::Value& signalObj);
    std::string extractTimestampFieldName(const rapidjson::Value& signalObj);
    std::string extractFieldName(const rapidjson::Value& signalObj, const std::string& field);

    template <typename TReadType>
    static bool isTypeTheSame(daq::SampleType sampleType);
};
} // namespace mqtt

namespace std
{

template <> struct hash<mqtt::SignalId>
{
    std::size_t operator()(const mqtt::SignalId& id) const noexcept
    {
        std::size_t h1 = std::hash<std::string>{}(id.topic);
        std::size_t h2 = std::hash<std::string>{}(id.signalName);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std
