#pragma once

#include <mqtt_streaming_client_module/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/input_port_config_ptr.h>
#include <opendaq/signal_config_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

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
    bool sharedTs;
    bool groupValues;
    bool useSignalNames;
};

struct SignalContext
{
    size_t index;
    InputPortConfigPtr inputPort;
};

struct ProcedureStatus
{
    bool success;
    std::vector<std::string> messages;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
