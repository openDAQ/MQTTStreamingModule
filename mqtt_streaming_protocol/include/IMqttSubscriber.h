#pragma once
#include "IMqttBase.h"
#include "MqttMessage.h"
#include <functional>
#include <mutex>

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
    virtual std::lock_guard<std::recursive_mutex> getCbLock() = 0;
    virtual ~IMqttSubscriber()
    {
    }

protected:
    std::unordered_map<std::string, std::function<void(const IMqttSubscriber&, mqtt::MqttMessage& msg)>> cbs;
    std::function<void(const IMqttSubscriber&, mqtt::MqttMessage& msg)> commonCb;
};
}  // namespace mqtt
