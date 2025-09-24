#pragma once

#include "IMqttSubscriber.h"

#include "MQTTAsync.h"
#include "MqttMessage.h"
#include <atomic>
#include <mutex>

namespace mqtt
{

struct MqttSubscription {
    std::string topic;
    int qos;

    MqttSubscription(std::string t, int q) : topic(t), qos(q) {}
};

class MqttAsyncSubscriber final : public virtual IMqttSubscriber
{
public:
    MqttAsyncSubscriber();
    MqttAsyncSubscriber(std::string serverUrl,
                        std::string clientId,
                        bool cleanSession,
                        bool enableSSL,
                        bool useCertificates,
                        bool verifyServerCert,
                        std::string trustStorePath,
                        std::string clientCertPath,
                        std::string privKeyPath,
                        std::string privKeyPass);

    ~MqttAsyncSubscriber();

    bool connect() override;
    bool disconnect() override;
    bool reconnect() override;
    MqttConnectionStatus isConnected() override;
    void setServerURL(std::string serverUrl) override;
    void setClientId(std::string clientId) override;
    void setUsernamePasswrod(std::string username, std::string password) override;

    MqttClientType getClientType() const override;
    std::string getServerUrl() const override;

    bool subscribe(std::string topic, int qos) override;
    bool unsubscribe(std::string topic) override;
    bool unsubscribeAll() override;

    void setMessageArrivedCb(std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) override;
    void setMessageArrivedCb(std::string topic, std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) override;

private:
    std::string serverUrl;
    std::string clientId;
    std::string username;
    std::string password;
    std::vector<MqttSubscription> subscriptions;
    std::recursive_mutex cbMtx;
    std::atomic<bool> pendingConnect;

    MQTTAsync client;
    MQTTAsync_connectOptions connOpts;
    MQTTAsync_createOptions createOpts;
    MQTTAsync_SSLOptions sslOpts;

    static int onMsgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
    static void onConnected(void* context, char* cause);
    static void onConnectionLost(void* context, char* cause);
    static void onSubscribeSuccess(void* context, MQTTAsync_successData* response);
    static void onSubscribeFailure(void* context, MQTTAsync_failureData* response);
    static void onConnectSuccess(void* context, MQTTAsync_successData data);
    static void onConnectFailure(void* context, MQTTAsync_failureData data);
    static void onDeliveryCompleted(void* context, MQTTAsync_token token);
};
}  // namespace mqtt
