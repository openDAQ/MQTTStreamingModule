#include "MqttDataWrapper.h"

#include "rapidjson/writer.h"
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

namespace mqtt
{

static const char* TOPIC_ALL_SIGNALS_PREFIX = "openDAQ";

MqttDataWrapper::MqttDataWrapper(daq::LoggerComponentPtr loggerComponent)
    : loggerComponent(loggerComponent)
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
        if (!elem.IsObject())
            continue;

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
        return CmdResult(false, "The JSON config has wrong format");
    if (!doc.IsObject())
        return CmdResult(false, "The JSON config is not an object");

    if (doc.MemberCount() > 1)
        return CmdResult(false, "The JSON config has insufficient number of fields");
    const auto& value = doc.MemberBegin()->value;
    if (value.IsArray() == false)
        return CmdResult(false, "The JSON config has wrong format (expected array of signals)");

    for (const auto& element : value.GetArray())
    {
        if (!element.IsObject())
            return CmdResult(false, "The JSON config has wrong format (expected signal object)");
    }

    return CmdResult(true);
}

void MqttDataWrapper::setOutputSignal(daq::SignalConfigPtr outputSignal)
{
    this->outputSignal = outputSignal;
}

MqttDataWrapper::CmdResult MqttDataWrapper::createAndSendDataPacket(const std::string& json)
{
    auto [status, packets] = extractDataSamples(msgDescriptor, json);
    if (status.success)
    {
        for (const auto& data : packets)
        {
            sendDataSamples(data);
        }
    }
    return status;
}

// bool MqttDataWrapper::hasDomainSignal(const SignalId& signalId) const
// {
//     return (msgDescriptor.signalName == signalId.signalName) ? !msgDescriptor.tsFieldName.empty() : false;
// }

void MqttDataWrapper::setValueFieldName(std::string valueFieldName)
{
    msgDescriptor.valueFieldName = std::move(valueFieldName);
}

void MqttDataWrapper::setTimestampFieldName(std::string tsFieldName)
{
    msgDescriptor.tsFieldName = std::move(tsFieldName);
}

std::pair<MqttDataWrapper::CmdResult, std::vector<DataPackets>> MqttDataWrapper::extractDataSamples(const MqttMsgDescriptor& msgDescriptor, const std::string& json)
{
    using ValueVariant = std::variant<int64_t, double, std::string>;
    ValueVariant value{};
    std::vector<DataPackets> outputData;
    uint64_t ts = 0;
    bool hasTS = false;
    bool hasValue = false;
    CmdResult result(true);
    try
    {
        rapidjson::Document jsonDocument;
        jsonDocument.Parse(json);
        if (jsonDocument.HasParseError())
        {
            return std::pair{CmdResult(false, "Error parsing mqtt payload as JSON"), outputData};
        }

        if (jsonDocument.IsObject())
        {
            for (auto it = jsonDocument.MemberBegin(); it != jsonDocument.MemberEnd(); ++it)
            {
                const std::string name = it->name.GetString();
                if (!msgDescriptor.valueFieldName.empty() && name == msgDescriptor.valueFieldName)
                {
                    const auto& v = jsonDocument[name];
                    hasValue = true;
                    if (v.IsInt64())
                        value = v.GetInt64();
                    else if (v.IsUint64())
                        value = static_cast<int64_t>(v.GetUint64());
                    else if (v.IsDouble())
                        value = v.GetDouble();
                    else if (v.IsString())
                        value = std::string(v.GetString());
                    else
                    {
                        result.success = false;
                        result.msg = fmt::format("Unsupported value type for '{}'.", name);
                        hasValue = false;
                    }
                }
                else if (!msgDescriptor.tsFieldName.empty() && name == msgDescriptor.tsFieldName)
                {
                    if (jsonDocument[name].IsInt() || jsonDocument[name].IsUint64() ||
                        jsonDocument[name].IsInt64())
                    {
                        ts = utils::numericToMicroseconds(jsonDocument[name].GetUint64());
                        hasTS = true;
                    }
                    else if (jsonDocument[name].IsString())
                    {
                        ts = utils::toUnixTicks(jsonDocument[name].GetString());
                        hasTS = true;
                    }
                    else
                    {
                        result.success = false;
                        result.msg = "Timestamp value type is not supported.";
                    }
                }
                else
                {
                    LOG_T("Field \"{}\" is not supported.", name);
                }
            }
        }
    }
    catch (...)
    {
        result.success = false;
        result.msg = "Error deserializing MQTT payload";
    }
    if (!hasValue || (!msgDescriptor.tsFieldName.empty() && !hasTS))
    {
        result.success = false;
        result.msg = "Not all required fields are present in the JSON message.";
    }

    if (result.success)
    {
        // TODO : value [1, 2, 3, ...] support
        DataPackets dataPackets;
        std::visit(
            [&](auto&& val)
            {
                using T = std::decay_t<decltype(val)>;
                if (hasTS)
                    dataPackets = buildDataPackets(val, ts);
                else
                    dataPackets = buildDataPackets(val);
            },
            value);
        if (dataPackets.dataPacket.assigned())
            outputData.push_back(std::move(dataPackets));
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
MqttDataWrapper::buildDataPackets(T value, uint64_t timestamp)
{
    DataPackets dataPackets;

    dataPackets.domainDataPacket = buildDomainDataPacket(outputSignal, timestamp);
    dataPackets.dataPacket = buildDataPacket(outputSignal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(T value)
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

template<typename T>
daq::DataPacketPtr MqttDataWrapper::buildDataPacket(daq::GenericSignalConfigPtr<> signalConfig, T value, const daq::DataPacketPtr domainPacket)
{
    const auto curType = signalConfig.getDescriptor().getSampleType();
    if (isTypeTheSame<T>(curType) == false)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            // because daq::SampleType::String is not implemented properly, we use Binary type for string data
            // daq::SampleType::BinaryData != daq::SampleType::String
            auto descriptor = DataDescriptorBuilderCopy(signalConfig.getDescriptor()).setSampleType(daq::SampleType::Binary).build();
            signalConfig.setDescriptor(descriptor);
        }
        else
        {
            auto descriptor = DataDescriptorBuilderCopy(signalConfig.getDescriptor()).setSampleType(daq::SampleTypeFromType<T>::SampleType).build();
            signalConfig.setDescriptor(descriptor);
        }
    }
    daq::DataPacketPtr  dataPacket = createEmptyDataPacket<T>(signalConfig, domainPacket, value);
    copyDataIntoPacket(dataPacket, value);
    return dataPacket;
}

template<typename T>
daq::DataPacketPtr MqttDataWrapper::createEmptyDataPacket(const daq::GenericSignalConfigPtr<> signalConfig, const daq::DataPacketPtr domainPacket, T value)
{
    daq::DataPacketPtr dataPacket;
    if constexpr (std::is_same_v<T, std::string>)
    {
        dataPacket = daq::BinaryDataPacket(domainPacket, signalConfig.getDescriptor(), value.size());
    }
    else
    {
        if (signalConfig.getDomainSignal().assigned() && domainPacket.assigned())
        {
            dataPacket = DataPacketWithDomain(domainPacket, signalConfig.getDescriptor(), 1);
        }
        else
        {
            dataPacket = DataPacket(signalConfig.getDescriptor(), 1);
        }
    }
    return dataPacket;
}

template <typename T>
void MqttDataWrapper::copyDataIntoPacket(daq::DataPacketPtr dataPacket, T value)
{
    if (!dataPacket.assigned())
        return;
    if constexpr (std::is_same_v<T, std::string>)
    {
        memcpy(dataPacket.getData(), value.c_str(), value.size());
    }
    else
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
