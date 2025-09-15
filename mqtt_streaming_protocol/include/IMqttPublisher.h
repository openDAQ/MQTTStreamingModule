#pragma once
#include "IMqttBase.h"
#include <functional>

namespace mqtt
{
class IMqttPublisher : public virtual IMqttBase
{
public:
    virtual void publishBirthCertificates() = 0;
    virtual bool publish(const std::string& topic, void* data, size_t dataLen, std::string* err = nullptr, int qos = 1, int *token = nullptr, bool retained = false) = 0;
    virtual void setStateCB(std::function<void()> cb) = 0;
    virtual void setPublishSuccessCB(std::function<void(int)> cb)
    {
        this->sentSuccessCb = cb;
    }

    virtual void setPublishFailCB(std::function<void(int)> cb)
    {
        this->sentFailCb = cb;
    }

    virtual ~IMqttPublisher()
    {
    }

protected:
    std::function<void()> cb;
    std::function<void(int)> sentSuccessCb;
    std::function<void(int)> sentFailCb;
};
}  // namespace mqtt
