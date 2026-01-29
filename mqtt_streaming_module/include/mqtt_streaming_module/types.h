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

struct MqttData {
    std::vector<MqttDataSample> data;
    bool needRevalidation = false;

    void merge(MqttData&& other)
    {
        data.reserve(data.size() + other.data.size());
        data.insert(data.end(), std::make_move_iterator(other.data.begin()), std::make_move_iterator(other.data.end()));
        needRevalidation = needRevalidation || other.needRevalidation;
    }
};

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
    SignalConfigPtr previewSignal;
    SignalContext(size_t index, InputPortConfigPtr inputPort, SignalConfigPtr previewSignal)
        : index(index), inputPort(inputPort), previewSignal(previewSignal)
    {
    }
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

    void merge(const ProcedureStatus& other)
    {
        success = success && other.success;
        messages.insert(messages.end(), other.messages.begin(), other.messages.end());
    }
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
