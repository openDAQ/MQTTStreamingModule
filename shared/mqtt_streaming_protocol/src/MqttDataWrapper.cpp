#include "MqttDataWrapper.h"

#include <boost/algorithm/string.hpp>
#include <coreobjects/unit_factory.h>
#include <opendaq/binary_data_packet_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/sample_type_traits.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <timestampConverter.h>
#include <variant>

namespace{

template <typename T, typename IsFn, typename GetFn>
std::vector<T> parseHomogeneousArrayInternal(
    const rapidjson::Value::ConstArray& arr,
    IsFn isValid,
    GetFn getValue)
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
std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<T>> parseHomogeneousArray(
    const rapidjson::Value::ConstArray& arr)
{
    return std::pair{mqtt::MqttDataWrapper::CmdResult{false, {}}, std::vector<T>{}};
}

template <>
std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<int64_t>> parseHomogeneousArray(
    const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<int64_t>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<int64_t>(
        arr,
        [](const auto& x) { return x.IsInt64() || x.IsUint64(); },
        [](const auto& x) { return x.GetInt64(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected integers). ");
    }
    return result;
}

template <>
std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<uint64_t>> parseHomogeneousArray(
    const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<uint64_t>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<uint64_t>(
        arr,
        [](const auto& x) { return x.IsUint64(); },
        [](const auto& x) { return x.GetUint64(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected unsigned integers). ");
    }
    return result;
}

template <>
std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<double>> parseHomogeneousArray(
    const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<double>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<double>(
        arr,
        [](const auto& x) { return x.IsDouble(); },
        [](const auto& x) { return x.GetDouble(); });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected doubles). ");
    }
    return result;
}

template <>
std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<std::string>> parseHomogeneousArray(
    const rapidjson::Value::ConstArray& arr)
{
    std::pair<mqtt::MqttDataWrapper::CmdResult, std::vector<std::string>> result{{true, {}}, {}};
    result.second = parseHomogeneousArrayInternal<std::string>(
        arr,
        [](const auto& x) { return x.IsString(); },
        [](const auto& x) { return std::string{x.GetString()}; });
    if (result.second.empty())
    {
        result.first.addError("Mixed types in value array (expected strings). ");
    }
    return result;
}
}

namespace mqtt
{

MqttDataWrapper::MqttDataWrapper(daq::LoggerComponentPtr loggerComponent)
    : loggerComponent(loggerComponent),
      domainSignalType(DomainSignalMode::None)
{
}

MqttDataWrapper::CmdResult MqttDataWrapper::validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent)
{

    MqttDataWrapper::CmdResult result(true, "");
    if (!topic.assigned() || topic.getLength() == 0)
    {
        result = MqttDataWrapper::CmdResult(false, "Empty topic is not allowed!");
        if (loggerComponent.assigned())
        {
            LOG_W("{}", result.msg);
        }
        return result;
    }

    std::vector<std::string> list;
    boost::split(list, topic.toStdString(), boost::is_any_of("/"));

    for (const auto& part : list)
    {
        if (part == "#" || part == "+")
        {
            result = MqttDataWrapper::CmdResult(false,
                                                fmt::format("Wildcard characters '+' and '#' are not allowed in topic: {}",
                                                            topic.toStdString()));
            if (loggerComponent.assigned())
            {
                LOG_W("{}", result.msg);
            }
            return result;
        }
    }

    return result;
}

void MqttDataWrapper::setConfig(const std::string& config)
{
    this->config = config;
    doc.Parse(config.c_str());
}

std::vector<std::pair<std::string, MqttMsgDescriptor>> MqttDataWrapper::extractDescription()
{
    std::vector<std::pair<std::string, MqttMsgDescriptor>> result;
    if (config.empty() || !isJsonValid().success)
        return result;

    auto it = doc.MemberBegin();
    const rapidjson::Value& array = it->value;
    // Each topic array contains one or more signal objects
    for (const auto& elem : array.GetArray())
    {
        // Iterate over signals inside this element
        for (auto sigIt = elem.MemberBegin(); sigIt != elem.MemberEnd(); ++sigIt)
        {
            std::string SignalName(sigIt->name.GetString());
            const rapidjson::Value& signalObj = sigIt->value;
            MqttMsgDescriptor msgDescriptor;
            msgDescriptor.unit = extractSignalUnit(signalObj);
            msgDescriptor.valueFieldName = extractValueFieldName(signalObj);
            msgDescriptor.tsFieldName = extractTimestampFieldName(signalObj);
            result.push_back(std::pair(std::move(SignalName), std::move(msgDescriptor)));
        }
    }

    return result;
}

std::string MqttDataWrapper::extractTopic()
{
    std::string topic;
    if (config.empty() || !isJsonValid().success)
        return topic;

    topic = doc.MemberBegin()->name.GetString();
    return topic;
}

MqttDataWrapper::CmdResult MqttDataWrapper::isJsonValid()
{
    rapidjson::Document doc;

    if (config.empty())
        return CmdResult(true);

    if (doc.Parse(config.c_str()).HasParseError())
        return CmdResult(false, "The JSON config has an invalid format");

    if (!doc.IsObject())
        return CmdResult(false, "The JSON config must be an object");

    if (doc.MemberCount() != 1)
        return CmdResult(false, "The JSON config must contain exactly one topic");

    const auto& arrayValue = doc.MemberBegin()->value;
    if (!arrayValue.IsArray())
        return CmdResult(false, "The JSON config has wrong format (expected array of signals)");

    if (!arrayValue.Empty())
    {
        for (const auto& signalEntry : arrayValue.GetArray())
        {
            if (!signalEntry.IsObject())
                return CmdResult(false, "Each signal entry must be an object");

            if (signalEntry.MemberCount() != 1)
                return CmdResult(false, "Each signal entry must contain exactly one signal");

            const auto& signal = *signalEntry.MemberBegin();
            const auto& signalBody = signal.value;

            if (!signalBody.IsObject())
                return CmdResult(false, "Signal definition must be an object");

            if (!signalBody.HasMember("Value"))
            {
                return CmdResult(false, "Signal must contain a Value field");
            }

            if (!signalBody["Value"].IsString())
                return CmdResult(false, "Signal field 'Value' must be a string");

            if (signalBody.HasMember("Timestamp") && !signalBody["Timestamp"].IsString())
                return CmdResult(false, "Signal field 'Timestamp' must be a string");

            if (signalBody.HasMember("Unit"))
            {
                const auto& unit = signalBody["Unit"];
                if (!unit.IsArray() || unit.Empty())
                    return CmdResult(false, "Signal field 'Unit' must be a non-empty array");
                for (const auto& u : unit.GetArray())
                {
                    if (!u.IsString())
                        return CmdResult(false, "Each Unit entry must be a string");
                }
            }
        }
    }
    return CmdResult(true);
}

void MqttDataWrapper::setOutputSignal(daq::SignalConfigPtr outputSignal)
{
    this->outputSignal = outputSignal;
}

MqttDataWrapper::CmdResult MqttDataWrapper::createAndSendDataPacket(const std::string& json, const uint64_t externalTs)
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
    domainSignalType = mode;
}

std::pair<MqttDataWrapper::CmdResult, std::vector<DataPackets>> MqttDataWrapper::extractDataSamples(const std::string& json,
                                                                                                    const uint64_t externalTs)
{
    using ValueVariant = std::variant<std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;
    ValueVariant value{};
    std::vector<DataPackets> outputData;
    std::vector<uint64_t> ts;
    bool hasTS = false;
    bool hasValue = false;
    CmdResult result(true);

    try
    {
        rapidjson::Document jsonDocument;
        jsonDocument.Parse(json);
        if (jsonDocument.HasParseError())
        {
            return std::pair{result.addError("Error parsing mqtt payload as JSON"), outputData};
        }

        if (jsonDocument.IsObject())
        {
            for (auto it = jsonDocument.MemberBegin(); it != jsonDocument.MemberEnd(); ++it)
            {
                const std::string name = it->name.GetString();
                if (!msgDescriptor.valueFieldName.empty() && name == msgDescriptor.valueFieldName)
                {
                    const auto& v = jsonDocument[name];
                    if (v.IsArray())
                    {
                        const auto& arr = v.GetArray();
                        if (arr.Empty())
                        {
                            result.addError("Value field is an array but it is empty. ");
                        }
                        else if (arr[0].IsInt64() || arr[0].IsUint64())
                        {
                            auto [parsingStatus, out] = parseHomogeneousArray<int64_t>(arr);
                            result.merge(parsingStatus);
                            hasValue = parsingStatus.success;
                            if (parsingStatus.success)
                                value = std::move(out);
                        }
                        else if (arr[0].IsDouble())
                        {
                            auto [parsingStatus, out] = parseHomogeneousArray<double>(arr);
                            result.merge(parsingStatus);
                            hasValue = parsingStatus.success;
                            if (parsingStatus.success)
                                value = std::move(out);
                        }
                        else if (arr[0].IsString())
                        {
                            auto [parsingStatus, out] = parseHomogeneousArray<std::string>(arr);
                            result.merge(parsingStatus);
                            hasValue = parsingStatus.success;
                            if (parsingStatus.success)
                                value = std::move(out);
                        }
                        else
                        {
                            result.addError(fmt::format("Unsupported value type for '{}' array. ", name));
                        }
                    }
                    else
                    {
                        hasValue = true;
                        if (v.IsInt64())
                            value = std::vector<int64_t>{v.GetInt64()};
                        else if (v.IsUint64())
                            value = std::vector<int64_t>{static_cast<int64_t>(v.GetUint64())};
                        else if (v.IsDouble())
                            value = std::vector<double>{v.GetDouble()};
                        else if (v.IsString())
                            value = std::vector<std::string>{std::string(v.GetString())};
                        else
                        {
                            result.addError(fmt::format("Unsupported value type for '{}'. ", name));
                            hasValue = false;
                        }
                    }
                }
                else if (domainSignalType == DomainSignalMode::ExtractFromMessage && !msgDescriptor.tsFieldName.empty() &&
                         name == msgDescriptor.tsFieldName)
                {
                    const auto& tsField = jsonDocument[name];
                    if (tsField.IsArray())
                    {
                        const auto& arr = tsField.GetArray();
                        if (arr.Empty())
                        {
                            result.addError("Timestamp field is an array but it is empty. ");
                        }
                        else if (arr[0].IsInt() || arr[0].IsUint64() || arr[0].IsInt64())
                        {
                            auto [parsingStatus, out] = parseHomogeneousArray<uint64_t>(arr);
                            result.merge(parsingStatus);
                            hasTS = parsingStatus.success;
                            if (parsingStatus.success)
                                ts = std::move(out);

                            std::for_each(ts.begin(), ts.end(), [](auto& val) { val = mqtt::utils::numericToMicroseconds(val); });
                        }
                        else if (arr[0].IsString())
                        {
                            std::vector<std::string> stringTs;
                            auto [parsingStatus, out] = parseHomogeneousArray<std::string>(arr);
                            result.merge(parsingStatus);
                            hasTS = parsingStatus.success;
                            if (parsingStatus.success)
                                stringTs = std::move(out);
                            ts.reserve(stringTs.size());
                            std::for_each(stringTs.cbegin(),
                                          stringTs.cend(),
                                          [&ts](const auto& val) { ts.push_back(utils::toUnixTicks(val)); });
                            hasTS = true;
                        }
                        else
                        {
                            result.addError("Timestamp value type is not supported. ");
                        }
                    }
                    else
                    {
                        if (tsField.IsInt() || tsField.IsUint64() || tsField.IsInt64())
                        {
                            ts.push_back(mqtt::utils::numericToMicroseconds(tsField.GetUint64()));
                            hasTS = true;
                        }
                        else if (tsField.IsString())
                        {
                            ts.push_back(utils::toUnixTicks(tsField.GetString()));
                            hasTS = true;
                        }
                        else
                        {
                            result.addError("Timestamp value type is not supported. ");
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        result.addError("Error deserializing MQTT payload. ");
    }
    if (!hasValue || (domainSignalType == DomainSignalMode::ExtractFromMessage && !msgDescriptor.tsFieldName.empty() && !hasTS))
    {
        result.addError("Not all required fields are present in the JSON message. ");
        if (!hasValue)
            result.addError(fmt::format("Couldn't extract value field (\"{}\") from the JSON message. ", msgDescriptor.valueFieldName));
        if (domainSignalType == DomainSignalMode::ExtractFromMessage && !msgDescriptor.tsFieldName.empty() && !hasTS)
            result.addError(fmt::format("Couldn't extract timestamp field (\"{}\") from the JSON message. ", msgDescriptor.tsFieldName));
    }

    if (result.success)
    {
        std::visit(
            [&](auto&& values)
            {
                using T = std::decay_t<decltype(values)>;
                if (domainSignalType == DomainSignalMode::ExtractFromMessage && hasTS && ts.size() != values.size())
                {
                    result.addError("Timestamp and value array sizes do not match. ");
                    return;
                }
                if (domainSignalType == DomainSignalMode::ExternalTimestamp && values.size() > 1)
                {
                    result.addError("External timestamp mode doesn't support arrays of values. ");
                    return;
                }
                if constexpr (std::is_same_v<T, std::vector<std::string>>)
                {
                    for (size_t i = 0; i < values.size(); ++i)
                    {
                        DataPackets dp;
                        if (domainSignalType == DomainSignalMode::ExternalTimestamp)
                        {
                            dp = buildDataPackets(values[i], externalTs);
                        }
                        else if (domainSignalType == DomainSignalMode::ExtractFromMessage)
                        {
                            dp = buildDataPackets(values[i], ts[i]);
                        }
                        else if (domainSignalType == DomainSignalMode::None)
                        {
                            dp = buildDataPackets(values[i]);
                        }

                        if (dp.dataPacket.assigned())
                            outputData.push_back(std::move(dp));
                    }
                }
                else
                {
                    DataPackets dp;
                    if (domainSignalType == DomainSignalMode::ExternalTimestamp)
                    {
                        dp = buildDataPackets(values, externalTs);
                    }
                    else if (domainSignalType == DomainSignalMode::ExtractFromMessage)
                    {
                        dp = buildDataPackets(values, ts);
                    }
                    else if (domainSignalType == DomainSignalMode::None)
                    {
                        dp = buildDataPackets(values);
                    }

                    if (dp.dataPacket.assigned())
                        outputData.push_back(std::move(dp));
                }
            },
            value);
    }
    return std::pair{result, outputData};
}

void MqttDataWrapper::sendDataSamples(const DataPackets& dataPackets)
{
    auto domainSignal = outputSignal.getDomainSignal();

    outputSignal.sendPacket(dataPackets.dataPacket);
    if (domainSignal.assigned() && dataPackets.domainDataPacket.assigned())
        domainSignal.asPtr<daq::ISignalConfig>().sendPacket(dataPackets.domainDataPacket);
}

template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(const T& value, uint64_t timestamp)
{
    DataPackets dataPackets;

    dataPackets.domainDataPacket = buildDomainDataPacket(outputSignal, timestamp);
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(const T& value, const std::vector<uint64_t>& timestamp)
{
    DataPackets dataPackets;

    dataPackets.domainDataPacket = buildDomainDataPacket(outputSignal, timestamp);
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(const T& value)
{
    DataPackets dataPackets;
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, daq::DataPacketPtr());

    return dataPackets;
}

template <typename TReadType>
bool MqttDataWrapper::isTypeTheSame(daq::SampleType sampleType)
{
    using daq::SampleTypeToType;
    using daq::SampleTypeFromType;
    using daq::SampleType;
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
MqttDataWrapper::buildDataPacket(daq::GenericSignalConfigPtr<> signalConfig, const T& value, const daq::DataPacketPtr domainPacket)
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

template<typename T>
daq::DataPacketPtr MqttDataWrapper::createEmptyDataPacket(const daq::GenericSignalConfigPtr<> signalConfig, const daq::DataPacketPtr domainPacket, const T& value)
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

daq::DataPacketPtr MqttDataWrapper::buildDomainDataPacket(daq::GenericSignalConfigPtr<> signalConfig, uint64_t timestamp)
{
    daq::DataPacketPtr dataPacket;
    if (signalConfig.getDomainSignal().assigned())
    {
        dataPacket = daq::DataPacket(signalConfig.getDomainSignal().getDescriptor(), 1);
        std::memcpy(dataPacket.getRawData(), &timestamp, sizeof(timestamp));
    }

    return dataPacket;
}

daq::DataPacketPtr MqttDataWrapper::buildDomainDataPacket(daq::GenericSignalConfigPtr<> signalConfig, const std::vector<uint64_t>& timestamp)
{
    daq::DataPacketPtr dataPacket;
    if (signalConfig.getDomainSignal().assigned())
    {
        dataPacket = daq::DataPacket(signalConfig.getDomainSignal().getDescriptor(), timestamp.size());
        std::memcpy(dataPacket.getRawData(), timestamp.data(), timestamp.size() * sizeof(uint64_t));
    }

    return dataPacket;
}

daq::UnitPtr MqttDataWrapper::extractSignalUnit(const rapidjson::Value& signalObj)
{
    daq::UnitPtr unit;
    if (signalObj.HasMember("Unit") && signalObj["Unit"].IsArray())
    {
        auto unitBuilder = daq::UnitBuilder();
        auto unitArr = signalObj["Unit"].GetArray();
        if (unitArr.Size() >= 1 && unitArr[0].IsString())
        {
            unitBuilder.setSymbol(unitArr[0].GetString());
        }
        if (unitArr.Size() >= 2 && unitArr[1].IsString())
        {
            unitBuilder.setName(unitArr[1].GetString());
        }
        if (unitArr.Size() >= 3 && unitArr[2].IsString())
        {
            unitBuilder.setQuantity(unitArr[2].GetString());
        }
        unit = unitBuilder.build();
    }
    return unit;
}

std::string MqttDataWrapper::extractValueFieldName(const rapidjson::Value& signalObj)
{
    return extractFieldName(signalObj, "Value");
}

std::string MqttDataWrapper::extractTimestampFieldName(const rapidjson::Value& signalObj)
{
    return extractFieldName(signalObj, "Timestamp");
}
std::string MqttDataWrapper::extractFieldName(const rapidjson::Value& signalObj,
                                              const std::string& field)
{
    return (signalObj.HasMember(field) && signalObj[field].IsString())
               ? signalObj[field].GetString()
               : "";
}
} // namespace mqtt
