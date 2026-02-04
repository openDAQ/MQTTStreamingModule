#pragma once
#include "MqttAsyncClient.h"
#include <future>

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
