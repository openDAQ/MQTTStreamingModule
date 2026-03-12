#pragma once

#include <rapidjson/document.h>
#include <opendaq/logger_component_ptr.h>
#include <coreobjects/unit_ptr.h>
#include <coretypes/string_ptr.h>
#include "common.h"

namespace mqtt
{

class JsonConfigWrapper final
{
public:
    JsonConfigWrapper(const std::string& config);

    static CmdResult validateTopic(const daq::StringPtr topic, const daq::LoggerComponentPtr loggerComponent = nullptr);

    std::vector<std::pair<std::string, MqttMsgDescriptor>> extractDescription();
    std::string extractTopic();
    CmdResult isJsonValid();

private:
    rapidjson::Document doc;
    std::string config;

    daq::UnitPtr extractSignalUnit(const rapidjson::Value& signalObj);
    std::string extractValueFieldName(const rapidjson::Value& signalObj);
    std::string extractTimestampFieldName(const rapidjson::Value& signalObj);
    std::string extractFieldName(const rapidjson::Value& signalObj, const std::string& field);

};
} // namespace mqtt
