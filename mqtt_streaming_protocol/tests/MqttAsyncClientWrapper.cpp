#include "MqttAsyncClientWrapper.h"
#include "MqttAsyncClient.h"
#include "Timer.h"
#include <future>

using namespace std::chrono;

MqttAsyncClientWrapper::MqttAsyncClientWrapper(std::string clientId)
    : instance(std::make_shared<mqtt::MqttAsyncClient>()),
      clientId(clientId) {};

MqttAsyncClientWrapper::MqttAsyncClientWrapper(std::shared_ptr<mqtt::MqttAsyncClient> instance, std::string clientId)
    : instance(instance)
{
    if (!clientId.empty())
    {
        this->clientId = clientId;
    }
}

bool MqttAsyncClientWrapper::createConnection(const std::string& url, const std::string& id)
{
    instance->setServerURL(url);
    instance->setClientId(id);

    connectedDone = false;
    connectedPromise = std::promise<bool>();
    connectedFuture = connectedPromise.get_future();

    instance->setConnectedCb(
        [this]()
        {
            bool expected = false;
            if (connectedDone.compare_exchange_strong(expected, true))
            {
                connectedPromise.set_value(true);
            }
        });
    auto result = instance->connect();
    return result.success;
}

bool MqttAsyncClientWrapper::connect(const std::string& url)
{
    return connect(url, clientId);
}

bool MqttAsyncClientWrapper::connect(const std::string& url, const std::string& id)
{
    bool res = createConnection(url, id);
    if (res)
    {
        auto status = connectedFuture.wait_for(milliseconds(successTimeout));
        instance->setConnectedCb(nullptr);
        res = (status == std::future_status::ready && connectedFuture.get() == true);
    }
    return res;
}

bool MqttAsyncClientWrapper::disconnect()
{
    if (instance->isConnected() != mqtt::MqttConnectionStatus::connected)
    {
        return true;
    }
    std::atomic<bool> done{false};
    std::promise<bool> disconnectedPromise;
    auto disconnectedFuture = disconnectedPromise.get_future();
    instance->setDisconnectCb(
        [promise = &disconnectedPromise, &done](bool result)
        {
            bool expected = false;
            if (done.compare_exchange_strong(expected, true))
            {
                promise->set_value(result);
            }
        });

    auto result = instance->disconnect();
    if (!result.success)
    {
        return false;
    }

    auto status = disconnectedFuture.wait_for(milliseconds(successTimeout));
    instance->setDisconnectCb(nullptr);
    return (status == std::future_status::ready && disconnectedFuture.get() == true);
}

bool MqttAsyncClientWrapper::removeRetainedTopic(const std::string& topic)
{
    return publishMsg(topic, "", true);
}

bool MqttAsyncClientWrapper::publishMsg(const std::string& topic, const std::string& data, bool retained)
{
    const mqtt::MqttMessage msg(topic, std::vector<uint8_t>(data.begin(), data.end()), 1, retained);
    return publishMsg(msg);
}

bool MqttAsyncClientWrapper::publishMsg(const mqtt::MqttMessage& msg)
{
    std::promise<int> deliveryPromise;
    auto deliveryFuture = deliveryPromise.get_future();
    instance->setDeliveryCompletedCb([promise = &deliveryPromise](int deliveredToken) { promise->set_value(deliveredToken); });

    Timer sendTimer(successTimeout);
    auto result = instance->publish(msg.getTopic(),
                                (void*)(msg.getData().data()),
                                msg.getData().size(),
                                msg.getQos(),
                                msg.getRetained());
    if (!result.success || result.token == 0)
    {
        instance->setDeliveryCompletedCb(nullptr);
        return false;
    }

    auto status = deliveryFuture.wait_for(sendTimer.remain());
    instance->setDeliveryCompletedCb(nullptr);
    return (status == std::future_status::ready && deliveryFuture.get() == result.token);
}
