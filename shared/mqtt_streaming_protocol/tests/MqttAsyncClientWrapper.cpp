#include "MqttAsyncClientWrapper.h"
#include "MqttAsyncClient.h"
#include "mqtt_streaming_helper/timer.h"
#include <future>
#include <iostream>
#include <ostream>

using namespace std::chrono;
using namespace helper::utils;

MqttAsyncClientWrapper::MqttAsyncClientWrapper(std::string clientId)
    : instance(std::make_unique<mqtt::MqttAsyncClient>()),
      clientId(clientId) {};

MqttAsyncClientWrapper::~MqttAsyncClientWrapper()
{
    if (instance)
    {
        instance->setMessageArrivedCb(nullptr);
        auto result = instance->unsubscribe(subscribedTopics);
        if (result.success)
            result = instance->waitForCompletion(result.token, 1000);
        subscribedTopics.clear();
        instance->syncDisconnect(1000);
        instance.reset();
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

bool MqttAsyncClientWrapper::subscribe(const std::string& topic, int qos)
{
    auto status = instance->subscribe(topic, qos);
    if (status.success)
        subscribedTopics.push_back(topic);
    return status.success;
}

void MqttAsyncClientWrapper::expectMsgs(const std::string& topic,
                                        const std::vector<std::string>& msgs,
                                        std::promise<bool>& promise,
                                        std::atomic<bool>& done)
{
    instance->setMessageArrivedCb(topic,
                                  [topic, &done, &msgs, &promise, i = size_t(0)](const mqtt::MqttAsyncClient&,
                                                                         mqtt::MqttMessage& receivedMsg) mutable
                                  {
                                      const auto receivedStr = receivedMsg.toString();
                                      std::cout << "{topic | msg}: " << receivedMsg.getTopic() << " | " << receivedStr << std::endl;
                                      if (receivedMsg.getTopic() != topic || i >= msgs.size() || msgs[i] != receivedStr)
                                          return;

                                      bool expected = false;
                                      if (++i == msgs.size() && done.compare_exchange_strong(expected, true))
                                          promise.set_value(true);
                                  });
}

void MqttAsyncClientWrapper::expectMultiMsgs(const std::string& topic,
                                        const std::vector<std::string>& msgs,
                                        std::promise<bool>& promise,
                                        std::atomic<bool>& done)
{
    instance->setMessageArrivedCb(topic,
                                  [topic, &done, localMsgs = msgs, &promise](const mqtt::MqttAsyncClient&,
                                                                                 mqtt::MqttMessage& receivedMsg) mutable
                                  {
                                      const auto receivedStr = receivedMsg.toString();
                                      std::cout << "{topic | msg}: " << receivedMsg.getTopic() << " | " << receivedStr << std::endl;
                                      if (receivedMsg.getTopic() != topic || localMsgs.empty())
                                          return;
                                      if ((std::string("[") + localMsgs.at(0) + "]") == receivedStr)
                                      {
                                          localMsgs.erase(localMsgs.begin());
                                      }
                                      else if (localMsgs.size() >= 2 && (std::string("[") + localMsgs.at(1) + "]") == receivedStr)
                                      {
                                          localMsgs.erase(localMsgs.begin() + 1);
                                      }
                                      else if(localMsgs.size() >= 2 && (std::string("[") + localMsgs.at(0) + ", " + localMsgs.at(1) + "]") == receivedStr)
                                      {
                                          localMsgs.erase(localMsgs.begin(), localMsgs.begin() + 2);
                                      }
                                      else if(localMsgs.size() >= 2 && (std::string("[") + localMsgs.at(1) + ", " + localMsgs.at(0) + "]") == receivedStr)
                                      {
                                          localMsgs.erase(localMsgs.begin(), localMsgs.begin() + 2);
                                      }
                                      bool expected = false;
                                      if (localMsgs.empty() && done.compare_exchange_strong(expected, true))
                                          promise.set_value(true);
                                  });
}
