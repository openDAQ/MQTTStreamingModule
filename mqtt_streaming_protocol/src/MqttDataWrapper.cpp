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
static const char* DEVICE_SIGNAL_LIST = "$signals";

MqttDataWrapper::MqttDataWrapper(daq::LoggerComponentPtr loggerComponent)
    : loggerComponent(loggerComponent)
{
}

std::string MqttDataWrapper::serializeSampleData(const SampleData& data)
{
    std::string result;

    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("value", rapidjson::Value(data.value), doc.GetAllocator());
    doc.AddMember("timestamp", rapidjson::Value(data.timestamp), doc.GetAllocator());

    // Serialize to string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    result = buffer.GetString();

    return result;
}

std::string MqttDataWrapper::serializeSignalDescriptors(
    daq::ListObjectPtr<daq::IList, daq::ISignal, daq::GenericSignalPtr<daq::ISignal>> signals)
{
    std::string result;
    rapidjson::Document doc;
    doc.SetObject();

    auto& alc = doc.GetAllocator();

    for (const auto& signal : signals)
    {

        rapidjson::Value signalValueObj(rapidjson::kObjectType);
        signalValueObj.AddMember("Value", rapidjson::Value("value", alc), alc);
        signalValueObj.AddMember("Timestamp", rapidjson::Value("timestamp", alc), alc);
        // unit
        rapidjson::Value unitArray(rapidjson::kArrayType);
        {
            auto unit = signal.getDescriptor().getUnit();
            if (unit.assigned())
            {
                auto addUnitInfo = [&unitArray, &alc](daq::StringPtr unitInfo)
                {
                    if (unitInfo.assigned())
                        unitArray.PushBack(rapidjson::Value(unitInfo.toStdString().c_str(), alc),
                                           alc);
                };
                addUnitInfo(unit.getSymbol());
                addUnitInfo(unit.getName());
                addUnitInfo(unit.getQuantity());
            }
        }
        signalValueObj.AddMember("Unit", unitArray, alc);

        rapidjson::Value signalObj(rapidjson::kObjectType);
        signalObj.AddMember(rapidjson::Value(signal.getName().toStdString().c_str(), alc),
                            signalValueObj,
                            alc);
        rapidjson::Value topicArray(rapidjson::kArrayType);
        topicArray.PushBack(signalObj, alc);

        doc.AddMember(rapidjson::Value(buildTopicFromId(signal.getGlobalId().toStdString()).c_str(),
                                       alc),
                      topicArray,
                      alc);
    }

    // Serialize to string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    result = buffer.GetString();
    return result;
}

std::string MqttDataWrapper::extractDeviceName(const std::string& topic)
{

    std::vector<std::string> list;
    boost::split(list, topic, boost::is_any_of("/"));

    if (list.size() != 3 || list[0] != TOPIC_ALL_SIGNALS_PREFIX || list[2] != DEVICE_SIGNAL_LIST)
    {
        return ""; // not a signal list message
    }

    return list[1];
}

std::string MqttDataWrapper::buildTopicFromId(const std::string& globalId)
{
    return (TOPIC_ALL_SIGNALS_PREFIX + globalId);
}

std::string MqttDataWrapper::buildSignalsTopic(const std::string& deviceId)
{
    return (TOPIC_ALL_SIGNALS_PREFIX + deviceId + "/" + DEVICE_SIGNAL_LIST);
}

bool MqttDataWrapper::validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent)
{
    if (!topic.assigned() || topic.getLength() == 0)
    {
        if (loggerComponent.assigned())
            LOG_W("Empty topic is not allowed!");
        return false;
    }

    std::vector<std::string> list;
    boost::split(list, topic.toStdString(), boost::is_any_of("/"));

    for (const auto& part : list)
    {
        if (part == "#" || part == "+")
        {
            if (loggerComponent.assigned())
                LOG_W("Wildcard characters '+' and '#' are not allowed in topic: {}", topic.toStdString());
            return false;
        }
    }

    return true;
}

void MqttDataWrapper::setConfig(const std::string& config)
{
    this->config = config;
}

std::unordered_map<mqtt::SignalId, daq::DataDescriptorPtr> MqttDataWrapper::extractDescription()
{
    std::unordered_map<mqtt::SignalId, daq::DataDescriptorPtr> result;
    rapidjson::Document doc;
    topicDescriptors.clear();

    if (config.empty())
    {
        LOG_E("The JSON config is empty");
        return result;
    }

    if (doc.Parse(config.c_str()).HasParseError())
    {
        LOG_E("The JSON config has wrong format");
        return result;
    }

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
    {
        const rapidjson::Value& array = it->value;
        const std::string topic = it->name.GetString();
        if (!validateTopic(topic, loggerComponent))
            continue;
        if (!array.IsArray())
        {
            LOG_W("Wrong description for \"{}\" topic. Skip!", topic);
            continue;
        }

        std::vector<MqttMsgDescriptor> msgDescriptors;
        // Each topic array contains one or more signal objects
        for (const auto& elem : array.GetArray())
        {
            if (!elem.IsObject())
                continue;



            // Iterate over signals inside this element
            for (auto sigIt = elem.MemberBegin(); sigIt != elem.MemberEnd(); ++sigIt)
            {
                mqtt::SignalId SignalId{.topic = topic, .signalName = sigIt->name.GetString()};

                const rapidjson::Value& signalObj = sigIt->value;

                auto unit = extractSignalUnit(signalObj);
                std::string valueFieldName = extractValueFieldName(signalObj);
                std::string tsFieldName = extractTimestampFieldName(signalObj);

                msgDescriptors.emplace_back(MqttMsgDescriptor{SignalId.signalName,
                                                              std::move(valueFieldName),
                                                              std::move(tsFieldName)});

                auto dataDescBdr =
                    daq::DataDescriptorBuilder().setSampleType(daq::SampleType::Float64);
                if (unit.assigned())
                    dataDescBdr.setUnit(unit);

                result.emplace(std::pair(std::move(SignalId), dataDescBdr.build()));
            }
        }
        topicDescriptors.emplace(std::pair(topic, std::move(msgDescriptors)));
    }
    return result;
}

void MqttDataWrapper::setOutputSignals(
    std::unordered_map<SignalId, daq::SignalConfigPtr>* const outputSignals)
{
    this->outputSignals = outputSignals;
}

void MqttDataWrapper::createAndSendDataPacket(const std::string& topic, const std::string& json)
{
    auto msgDescriptors = topicDescriptors.find(topic);
    if (msgDescriptors != topicDescriptors.end())
    {
        for (const auto& dsc : msgDescriptors->second)
        {
            auto packets = extractDataSamples(topic, dsc, json);
            for (const auto& [signalId, data] : packets)
            {
                sendDataSamples(signalId, data);
            }
        }
    }
}

bool MqttDataWrapper::hasDomainSignal(const SignalId& signalId) const
{
    auto it = topicDescriptors.find(signalId.topic);
    if (it != topicDescriptors.end())
    {

        for (const auto& desc : it->second)
        {
            if (desc.signalName == signalId.signalName)
            {
                return !desc.tsFieldName.empty();
            }
        }
    }
    return false;
}

std::vector<std::pair<SignalId, DataPackets>> MqttDataWrapper::extractDataSamples(
    const std::string& topic, const MqttMsgDescriptor& msgDescriptor, const std::string& json)
{
    using ValueVariant = std::variant<int64_t, double, std::string>;
    ValueVariant value{};
    std::vector<std::pair<SignalId, DataPackets>> res;
    uint64_t ts = 0;
    bool hasTS = false;
    bool hasValue = false;
    try
    {
        rapidjson::Document jsonDocument;
        jsonDocument.Parse(json);
        if (jsonDocument.HasParseError())
        {
            LOG_E("Error parsing mqtt payload as JSON");
            return res;
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
                        hasValue = false;
                        LOG_W("Unsupported value type for '{}'.", name);
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
                        LOG_W("Value is not supported.");
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
        LOG_E("Error deserializing mqtt payload");
    }

    if (!hasValue)
    {
        LOG_W("Not all required fields are present.");
    }
    else if (!msgDescriptor.tsFieldName.empty() && hasTS == false)
    {
        LOG_W("Timestamp field is expected but missing.");
    }
    else
    {
        // TODO : value [1, 2, 3, ...] support
        SignalId signalId{topic, msgDescriptor.signalName};
        DataPackets dataPackets;
        std::visit(
            [&](auto&& val)
            {
                using T = std::decay_t<decltype(val)>;
                if (hasTS)
                    dataPackets = buildDataPackets(signalId, val, ts);
                else
                    dataPackets = buildDataPackets(signalId, val);
            },
            value);
        if (dataPackets.dataPacket.assigned())
            res.emplace_back(std::move(signalId), std::move(dataPackets));
    }
    return res;
}

void MqttDataWrapper::sendDataSamples(const SignalId& signalId, const DataPackets& dataPackets)
{
    const auto signalIter = outputSignals->find(signalId);
    if (signalIter == outputSignals->end())
    {
        return;
    }

    auto signal = signalIter->second;
    auto domainSignal = signal.getDomainSignal();

    signal.sendPacket(dataPackets.dataPacket);
    if (domainSignal.assigned() && dataPackets.domainDataPacket.assigned())
        signal.getDomainSignal().asPtr<daq::ISignalConfig>().sendPacket(
            dataPackets.domainDataPacket);
}
template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(const SignalId& signalId, T value, uint64_t timestamp)
{
    DataPackets dataPackets;
    const auto signalIter = outputSignals->find(signalId);
    if (signalIter == outputSignals->end())
    {
        return dataPackets;
    }

    auto signal = signalIter->second;

    dataPackets.domainDataPacket = buildDomainDataPacket(signal, timestamp);
    dataPackets.dataPacket = buildDataPacket(signal, value, dataPackets.domainDataPacket);

    return dataPackets;
}

template <typename T>
DataPackets
MqttDataWrapper::buildDataPackets(const SignalId& signalId, T value)
{
    DataPackets dataPackets;
    const auto signalIter = outputSignals->find(signalId);
    if (signalIter == outputSignals->end())
    {
        return dataPackets;
    }

    auto signal = signalIter->second;
    dataPackets.dataPacket = buildDataPacket(signal, value, daq::DataPacketPtr());

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
