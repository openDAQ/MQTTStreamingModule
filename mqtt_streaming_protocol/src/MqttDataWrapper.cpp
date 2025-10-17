#include "MqttDataWrapper.h"

#include "rapidjson/writer.h"
#include <boost/algorithm/string.hpp>
#include <coreobjects/unit_factory.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/packet_factory.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <opendaq/custom_log.h>

#include <timestampConverter.h>

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

void MqttDataWrapper::setConfig(const std::string& config)
{
    this->config = config;
}

std::unordered_map<mqtt::SignalId, daq::DataDescriptorPtr> MqttDataWrapper::extractDescription()
{
    std::unordered_map<mqtt::SignalId, daq::DataDescriptorPtr> result;
    rapidjson::Document doc;
    topicDescriptors.clear();

    if (doc.Parse(config.c_str()).HasParseError())
    {
        LOG_E("The JSON config has wrong format");
        return result;
    }

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
    {
        const rapidjson::Value& array = it->value;
        const std::string topic = it->name.GetString();
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
    for (const auto& dsc : msgDescriptors->second)
    {
        auto packets = extractDataSamples(topic, dsc, json);
        for (const auto& data : packets)
        {
            sendDataSamples(data.first, data.second);
        }
    }
}

std::vector<std::pair<SignalId, DataPackets>> MqttDataWrapper::extractDataSamples(
    const std::string& topic, const MqttMsgDescriptor& msgDescriptor, const std::string& json)
{
    std::vector<std::pair<SignalId, DataPackets>> res;
    double value = 0.0;
    uint64_t ts = 0;
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

            int successCnt = 0;
            for (auto it = jsonDocument.MemberBegin(); it != jsonDocument.MemberEnd(); ++it)
            {
                const std::string name = it->name.GetString();
                if (name == msgDescriptor.valueFieldName)
                {
                    if (jsonDocument[name].IsDouble() || jsonDocument[name].IsInt() ||
                        jsonDocument[name].IsFloat())
                    {
                        value = jsonDocument[name].GetDouble();
                        successCnt++;
                    }
                    else
                    {
                        LOG_W("Value is not supported.");
                    }
                }
                else if (name == msgDescriptor.tsFieldName)
                {
                    if (jsonDocument[name].IsInt() || jsonDocument[name].IsUint64() ||
                        jsonDocument[name].IsInt64())
                    {
                        ts = utils::numericToMicroseconds(jsonDocument[name].GetUint64());
                        successCnt++;
                    }
                    else if (jsonDocument[name].IsString())
                    {
                        ts = utils::toUnixTicks(jsonDocument[name].GetString());
                        successCnt++;
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
            if (successCnt != 2)
            {
                LOG_W("Not all required fields are present.");
            }
            else
            {
                // TODO : value [1, 2, 3, ...] support
                SignalId signalId{topic, msgDescriptor.signalName};
                auto dataPackets = buildDataPackets(signalId, value, ts);
                res.emplace_back(std::move(signalId), std::move(dataPackets));
            }
        }
    }
    catch (...)
    {
        LOG_E("Error deserializing mqtt payload");
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

DataPackets
MqttDataWrapper::buildDataPackets(const SignalId& signalId, double value, uint64_t timestamp)
{
    DataPackets dataPackets;
    const auto signalIter = outputSignals->find(signalId);
    if (signalIter == outputSignals->end())
    {
        return dataPackets;
    }

    auto signal = signalIter->second;
    auto domainSignal = signal.getDomainSignal();

    if (domainSignal.assigned())
    {
        dataPackets.domainDataPacket = daq::DataPacket(signal.getDomainSignal().getDescriptor(), 1);
        std::memcpy(dataPackets.domainDataPacket.getRawData(), &timestamp, sizeof(timestamp));
        dataPackets.dataPacket =
            DataPacketWithDomain(dataPackets.domainDataPacket, signal.getDescriptor(), 1);
    }
    else
    {
        dataPackets.dataPacket = DataPacket(signal.getDescriptor(), 1);
    }

    auto outputData = reinterpret_cast<daq::Float*>(dataPackets.dataPacket.getRawData());
    *outputData = value;

    return dataPackets;
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
