#include "mqtt_streaming_protocol/MqttDataWrapper.h"

#include <opendaq/binary_data_packet_factory.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/sample_type_traits.h>
#include <rapidjson/document.h>

#include <mqtt_streaming_protocol/utils.h>

namespace
{

template <typename T, typename IsFn, typename GetFn>
std::vector<T> parseHomogeneousArrayInternal(const rapidjson::Value::ConstArray& arr, IsFn isValid, GetFn getValue)
{
    std::vector<T> out;
    out.reserve(arr.Size());

    for (const auto& x : arr)
    {
        if (!isValid(x))
            return std::vector<T>{};

        out.push_back(getValue(x));
    }
    return out;
}

template <typename T>
std::pair<mqtt::CmdResult, std::vector<T>> parseHomogeneousArray(const rapidjson::Value::ConstArray& arr)
{
    return std::pair{mqtt::CmdResult{false, {}}, std::vector<T>{}};
}

template <>
std::pair<mqtt::CmdResult, std::vector<int64_t>> parseHomogeneousArray(const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::CmdResult, std::vector<int64_t>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<
        int64_t>(arr, [](const auto& x) { return x.IsInt64() || x.IsUint64(); }, [](const auto& x) { return x.GetInt64(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected integers). ");
    }
    return result;
}

template <>
std::pair<mqtt::CmdResult, std::vector<uint64_t>> parseHomogeneousArray(const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::CmdResult, std::vector<uint64_t>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<
        uint64_t>(arr, [](const auto& x) { return x.IsUint64(); }, [](const auto& x) { return x.GetUint64(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected unsigned integers). ");
    }
    return result;
}

template <>
std::pair<mqtt::CmdResult, std::vector<double>> parseHomogeneousArray(const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::CmdResult, std::vector<double>> result{{true, {}}, {}};
    result.second =
        parseHomogeneousArrayInternal<double>(arr, [](const auto& x) { return x.IsDouble(); }, [](const auto& x) { return x.GetDouble(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected doubles). ");
    }
    return result;
}

template <>
std::pair<mqtt::CmdResult, std::vector<std::string>> parseHomogeneousArray(const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::CmdResult, std::vector<std::string>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<
        std::string>(arr, [](const auto& x) { return x.IsString(); }, [](const auto& x) { return std::string{x.GetString()}; });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected strings). ");
    }
    return result;
}
} // namespace

namespace mqtt
{

MqttDataWrapper::MqttDataWrapper()
    : domainSignalMode(DomainSignalMode::None)
{
}

void MqttDataWrapper::setOutputSignal(daq::SignalConfigPtr outputSignal)
{
    this->outputSignal = outputSignal;
}

CmdResult MqttDataWrapper::createAndSendDataPacket(const std::string& json, const uint64_t externalTs)
{
    auto [status, packets] = extractDataSamples(json, externalTs);
    if (status.success)
    {
        for (const auto& data : packets)
        {
            sendDataSamples(data);
        }
    }
    return status;
}

void MqttDataWrapper::setValueFieldName(std::string valueFieldName)
{
    msgDescriptor.valueFieldName = std::move(valueFieldName);
}

void MqttDataWrapper::setTimestampFieldName(std::string tsFieldName)
{
    msgDescriptor.tsFieldName = std::move(tsFieldName);
}

void MqttDataWrapper::setDomainSignalMode(DomainSignalMode mode)
{
    domainSignalMode = mode;
}

bool MqttDataWrapper::parseJson(ExtractionContext& ctx, const std::string& json)
{
    ctx.jsonParsed = true;
    try
    {
        ctx.jsonDocument.Parse(json);
        if (ctx.jsonDocument.HasParseError())
        {
            ctx.result.addError("Error parsing MQTT payload as JSON. ");
            ctx.jsonParsed = false;
        }
        else if (!ctx.jsonDocument.IsObject())
        {
            ctx.result.addError("MQTT payload has wrong format. ");
            ctx.jsonParsed = false;
        }
    }
    catch (...)
    {
        ctx.result.addError("Error deserializing MQTT payload. ");
        ctx.jsonParsed = false;
    }
    return ctx.jsonParsed;
}

bool MqttDataWrapper::parseJsonFields(ExtractionContext& ctx)
{
    if (!ctx.jsonParsed)
        return false;
    try
    {
        for (auto it = ctx.jsonDocument.MemberBegin(); it != ctx.jsonDocument.MemberEnd(); ++it)
        {
            const std::string name = it->name.GetString();
            bool processed = false;
            processed |= extractValue(ctx, name);
            if (domainSignalMode == DomainSignalMode::ExtractFromMessage && !processed)
            {
                processed |= extractTimestamp(ctx, name);
            }
            if (!processed)
            {
                // the field is not precessed
            }
        }
    }
    catch (...)
    {
        ctx.result.addError("Error deserializing MQTT payload. ");
        return false;
    }
    return true;
}

bool MqttDataWrapper::extractValue(ExtractionContext& ctx, const std::string& jsonFieldName)
{
    bool fieldFound = (!msgDescriptor.valueFieldName.empty() && jsonFieldName == msgDescriptor.valueFieldName);
    if (fieldFound)
    {
        const auto fillContext = [&ctx](mqtt::CmdResult& parsingStatus, auto&& out)
        {
            ctx.result.merge(parsingStatus);
            ctx.valueExtracted = parsingStatus.success;
            if (parsingStatus.success)
                ctx.value = std::move(out);
        };
        const auto& v = ctx.jsonDocument[jsonFieldName];
        if (v.IsArray())
        {
            const auto& arr = v.GetArray();
            if (arr.Empty())
            {
                ctx.result.addError("Value field is an array but it is empty. ");
            }
            else if (arr[0].IsInt64() || arr[0].IsUint64())
            {
                auto [parsingStatus, out] = parseHomogeneousArray<int64_t>(arr);
                fillContext(parsingStatus, std::move(out));
            }
            else if (arr[0].IsDouble())
            {
                auto [parsingStatus, out] = parseHomogeneousArray<double>(arr);
                fillContext(parsingStatus, std::move(out));
            }
            else if (arr[0].IsString())
            {
                auto [parsingStatus, out] = parseHomogeneousArray<std::string>(arr);
                fillContext(parsingStatus, std::move(out));
            }
            else
            {
                ctx.result.addError(fmt::format("Unsupported value type for '{}' array. ", jsonFieldName));
            }
        }
        else
        {
            ctx.valueExtracted = true;
            if (v.IsInt64())
                ctx.value = std::vector<int64_t>{v.GetInt64()};
            else if (v.IsUint64())
                ctx.value = std::vector<int64_t>{static_cast<int64_t>(v.GetUint64())};
            else if (v.IsDouble())
                ctx.value = std::vector<double>{v.GetDouble()};
            else if (v.IsString())
                ctx.value = std::vector<std::string>{std::string(v.GetString())};
            else
            {
                ctx.result.addError(fmt::format("Unsupported value type for '{}'. ", jsonFieldName));
                ctx.valueExtracted = false;
            }
        }
    }
    return fieldFound;
}

bool MqttDataWrapper::extractTimestamp(ExtractionContext& ctx, const std::string& jsonFieldName)
{
    bool fieldFound = (!msgDescriptor.tsFieldName.empty() && jsonFieldName == msgDescriptor.tsFieldName);
    if (fieldFound)
    {
        const auto& tsField = ctx.jsonDocument[jsonFieldName];
        if (tsField.IsArray())
        {
            const auto& arr = tsField.GetArray();
            if (arr.Empty())
            {
                ctx.result.addError("Timestamp field is an array but it is empty. ");
            }
            else if (arr[0].IsInt() || arr[0].IsUint64() || arr[0].IsInt64())
            {
                auto [parsingStatus, out] = parseHomogeneousArray<uint64_t>(arr);
                ctx.result.merge(parsingStatus);
                ctx.tsExtracted = parsingStatus.success;
                if (parsingStatus.success)
                    ctx.ts = std::move(out);

                std::for_each(ctx.ts.begin(), ctx.ts.end(), [](auto& val) { val = mqtt::utils::numericToMicroseconds(val); });
            }
            else if (arr[0].IsString())
            {
                auto [parsingStatus, out] = parseHomogeneousArray<std::string>(arr);
                ctx.result.merge(parsingStatus);
                ctx.tsExtracted = parsingStatus.success;
                if (parsingStatus.success)
                {
                    ctx.ts.reserve(out.size());
                    std::for_each(out.cbegin(), out.cend(), [&ctx](const auto& val) { ctx.ts.push_back(utils::toUnixTicks(val)); });
                }
            }
            else
            {
                ctx.result.addError("Timestamp value type is not supported. ");
            }
        }
        else
        {
            ctx.tsExtracted = true;
            if (tsField.IsInt() || tsField.IsUint64() || tsField.IsInt64())
                ctx.ts.push_back(mqtt::utils::numericToMicroseconds(tsField.GetUint64()));
            else if (tsField.IsString())
                ctx.ts.push_back(utils::toUnixTicks(tsField.GetString()));
            else
            {
                ctx.result.addError("Timestamp value type is not supported. ");
                ctx.tsExtracted = false;
            }
        }
    }
    return fieldFound;
}

bool MqttDataWrapper::validateExtractionResult(ExtractionContext& ctx)
{
    if (!ctx.jsonParsed)
        return false;

    bool valueExtractionError = !ctx.valueExtracted;
    bool tsExtractionError =
        (domainSignalMode == DomainSignalMode::ExtractFromMessage && !msgDescriptor.tsFieldName.empty() && !ctx.tsExtracted);
    bool alignmentError = false;

    if (valueExtractionError || tsExtractionError)
    {
        ctx.result.addError("Not all required fields are present in the JSON message. ");
        if (valueExtractionError)
            ctx.result.addError(fmt::format("Couldn't extract value field (\"{}\") from the JSON message. ", msgDescriptor.valueFieldName));
        if (tsExtractionError)
            ctx.result.addError(
                fmt::format("Couldn't extract timestamp field (\"{}\") from the JSON message. ", msgDescriptor.tsFieldName));
    }

    auto getSize = [](const ValueVariant& v) { return std::visit([](const auto& vec) { return vec.size(); }, v); };

    if (domainSignalMode == DomainSignalMode::ExtractFromMessage && ctx.tsExtracted && ctx.ts.size() != getSize(ctx.value))
    {
        ctx.result.addError("Timestamp and value array sizes do not match. ");
        alignmentError = true;
    }
    else if (domainSignalMode == DomainSignalMode::ExternalTimestamp && getSize(ctx.value) != 1)
    {
        ctx.result.addError("External timestamp mode doesn't support arrays of values. ");
        alignmentError = true;
    }

    return (valueExtractionError || tsExtractionError || alignmentError);
}

bool MqttDataWrapper::buildPackets(ExtractionContext& ctx, const uint64_t externalTs)
{
    ctx.dataPacketBuilt = false;
    if (!ctx.result.success)
        return ctx.dataPacketBuilt;

    std::visit(
        [this, &ctx, externalTs](auto&& values)
        {
            if (domainSignalMode == DomainSignalMode::ExternalTimestamp)
                ctx.ts = std::vector<uint64_t>{externalTs};

            if (domainSignalMode != DomainSignalMode::None)
                ctx.outputData = buildDataPackets(values, ctx.ts);
            else
                ctx.outputData = buildDataPackets(values);

            ctx.dataPacketBuilt = (ctx.outputData.size() == values.size());
        },
        ctx.value);
    return ctx.dataPacketBuilt;
}

std::pair<CmdResult, std::vector<MqttDataWrapper::DataPackets>> MqttDataWrapper::extractDataSamples(const std::string& json,
                                                                                                    const uint64_t externalTs)
{
    ExtractionContext ctx;

    parseJson(ctx, json);
    parseJsonFields(ctx);
    validateExtractionResult(ctx);
    buildPackets(ctx, externalTs);

    return std::pair{std::move(ctx.result), std::move(ctx.outputData)};
}

void MqttDataWrapper::sendDataSamples(const DataPackets& dataPackets)
{
    auto domainSignal = outputSignal.getDomainSignal();

    outputSignal.sendPacket(dataPackets.dataPacket);
    if (domainSignal.assigned() && dataPackets.domainDataPacket.assigned())
        domainSignal.asPtr<daq::ISignalConfig>().sendPacket(dataPackets.domainDataPacket);
}

template <typename T>
std::vector<MqttDataWrapper::DataPackets> MqttDataWrapper::buildDataPackets(const std::vector<T>& value,
                                                                            const std::vector<uint64_t>& timestamp)
{
    std::vector<DataPackets> dataPackets;
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
    {
        dataPackets.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i)
            dataPackets.push_back(buildDataPacketsImpl(value[i], timestamp[i]));
    }
    else
    {
        dataPackets.push_back(buildDataPacketsImpl(value, timestamp));
    }

    return dataPackets;
}

template <typename T>
std::vector<MqttDataWrapper::DataPackets> MqttDataWrapper::buildDataPackets(const std::vector<T>& value)
{
    std::vector<DataPackets> dataPackets;
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
    {
        dataPackets.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i)
        {
            DataPackets pkt;
            pkt.dataPacket = buildDataPacket(outputSignal, value[i], daq::DataPacketPtr());
            dataPackets.push_back(std::move(pkt));
        }
    }
    else
    {
        DataPackets pkt;
        pkt.dataPacket = buildDataPacket(outputSignal, value, daq::DataPacketPtr());
        dataPackets.push_back(std::move(pkt));
    }

    return dataPackets;
}

template <typename T>
MqttDataWrapper::DataPackets MqttDataWrapper::buildDataPacketsImpl(const T& value, uint64_t timestamp)
{
    DataPackets dataPackets;

    dataPackets.domainDataPacket = buildDomainDataPacket(outputSignal, timestamp);
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename T>
MqttDataWrapper::DataPackets MqttDataWrapper::buildDataPacketsImpl(const std::vector<T>& value, const std::vector<uint64_t>& timestamp)
{
    DataPackets dataPackets;

    dataPackets.domainDataPacket = buildDomainDataPacket(outputSignal, timestamp);
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename TReadType>
bool MqttDataWrapper::isTypeTheSame(daq::SampleType sampleType)
{
    using daq::SampleType;
    using daq::SampleTypeFromType;
    using daq::SampleTypeToType;
    switch (sampleType)
    {
        case daq::SampleType::Float32:
            return std::is_same_v<daq::SampleTypeToType<daq::SampleType::Float32>::Type, TReadType>;
        case daq::SampleType::Float64:
            return std::is_same_v<SampleTypeToType<SampleType::Float64>::Type, TReadType>;
        case daq::SampleType::UInt8:
            return std::is_same_v<SampleTypeToType<SampleType::UInt8>::Type, TReadType>;
        case daq::SampleType::Int8:
            return std::is_same_v<SampleTypeToType<SampleType::Int8>::Type, TReadType>;
        case daq::SampleType::Int16:
            return std::is_same_v<SampleTypeToType<SampleType::Int16>::Type, TReadType>;
        case daq::SampleType::UInt16:
            return std::is_same_v<SampleTypeToType<SampleType::UInt16>::Type, TReadType>;
        case daq::SampleType::Int32:
            return std::is_same_v<SampleTypeToType<SampleType::Int32>::Type, TReadType>;
        case daq::SampleType::UInt32:
            return std::is_same_v<SampleTypeToType<SampleType::UInt32>::Type, TReadType>;
        case daq::SampleType::Int64:
            return std::is_same_v<SampleTypeToType<SampleType::Int64>::Type, TReadType>;
        case daq::SampleType::UInt64:
            return std::is_same_v<SampleTypeToType<SampleType::UInt64>::Type, TReadType>;
        case daq::SampleType::Binary:
        case daq::SampleType::String:
            return std::is_same_v<SampleTypeToType<SampleType::String>::Type, typename SampleTypeFromType<TReadType>::Type>;
        case daq::SampleType::RangeInt64:
        case daq::SampleType::ComplexFloat32:
        case daq::SampleType::ComplexFloat64:
        case daq::SampleType::Struct:
        case daq::SampleType::Invalid:
        case daq::SampleType::Null:
        case daq::SampleType::_count:
            break;
    }

    return false;
}

template <typename T>
daq::DataPacketPtr
MqttDataWrapper::buildDataPacket(daq::SignalConfigPtr signalConfig, const T& value, const daq::DataPacketPtr domainPacket)
{
    const auto curType = signalConfig.getDescriptor().getSampleType();
    using ActualType = sample_type_t<T>;
    if (!isTypeTheSame<ActualType>(curType))
    {
        auto descriptor =
            DataDescriptorBuilderCopy(signalConfig.getDescriptor()).setSampleType(daq::SampleTypeFromType<ActualType>::SampleType).build();
        signalConfig.setDescriptor(descriptor);
    }

    auto dataPacket = createEmptyDataPacket<T>(signalConfig, domainPacket, value);
    copyDataIntoPacket(dataPacket, value);
    return dataPacket;
}

template <typename T>
daq::DataPacketPtr
MqttDataWrapper::createEmptyDataPacket(const daq::SignalConfigPtr signalConfig, const daq::DataPacketPtr domainPacket, const T& value)
{
    daq::DataPacketPtr dataPacket;
    uint64_t size = 1;
    using ActualType = sample_type_t<T>;
    if constexpr (is_std_vector_v<T> || std::is_same_v<ActualType, std::string>)
    {
        size = value.size();
    }

    if constexpr (std::is_same_v<ActualType, std::string>)
    {
        dataPacket = daq::BinaryDataPacket(domainPacket, signalConfig.getDescriptor(), size);
    }
    else
    {
        if (signalConfig.getDomainSignal().assigned() && domainPacket.assigned())
        {
            dataPacket = DataPacketWithDomain(domainPacket, signalConfig.getDescriptor(), size);
        }
        else
        {
            dataPacket = DataPacket(signalConfig.getDescriptor(), size);
        }
    }

    return dataPacket;
}

template <typename T>
void MqttDataWrapper::copyDataIntoPacket(daq::DataPacketPtr dataPacket, const T& value)
{
    if (!dataPacket.assigned())
        return;

    using ActualType = sample_type_t<T>;
    if constexpr (is_std_vector_v<T> && !std::is_same_v<ActualType, std::string>)
    {
        std::memcpy(dataPacket.getRawData(), value.data(), value.size() * sizeof(ActualType));
    }
    else if constexpr (!is_std_vector_v<T> && std::is_same_v<ActualType, std::string>)
    {
        std::memcpy(dataPacket.getData(), value.c_str(), value.size());
    }
    else if constexpr (!is_std_vector_v<T> && !std::is_same_v<ActualType, std::string>)
    {
        auto outputData = reinterpret_cast<T*>(dataPacket.getRawData());
        *outputData = value;
    }
}

daq::DataPacketPtr MqttDataWrapper::buildDomainDataPacket(daq::SignalConfigPtr signalConfig, uint64_t timestamp)
{
    daq::DataPacketPtr dataPacket;
    if (signalConfig.getDomainSignal().assigned())
    {
        dataPacket = daq::DataPacket(signalConfig.getDomainSignal().getDescriptor(), 1);
        std::memcpy(dataPacket.getRawData(), &timestamp, sizeof(timestamp));
    }

    return dataPacket;
}

daq::DataPacketPtr MqttDataWrapper::buildDomainDataPacket(daq::SignalConfigPtr signalConfig, const std::vector<uint64_t>& timestamp)
{
    daq::DataPacketPtr dataPacket;
    if (signalConfig.getDomainSignal().assigned())
    {
        dataPacket = daq::DataPacket(signalConfig.getDomainSignal().getDescriptor(), timestamp.size());
        std::memcpy(dataPacket.getRawData(), timestamp.data(), timestamp.size() * sizeof(uint64_t));
    }

    return dataPacket;
}
} // namespace mqtt
