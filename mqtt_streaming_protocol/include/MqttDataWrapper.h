#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <coretypes/list_ptr.h>
#include <opendaq/signal.h>
#include <opendaq/signal_ptr.h>

namespace mqtt {

struct SampleData {
    double value;
    uint64_t timestamp;
};

struct Result {
    bool ok;
    std::vector<std::string> msg;
};

struct SignalDescriptor {
    std::string topic;
    std::string name;
    std::vector<std::string> unit;
};

class MqttDataWrapper final {
public:
    // first = device name, second = list of signal descriptors
    using DeviceDescriptorType = std::pair<std::string, std::vector<SignalDescriptor>>;
    MqttDataWrapper() = delete;

    static std::pair<Result, SampleData> parseSampleData(const std::string& json);
    static std::pair<Result, DeviceDescriptorType> parseSignalDescriptors(const std::string& topic, const std::string& json);

    static std::string serializeSampleData(const SampleData& data);
    static std::string serializeSignalDescriptors(daq::ListObjectPtr<daq::IList, daq::ISignal, daq::GenericSignalPtr<daq::ISignal>> signals);

    static  std::string buildTopicFromId(const std::string& globalId);
    static  std::string buildSignalsTopic(const std::string& deviceId);
};
} // namespace mqtt
