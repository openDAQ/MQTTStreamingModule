#pragma once

#include <mqtt_streaming_module/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/input_port_config_ptr.h>
#include <opendaq/signal_config_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

struct MqttDataSample {
    SignalConfigPtr previewSignal;
    std::string topic;
    std::string message;
};

using MqttData = std::vector<MqttDataSample>;

enum class TopicMode {
    PerSignal = 0,
    Single,
    _count
};

enum class SignalValueJSONKey {
    GlobalID = 0,
    LocalID,
    Name,
    _count
};

struct PublisherFbConfig {
    TopicMode topicMode;
    std::string topicName;
    bool sharedTs;
    bool groupValues;
    SignalValueJSONKey valueFieldName;
    size_t groupValuesPackSize;
    int qos;
    int periodMs;
    bool enablePreview;
};

struct SignalContext
{
    size_t index;
    InputPortConfigPtr inputPort;
    std::vector<DataPacketPtr> data;
    SignalConfigPtr previewSignal;
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
