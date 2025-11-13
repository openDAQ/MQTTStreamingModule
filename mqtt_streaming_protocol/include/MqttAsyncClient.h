#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "MQTTAsync.h"
#include "MqttMessage.h"

namespace mqtt {

enum class MqttConnectionStatus {
    not_connected,
    connected,
    pending,
};

struct CmdResult
{
    bool success = false;
    std::string msg;
    int token = 0;

    CmdResult()
        : success(false),
          msg(""),
          token(0)
    {
    }
    CmdResult(bool success)
        : success(success),
          msg(""),
          token(0)
    {
    }

    CmdResult(bool success, const std::string& msg)
        : success(success),
          msg(msg),
          token(0)
    {
    }

    CmdResult(bool success, const std::string& msg, int token)
        : success(success),
          msg(msg),
          token(token)
    {
    }
};

class MqttAsyncClient final {
public:
    using MsgArrivedCb_type = void(const MqttAsyncClient &, mqtt::MqttMessage &msg);

    MqttAsyncClient();
    MqttAsyncClient(std::string serverUrl, std::string clientId, bool cleanSession);
    MqttAsyncClient(const MqttAsyncClient &) = delete;
    MqttAsyncClient &operator=(const MqttAsyncClient &) = delete;
    MqttAsyncClient(MqttAsyncClient &&) = delete;
    MqttAsyncClient &operator=(MqttAsyncClient &&) = delete;
    ~MqttAsyncClient();

    CmdResult connect();
    CmdResult disconnect();
    bool syncDisconnect(int timeoutMs);

    CmdResult publish(const std::string &topic,
                 void *data,
                 size_t dataLen,
                 int qos = 1,
                 bool retained = false);

    CmdResult subscribe(std::string topic, int qos);
    CmdResult unsubscribe(std::string topic);
    CmdResult unsubscribe(const std::vector<std::string>& topics);
    CmdResult waitForCompletion(int token, unsigned long toutMs);

    void setConnectedCb(std::function<void()> cb);
    void setMessageArrivedCb(std::string topic, std::function<MsgArrivedCb_type> cb);
    void setMessageArrivedCb(std::vector<std::string> topics, std::function<MsgArrivedCb_type> cb);
    void setMessageArrivedCb(std::function<MsgArrivedCb_type> cb);
    void setDisconnectCb(std::function<void(bool)> cb);
    void setSentCb(std::function<void(int, bool)> cb);
    void setUnsubscribeCb(std::function<void(int, bool)> cb);
    void setDeliveryCompletedCb(std::function<void(int)> cb);

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

    std::recursive_mutex cbMtx;

    std::function<void()> onConnectedCb;
    std::function<void(int, bool)> onSentCb;
    std::function<void(int, bool)> onUnsubscribeCb;
    std::function<void(bool)> onDisconnectCb;
    std::function<void(bool)> onInternalDisconnectCb;
    std::function<void(int)> onDeliveryCompletedCb;
    std::function<MsgArrivedCb_type> onMsgArrivedCmnCb;
    std::unordered_map<std::string, std::function<MsgArrivedCb_type>> onMsgArrivedCbs;

    std::scoped_lock<std::recursive_mutex> getCbLock();

    static void onDeliveryCompleted(void *context, MQTTAsync_token token);
    static void onConnected(void *context, char *cause);
    static void onConnectionLost(void *context, char *cause);
    static int onMsgArrived(void *context,
                            char *topicName,
                            int topicLen,
                            MQTTAsync_message *message);
    static void onSendSuccess(void *context, MQTTAsync_successData *data);
    static void onSendFailure(void *context, MQTTAsync_failureData *data);
    static void onConnectSuccess(void *context, MQTTAsync_successData *data);
    static void onConnectFailure(void *context, MQTTAsync_failureData *data);
    static void onDisconnectSuccess(void *context, MQTTAsync_successData *data);
    static void onDisconnectFailure(void *context, MQTTAsync_failureData *data);
    static void onSubscribeSuccess(void *context, MQTTAsync_successData *response);
    static void onSubscribeFailure(void *context, MQTTAsync_failureData *response);
    static void onUnsubscribeSuccess(void *context, MQTTAsync_successData *response);
    static void onUnsubscribeFailure(void *context, MQTTAsync_failureData *response);
};
} // namespace mqtt
