#pragma once

#include <coreobjects/unit_ptr.h>
#include <string>

namespace mqtt
{

struct MqttMsgDescriptor
{
    std::string valueFieldName; // Value
    std::string tsFieldName;    // Timestamp
    daq::UnitPtr unit;
};

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
} // namespace mqtt::utils
