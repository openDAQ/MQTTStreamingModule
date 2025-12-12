#pragma once
// Copyright 2022 Patrik Kokol
// Created by patrik on 16. 03. 22.
//
#include <string>
#include <vector>
#include <cstdint>

namespace mqtt
{
class MqttMessage
{
public:
    MqttMessage() = default;
    MqttMessage(std::string topic, std::vector<uint8_t> data, int qos, bool retained)
        : topic(topic)
        , data(data)
        , qos(qos)
        , retained(retained)
    {}
    bool operator==(const MqttMessage &other) const
    {
        return topic == other.topic && data == other.data && qos == other.qos;
    }

    bool operator!=(const MqttMessage &other) const
    {
        return !(*this == other);
    }

    void setToken(int token) { this->token = token; }

    int getToken()
    {
        return token;
    }

    void setTopic(const std::string& f)
    {
        this->topic = f;
    }

    bool addData(uint8_t* ptr, size_t len)
    {
        data = std::vector<uint8_t>(ptr, ptr + len);
        return true;
    }

    std::vector<uint8_t>& getData()
    {
        return data;
    }

    const std::vector<uint8_t>& getData() const
    {
        return data;
    }

    std::string getTopic() const
    {
        return topic;
    }

    int getQos() const
    {
        return qos;
    }

    void setQos(int qos)
    {
        this->qos = qos;
    }

    std::string getQueueIndexString() const
    {
        return queueIndexString;
    }

    void setQueueIndexString(const std::string& indexString)
    {
        this->queueIndexString = indexString;
    }

    void setRetained(bool retained)
    {
        this->retained = retained;
    }

    bool getRetained() const
    {
        return this->retained;
    }

    std::string toString() const
    {
        return std::string(getData().cbegin(), getData().cend());
    }

private:
    int qos = 0;
    std::string topic;
    std::vector<uint8_t> data;
    std::string queueIndexString;
    bool retained = false;
    int token;
};
}  // namespace mqtt
