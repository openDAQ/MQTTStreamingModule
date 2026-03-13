#pragma once
#include "mqtt_streaming_protocol/MqttAsyncClient.h"
#include <future>
#include <iostream>

class MqttAsyncClientWrapper
{
public:
    MqttAsyncClientWrapper(std::string clientId = "");
    ~MqttAsyncClientWrapper();

    bool createConnection(const std::string& url, const std::string& id);
    bool connect(const std::string& url);
    bool connect(const std::string& url, const std::string& id);
    bool disconnect();
    bool removeRetainedTopic(const std::string& topic);
    bool publishMsg(const std::string& topic, const std::string& data, bool retained = false);
    bool publishMsg(const mqtt::MqttMessage& msg);
    bool subscribe(const std::string& topic, int qos);
    void expectMsgs(const std::string& topic, const std::vector<std::string>& msgs, std::promise<bool>& promise, std::atomic<bool>& done);
    void expectMultiMsgs(const std::string& topic, const std::vector<std::string>& msgs, std::promise<bool>& promise, std::atomic<bool>& done);
    void expectRawMsgsBinaryData(const std::string& topic,
                                 const std::vector<std::vector<uint8_t>>& data,
                                 std::promise<bool>& promise,
                                 std::atomic<bool>& done);

    template <typename T>
    void expectRawMsgs(const std::string& topic,
                                               const std::vector<std::pair<T, uint64_t>>& data,
                                               std::promise<bool>& promise,
                                               std::atomic<bool>& done)
    {
        instance->setMessageArrivedCb(topic,
                                      [topic, &done, &data, &promise, i = size_t(0)](const mqtt::MqttAsyncClient&,
                                                                                     mqtt::MqttMessage& receivedMsg) mutable
                                      {
                                          if (receivedMsg.getTopic() != topic || i >= data.size())
                                              return;
                                          if (sizeof(T) != receivedMsg.getData().size())
                                          {
                                              std::cerr << "Sample size and packet data size are not the same!" << std::endl;
                                              return;
                                          }
                                          if (*(reinterpret_cast<T*>(receivedMsg.getData().data())) != data[i].first)
                                          {
                                              std::cerr << "Data value and packet value are not the same!" << std::endl;
                                              return;
                                          }

                                          bool expected = false;
                                          if (++i == data.size() && done.compare_exchange_strong(expected, true))
                                              promise.set_value(true);
                                      });
    }

    std::unique_ptr<mqtt::MqttAsyncClient> instance;
    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;
    std::atomic<bool> connectedDone{false};

    int successTimeout = 5000;
    int failureTimeout = 3000;
    std::string clientId = "testMqttClientId";
protected:
    std::vector<std::string> subscribedTopics;
};
