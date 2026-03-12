#pragma once

#include "mqtt_streaming_protocol/common.h"
#include <string>

namespace mqtt::utils
{

mqtt::CmdResult validateTopic(const daq::StringPtr topic);
uint64_t numericToMicroseconds(uint64_t val);
uint64_t toUnixTicks(const std::string& input);

} // namespace mqtt::utils
