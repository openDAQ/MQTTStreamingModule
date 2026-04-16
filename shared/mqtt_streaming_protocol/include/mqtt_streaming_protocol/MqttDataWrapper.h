#pragma once

#include <cstdint>
#include <rapidjson/document.h>
#include <string>
#include <variant>
#include <vector>

#include "common.h"
#include <opendaq/packet_factory.h>
#include <opendaq/signal.h>
#include <opendaq/signal_config_ptr.h>

namespace mqtt
{

template <typename T>
struct container_traits
{
    static constexpr bool is_vector = false;
    using value_type = T;
};

template <typename U, typename Alloc>
struct container_traits<std::vector<U, Alloc>>
{
    static constexpr bool is_vector = true;
    using value_type = U;
};

template <typename T>
inline constexpr bool is_std_vector_v = container_traits<T>::is_vector;

template <typename T>
using sample_type_t = typename container_traits<T>::value_type;

class MqttDataWrapper final
{
public:
    using ValueVariant = std::variant<std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;

    enum class DomainSignalMode : int
    {
        None = 0,
        ExtractFromMessage,
        ExternalTimestamp,
        _count
    };

    MqttDataWrapper();

    void setOutputSignal(daq::SignalConfigPtr outputSignal);
    CmdResult createAndSendDataPacket(const std::string& json, const uint64_t externalTs);
    void setValueFieldName(std::string valueFieldName);
    void setTimestampFieldName(std::string tsFieldName);
    void setDomainSignalMode(DomainSignalMode mode);

private:
    struct DataPackets
    {
        daq::DataPacketPtr dataPacket;
        daq::DataPacketPtr domainDataPacket;
    };

    struct ExtractionContext
    {
        rapidjson::Document jsonDocument;
        ValueVariant value{};
        std::vector<uint64_t> ts;
        std::vector<DataPackets> outputData;
        CmdResult result{true};
        bool tsExtracted = false;
        bool valueExtracted = false;
        bool jsonParsed = false;
        bool dataPacketBuilt = false;
    };

    daq::SignalConfigPtr outputSignal;
    // used for description how to extract data from sample json
    MqttMsgDescriptor msgDescriptor;
    DomainSignalMode domainSignalMode;

    std::pair<CmdResult, std::vector<DataPackets>> extractDataSamples(const std::string& json, const uint64_t externalTs);
    void sendDataSamples(const DataPackets& dataPackets);

    template <typename T>
    std::vector<DataPackets> buildDataPackets(const std::vector<T>& value, const std::vector<uint64_t>& timestamp);
    template <typename T>
    std::vector<DataPackets> buildDataPackets(const std::vector<T>& value);
    template <typename T>
    DataPackets buildDataPacketsImpl(const T& value, uint64_t timestamp);
    template <typename T>
    DataPackets buildDataPacketsImpl(const std::vector<T>& value, const std::vector<uint64_t>& timestamp);

    template <typename T>
    daq::DataPacketPtr buildDataPacket(daq::SignalConfigPtr signalConfig, const T& value, const daq::DataPacketPtr domainPacket);
    daq::DataPacketPtr buildDomainDataPacket(daq::SignalConfigPtr signalConfig, uint64_t timestamp);
    daq::DataPacketPtr buildDomainDataPacket(daq::SignalConfigPtr signalConfig, const std::vector<uint64_t>& timestamp);
    template <typename T>
    daq::DataPacketPtr
    createEmptyDataPacket(const daq::SignalConfigPtr signalConfig, const daq::DataPacketPtr domainPacket, const T& value);
    template <typename T>
    void copyDataIntoPacket(daq::DataPacketPtr dataPacket, const T& value);

    bool parseJson(ExtractionContext& ctx, const std::string& json);
    bool parseJsonFields(ExtractionContext& ctx);
    bool extractValue(ExtractionContext& ctx, const rapidjson::Value& node);
    bool extractTimestamp(ExtractionContext& ctx, const rapidjson::Value& node);
    bool validateExtractionResult(ExtractionContext& ctx);
    bool buildPackets(ExtractionContext& ctx, const uint64_t externalTs);

    template <typename TReadType>
    static bool isTypeTheSame(daq::SampleType sampleType);
};
} // namespace mqtt
