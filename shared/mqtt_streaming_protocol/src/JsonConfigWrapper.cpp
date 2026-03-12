#include "JsonConfigWrapper.h"

#include <boost/algorithm/string.hpp>
#include <coreobjects/unit_factory.h>
#include <opendaq/binary_data_packet_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/sample_type_traits.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

namespace mqtt
{

JsonConfigWrapper::JsonConfigWrapper(const std::string& config)
    : config(config)
{
    doc.Parse(config.c_str());
}

CmdResult JsonConfigWrapper::validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent)
{

    CmdResult result(true, "");
    if (!topic.assigned() || topic.getLength() == 0)
    {
        result = CmdResult(false, "Empty topic is not allowed!");
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
            result = CmdResult(false, fmt::format("Wildcard characters '+' and '#' are not allowed in topic: {}", topic.toStdString()));
            if (loggerComponent.assigned())
            {
                LOG_W("{}", result.msg);
            }
            return result;
        }
    }

    return result;
}

std::vector<std::pair<std::string, MqttMsgDescriptor>> JsonConfigWrapper::extractDescription()
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

std::string JsonConfigWrapper::extractTopic()
{
    std::string topic;
    if (config.empty() || !isJsonValid().success)
        return topic;

    topic = doc.MemberBegin()->name.GetString();
    return topic;
}

CmdResult JsonConfigWrapper::isJsonValid()
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

daq::UnitPtr JsonConfigWrapper::extractSignalUnit(const rapidjson::Value& signalObj)
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

std::string JsonConfigWrapper::extractValueFieldName(const rapidjson::Value& signalObj)
{
    return extractFieldName(signalObj, "Value");
}

std::string JsonConfigWrapper::extractTimestampFieldName(const rapidjson::Value& signalObj)
{
    return extractFieldName(signalObj, "Timestamp");
}

std::string JsonConfigWrapper::extractFieldName(const rapidjson::Value& signalObj, const std::string& field)
{
    return (signalObj.HasMember(field) && signalObj[field].IsString()) ? signalObj[field].GetString() : "";
}
} // namespace mqtt
