#pragma once

#include <mqtt_streaming_module/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/input_port_config_ptr.h>
#include <opendaq/signal_config_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE


class IMqttDataSample {
public:
    IMqttDataSample(){};
    IMqttDataSample(SignalConfigPtr sigConfPtr, std::string&& topic)
        : previewSignal(std::move(sigConfPtr)),
          topic(std::move(topic))
    {
    }

    virtual ~IMqttDataSample() = default;
    SignalConfigPtr getPreviewSignal() const
    {
        return previewSignal;
    }
    std::string getTopic() const
    {
        return topic;
    }
    virtual void* getDataPointer() const = 0;
    virtual size_t getDataSize() const = 0;

protected:
    SignalConfigPtr previewSignal;
    std::string topic;
};

using MqttDataSamplePtr = std::shared_ptr<IMqttDataSample>;

template<typename T>
class MqttDataSample : public IMqttDataSample {
public:
    MqttDataSample(){};
    MqttDataSample(SignalConfigPtr sigConfPtr, std::string&& topic, T&& message)
        : IMqttDataSample(std::move(sigConfPtr), std::move(topic)),
          message(std::move(message))
    {
    }

    void* getDataPointer() const override
    {
        return const_cast<void*>(reinterpret_cast<const void*>(message.data()));
    }

    size_t getDataSize() const override
    {
        return message.size();
    }

protected:
    T message;
};

struct MqttData {
    std::vector<MqttDataSamplePtr> data;
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
    InputPortConfigPtr inputPort;
    SignalConfigPtr previewSignal;
    SignalContext(size_t index, InputPortConfigPtr inputPort, SignalConfigPtr previewSignal)
        : inputPort(inputPort), previewSignal(previewSignal), index(index)
    {
    }
    size_t getIndex() const
    {
        return index;
    }
private:
    size_t index;
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
