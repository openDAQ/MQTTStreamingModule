#pragma once

#include <mqtt_streaming_module/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/input_port_config_ptr.h>
#include <opendaq/signal_config_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

// std::vector<std::pair<topic, message>>
using MqttDataSample = std::pair<std::string, std::string>;
using MqttData = std::vector<MqttDataSample>;

enum class TopicMode {
    Single = 0,
    Multi,
    _count
};

struct PublisherFbConfig {
    TopicMode topicMode;
    std::string topicName;
    bool sharedTs;
    bool groupValues;
    bool useSignalNames;
    size_t groupValuesPackSize;
    int qos;
    int periodMs;
};

struct SignalContext
{
    size_t index;
    InputPortConfigPtr inputPort;
    std::vector<DataPacketPtr> data;
};

struct ProcedureStatus
{
    bool success;
    std::vector<std::string> messages;

    void addError(const std::string& msg)
    {
        success = false;
        messages.push_back(msg);
    }
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
