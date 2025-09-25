#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <atomic>

#include "MQTTAsync.h"
#include "MqttMessage.h"

namespace mqtt
{

enum class MqttConnectionStatus {
    not_connected,
    connected,
    pending,
};

struct MqttSubscription {
    std::string topic;
    int qos;

    MqttSubscription(std::string t, int q) : topic(t), qos(q) {}
};

class MqttAsyncClient final
{
public:
    using MsgArrivedCb_type = void(const MqttAsyncClient&, mqtt::MqttMessage& msg);

    MqttAsyncClient();
    MqttAsyncClient(std::string serverUrl,
                       std::string clientId,
                       bool cleanSession);
    MqttAsyncClient(const MqttAsyncClient&) = delete;
    MqttAsyncClient& operator=(const MqttAsyncClient&) = delete;
    MqttAsyncClient(MqttAsyncClient&&) = delete;
    MqttAsyncClient& operator=(MqttAsyncClient&&) = delete;
    ~MqttAsyncClient();

    bool connect();
    bool disconnect();

    bool publish(const std::string &topic,
                 void *data,
                 size_t dataLen,
                 std::string *err = nullptr,
                 int qos = 1,
                 int *token = nullptr,
                 bool retained = false);

    bool subscribe(std::string topic, int qos);
    bool unsubscribe(std::string topic);
    bool unsubscribeAll();

    void setConnectedCb(std::function<void()> cb);
    void setMessageArrivedCb(std::string topic, std::function<MsgArrivedCb_type> cb);
    void setMessageArrivedCb(std::function<MsgArrivedCb_type> cb);
    void setDisconnectCb(std::function<void(bool)> cb);

    void setServerURL(std::string serverUrl);
    std::string getServerUrl() const;
    void setUsernamePasswrod(std::string username, std::string password);
    void setClientId(std::string clientId);
    MqttConnectionStatus isConnected() const;

private:
    std::string serverUrl;
    std::string clientId;
    std::string username;
    std::string password;

    std::atomic<bool> pendingConnect;

    MQTTAsync client;
    MQTTAsync_connectOptions connOpts;
    MQTTAsync_disconnectOptions disconnOpts;
    MQTTAsync_createOptions createOpts;
    MQTTAsync_SSLOptions sslOpts = MQTTAsync_SSLOptions_initializer;

    std::recursive_mutex cbMtx;

    std::function<void()> onConnectedCb;
    std::function<void(int)> onSentSuccessCb;
    std::function<void(bool)> onDisconnectCb;
    std::function<void(int)> onSentFailCb;
    std::function<MsgArrivedCb_type> onMsgArrivedCmnCb;
    std::unordered_map<std::string, std::function<MsgArrivedCb_type>> onMsgArrivedCbs;

    std::vector<MqttSubscription> subscriptions;

    std::lock_guard<std::recursive_mutex> getCbLock();

    static void onDeliveryCompleted(void* context, MQTTAsync_token token);
    static void onConnected(void* context, char* cause);
    static void onConnectionLost(void* context, char* cause);
    static int onMsgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
    static void onSendSuccess(void* context, MQTTAsync_successData* data);
    static void onSendFailure(void* context, MQTTAsync_failureData* data);
    static void onConnectSuccess(void* context, MQTTAsync_successData* data);
    static void onConnectFailure(void* context, MQTTAsync_failureData* data);
    static void onDisconnectSuccess(void* context, MQTTAsync_successData* data);
    static void onDisconnectFailure(void* context, MQTTAsync_failureData* data);
    static void onSubscribeSuccess(void* context, MQTTAsync_successData* response);
    static void onSubscribeFailure(void* context, MQTTAsync_failureData* response);
    static void onUnsubscribeSuccess(void* context, MQTTAsync_successData* response);
    static void onUnsubscribeFailure(void* context, MQTTAsync_failureData* response);

};
}  // namespace mqtt
