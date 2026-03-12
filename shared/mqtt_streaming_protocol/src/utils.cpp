#pragma once

#include "mqtt_streaming_protocol/utils.h"
#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

namespace mqtt::utils
{
mqtt::CmdResult validateTopic(const daq::StringPtr topic)
{
    CmdResult result(true, "");
    if (!topic.assigned() || topic.getLength() == 0)
    {
        result = CmdResult(false, "Empty topic is not allowed!");
        return result;
    }

    std::vector<std::string> list;
    boost::split(list, topic.toStdString(), boost::is_any_of("/"));

    for (const auto& part : list)
    {
        if (part == "#" || part == "+")
        {
            result = CmdResult(false, fmt::format("Wildcard characters '+' and '#' are not allowed in topic: {}", topic.toStdString()));
            return result;
        }
    }

    return result;
}
uint64_t numericToMicroseconds(uint64_t val)
{
    // Determine number of digits
    int digits = (val == 0) ? 1 : static_cast<int>(std::log10(val)) + 1;

    switch (digits)
    {
    case 10:
        return val * 1'000'000ULL; // seconds → µs
    case 13:
        return val * 1'000ULL; // milliseconds → µs
    case 16:
        return val; // microseconds → µs
    case 19:
        return val / 1'000ULL; // nanoseconds → µs
    default:
        return 0; // unsupported
    }
}

uint64_t toUnixTicks(const std::string& input)
{
    std::string str = input;
    // Trim leading/trailing spaces
    str.erase(str.begin(),
              std::find_if(str.begin(),
                           str.end(),
                           [](unsigned char c) { return !std::isspace(c); }));
    str.erase(std::find_if(str.rbegin(),
                           str.rend(),
                           [](unsigned char c) { return !std::isspace(c); })
                  .base(),
              str.end());

    if (str.empty())
        return 0; // Exception replacement

    // Check if numeric
    if (std::all_of(str.begin(), str.end(), ::isdigit))
    {
        uint64_t val = 0;
        try
        {
            val = std::stoull(str);
        }
        catch (...)
        {
            return 0;
        }
        return numericToMicroseconds(val);
    }

    // Normalize ISO 8601: replace 'T' with space, remove 'Z'
    std::replace(str.begin(), str.end(), 'T', ' ');
    if (!str.empty() && (str.back() == 'Z' || str.back() == 'z'))
        str.pop_back();

    try
    {
        // Parse string to ptime
        boost::posix_time::ptime pt =
            boost::posix_time::time_from_string(str); // format "YYYY-MM-DD HH:MM:SS[.fff]"
        boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
        boost::posix_time::time_duration diff = pt - epoch;
        return static_cast<uint64_t>(diff.total_microseconds());
    }
    catch (...)
    {
        return 0; // Exception replacement
    }
}
} // namespace mqtt::utils
