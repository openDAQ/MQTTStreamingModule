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

template<typename T>
struct container_traits
{
    static constexpr bool is_vector = false;
    using value_type = T;
};

template<typename U, typename Alloc>
struct container_traits<std::vector<U, Alloc>>
{
    static constexpr bool is_vector = true;
    using value_type = U;
};

template<typename T>
inline constexpr bool is_std_vector_v = container_traits<T>::is_vector;

template<typename T>
using sample_type_t = typename container_traits<T>::value_type;

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
    std::string valueFieldName; // Value
    std::string tsFieldName;    // Timestamp
    daq::UnitPtr unit;
};

class MqttDataWrapper final
{
public:
    struct CmdResult
    {
        bool success = false;
        std::string msg;

        CmdResult()
            : success(false),
              msg("")
        {
        }
        CmdResult(bool success, const std::string& msg = "")
            : success(success),
              msg(msg)
        {
        }

        CmdResult addError(const std::string& newmsg)
        {
            success = false;
            msg += newmsg;
            return *this;
        }
        CmdResult merge(const CmdResult& other)
        {
            success = success && other.success;
            msg += other.msg;
            return *this;
        }
    };

    enum class DomainSignalMode : int
    {
        None = 0,
        ExtractFromMessage,
        ExternalTimestamp,
        _count
    };

    MqttDataWrapper(daq::LoggerComponentPtr loggerComponent);

    static CmdResult validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent = nullptr);

    void setConfig(const std::string& config);
    std::vector<std::pair<std::string, MqttMsgDescriptor>> extractDescription();
    std::string extractTopic();
    CmdResult isJsonValid();
    void setOutputSignal(daq::SignalConfigPtr outputSignal);
    CmdResult createAndSendDataPacket(const std::string& json, const uint64_t externalTs);
    //bool hasDomainSignal(const SignalId& signalId) const;
    void setValueFieldName(std::string valueFieldName);
    void setTimestampFieldName(std::string tsFieldName);
    void setDomainSignalMode(DomainSignalMode mode);

private:
    rapidjson::Document doc;
    std::string config;

    daq::LoggerComponentPtr loggerComponent;
    daq::SignalConfigPtr outputSignal;
    // used for description how to extract data from sample json
    MqttMsgDescriptor msgDescriptor;
    DomainSignalMode domainSignalType;

    std::pair<CmdResult, std::vector<DataPackets>>
    extractDataSamples(const std::string& json, const uint64_t externalTs);
    void sendDataSamples(const DataPackets& dataPackets);
    template <typename T>
    DataPackets buildDataPackets(const T& value, uint64_t timestamp);
    template <typename T>
    DataPackets buildDataPackets(const T& value, const std::vector<uint64_t>& timestamp);
    template <typename T>
    DataPackets buildDataPackets(const T& value);
    daq::DataPacketPtr buildDomainDataPacket(daq::GenericSignalConfigPtr<> signalConfig, uint64_t timestamp);
    daq::DataPacketPtr buildDomainDataPacket(daq::GenericSignalConfigPtr<> signalConfig, const std::vector<uint64_t>& timestamp);
    template<typename T>
    daq::DataPacketPtr buildDataPacket(daq::GenericSignalConfigPtr<> signalConfig,
                                       const T& value,
                                       const daq::DataPacketPtr domainPacket);
    template<typename T>
    daq::DataPacketPtr createEmptyDataPacket(const daq::GenericSignalConfigPtr<> signalConfig,
                                       const daq::DataPacketPtr domainPacket, const T& value);
    template <typename T> void copyDataIntoPacket(daq::DataPacketPtr dataPacket, const T& value);
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
