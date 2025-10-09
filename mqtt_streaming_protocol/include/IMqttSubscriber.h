#pragma once
#include "IMqttBase.h"
#include <functional>
#include "MqttMessage.h"

namespace mqtt
{
class IMqttSubscriber : public virtual IMqttBase
{
public:
    virtual bool subscribe(std::string topic, int qos) = 0;
    virtual bool unsubscribe(std::string topic) = 0;
    virtual bool unsubscribeAll() = 0;
    virtual void setMessageArrivedCb(std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) = 0;
    virtual ~IMqttSubscriber()
    {
    }

protected:
    std::function<void(const IMqttSubscriber&, mqtt::MqttMessage& msg)> cb;
};
}  // namespace mqtt
