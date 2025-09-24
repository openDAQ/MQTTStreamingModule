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
    virtual void setPublishSuccessCB(std::function<void(int)> cb)
    {
        auto lock = getCbLock();
        onSentSuccessCb = cb;
    }

    virtual void setPublishFailCB(std::function<void(int)> cb)
    {
        auto lock = getCbLock();
        onSentFailCb = cb;
    }

    virtual ~IMqttPublisher() = default;

protected:
    std::function<void(int)> onSentSuccessCb;
    std::function<void(int)> onSentFailCb;
};
}  // namespace mqtt
