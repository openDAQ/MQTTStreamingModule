#include "MqttDataWrapper.h"

#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include "rapidjson/writer.h"

namespace mqtt {

static const char* TOPIC_ALL_SIGNALS_PREFIX = "openDAQ";
static const char* DEVICE_SIGNAL_LIST = "$signals";

std::pair<Result, SampleData> MqttDataWrapper::parseSampleData(const std::string &json)
{
    std::pair<Result, SampleData> res{{false, {}}, {0.0, 0}};
    Result& status = res.first;
    SampleData& data = res.second;
    try {
        rapidjson::Document jsonDocument;
        jsonDocument.Parse(json);
        if (jsonDocument.HasParseError()) {
            status.msg.emplace_back("Error parsing mqtt payload as JSON");
            return res;
        }

        if (jsonDocument.IsObject()) {

            int successCnt = 0;
            for (auto it = jsonDocument.MemberBegin(); it != jsonDocument.MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                if (name == "value") {
                    if (jsonDocument[name].IsDouble() || jsonDocument[name].IsInt() || jsonDocument[name].IsFloat()){
                        data.value = jsonDocument[name].GetDouble();
                        successCnt++;
                    } else {
                        status.msg.emplace_back("Value is not supported.");
                    }
                } else if (name == "timestamp") {
                    if (jsonDocument[name].IsInt() || jsonDocument[name].IsUint64() || jsonDocument[name].IsInt64()){
                        data.timestamp = jsonDocument[name].GetUint64();
                        successCnt++;
                    } else {
                        status.msg.emplace_back("Value is not supported.");
                    }
                } else {
                    status.msg.emplace_back(fmt::format("Field \"{}\" is not supported.", name));
                }
            }
            if (successCnt == 2) {
                status.ok = true;
            } else {
                status.msg.emplace_back("Not all required fields are present.");

            }
        }
    }
    catch (...) {
        status.msg.emplace_back("Error deserializing mqtt payload");
    }
    return res;
}

std::string MqttDataWrapper::serializeSampleData(const SampleData &data)
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
    daq::ListObjectPtr<daq::IList, daq::ISignal, daq::GenericSignalPtr<daq::ISignal> > signals)
{
    std::string result;
    rapidjson::Document doc;
    doc.SetArray();
    for (const auto& signal : signals) {
        rapidjson::Value topicValue;
        topicValue.SetString(buildTopicFromId(signal.getGlobalId().toStdString()).c_str(), doc.GetAllocator());
        doc.PushBack(topicValue, doc.GetAllocator());
    }

    // Serialize to string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    result = buffer.GetString();
    return result;
}

std::pair<Result, MqttDataWrapper::DeviceDescriptorType>
MqttDataWrapper::parseSignalDescriptors(const std::string& topic, const std::string &json)
{
    std::pair<Result, MqttDataWrapper::DeviceDescriptorType> res{{false, {}}, {"", {}}};
    Result& status = res.first;
    auto& [deviceName, signalDesc] = res.second;
    {
        std::vector<std::string> list;
        boost::split(list, topic, boost::is_any_of("/"));

        if (list.size() != 3
            || list[0] != TOPIC_ALL_SIGNALS_PREFIX
            || list[2] != DEVICE_SIGNAL_LIST) {
            return res;         // not a signal list message
        }

        deviceName = list[1];
    }

    rapidjson::Document doc;

    if (doc.Parse(json.c_str()).HasParseError() || !doc.IsArray()) {
        status.msg.emplace_back(fmt::format("{} - Signal list format is not correct: {}", topic, json));
        return res;
    }

    const auto array = doc.GetArray();

    signalDesc.reserve(array.Size());
    for (const auto& v : array) {
        if (v.IsString()) {
            SignalDescriptor sd;
            sd.topic = v.GetString();
            signalDesc.emplace_back(std::move(sd));
        }
    }
    status.ok = true;
    return res;
}

std::string MqttDataWrapper::buildTopicFromId(const std::string& globalId)
{
    return (TOPIC_ALL_SIGNALS_PREFIX + globalId);
}

std::string MqttDataWrapper::buildSignalsTopic(const std::string& deviceId)
{
    return (TOPIC_ALL_SIGNALS_PREFIX + deviceId + "/" + DEVICE_SIGNAL_LIST);
}
}
