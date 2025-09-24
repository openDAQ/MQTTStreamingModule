#pragma once
#include "IMqttPublisher.h"

#include "MQTTAsync.h"
#include <atomic>

namespace mqtt
{
class MqttAsyncPublisher final : public virtual IMqttPublisher
{
public:
    MqttAsyncPublisher();
    MqttAsyncPublisher(std::string serverUrl,
                       std::string clientId,
                       bool cleanSession,
                       bool enableSSL,
                       bool useCertificates,
                       bool verifyServerCert,
                       std::string trustStorePath,
                       std::string clientCertPath,
                       std::string privKeyPath,
                       std::string privKeyPass);
    ~MqttAsyncPublisher();

    bool connect() override;
    bool disconnect() override;
    bool reconnect() override;
    MqttConnectionStatus isConnected() override;
    void setServerURL(std::string serverUrl) override;
    void setClientId(std::string clientId) override;
    void setUsernamePasswrod(std::string username, std::string password) override;
    void publishBirthCertificates() override;

    bool publish(const std::string& topic, void* data, size_t dataLen, std::string* err = nullptr, int qos = 1, int* token = nullptr, bool retained = false) override;


    MqttClientType getClientType() const override;
    std::string getServerUrl() const override;

private:
    std::string serverUrl;
    std::string clientId;
    std::string username;
    std::string password;

    std::atomic<bool> pendingConnect;

    MQTTAsync client;
    MQTTAsync_connectOptions connOpts;
    MQTTAsync_createOptions createOpts;
    MQTTAsync_SSLOptions sslOpts = MQTTAsync_SSLOptions_initializer;

    static void onDeliveryCompleted(void* context, MQTTAsync_token token);
    static void onConnected(void* context, char* cause);
    static void onConnectionLost(void* context, char* cause);
    static int onMsgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
    static void onSend(void* context, MQTTAsync_successData* data);
    static void onSendFailure(void* context, MQTTAsync_failureData* data);
    static void onConnectSuccess(void * context, MQTTAsync_successData data);
    static void onConnectFailure(void* context, MQTTAsync_failureData data);
};
}  // namespace mqtt
