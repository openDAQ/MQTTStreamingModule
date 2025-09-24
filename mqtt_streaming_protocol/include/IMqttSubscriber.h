#pragma once
#include "IMqttBase.h"
#include "MqttMessage.h"
#include <functional>

namespace mqtt
{
class IMqttSubscriber : public virtual IMqttBase
{
public:
    virtual bool subscribe(std::string topic, int qos) = 0;
    virtual bool unsubscribe(std::string topic) = 0;
    virtual bool unsubscribeAll() = 0;
    virtual void setMessageArrivedCb(std::string topic, std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) = 0;
    virtual void setMessageArrivedCb(std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) = 0;
    virtual ~IMqttSubscriber() = default;

protected:
    std::unordered_map<std::string, std::function<void(const IMqttSubscriber&, mqtt::MqttMessage& msg)>> onMsgArrivedCbs;
    std::function<void(const IMqttSubscriber&, mqtt::MqttMessage& msg)> onMsgArrivedCmnCb;
};
}  // namespace mqtt
