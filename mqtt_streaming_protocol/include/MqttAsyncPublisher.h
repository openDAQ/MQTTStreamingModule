#pragma once
#include "IMqttPublisher.h"

#include "MQTTAsync.h"
#include "MqttMessage.h"
#include "MQTTClientType.h"
#include <thread>
#include <mutex>
#include <algorithm>

namespace mqtt
{
class MqttAsyncPublisher : public virtual IMqttPublisher
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
    // Inherited via IMqttSyncPublisher
    virtual bool connect() override;
    virtual bool disconnect() override;
    virtual MqttConnectionStatus isConnected() override;
    virtual void setUsernamePasswrod(std::string username, std::string password) override;
    virtual void publishBirthCertificates();
    virtual bool publish(const std::string& topic, void* data, size_t dataLen, std::string* err = nullptr, int qos = 1, int* token = nullptr, bool retained = false) override;
    virtual void setStateCB(std::function<void()> cb) override;
    virtual void setServerURL(std::string serverUrl) override;
    virtual void setClientId(std::string clientId) override;
    virtual bool reconnect() override;

    virtual MqttClientType getClientType() const
    {
        return MqttClientType::async;
    }

    std::string getServerUrl() const override
    {
        return this->serverUrl;
    }

private:
    std::string serverUrl;
    std::string clientId;
    std::string username;
    std::string password;
    MQTTAsync client;
    MQTTAsync_connectOptions connOpts;
    MQTTAsync_createOptions createOpts;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    std::recursive_mutex mtx;
    bool penddingConnect;

    const MQTTClientType type = MQTTClientType::publisher;

    void setPendingConnect(bool penddingConnect)
    {
        this->penddingConnect = penddingConnect;
    }

    void writeConnectionStatusMsg(std::string who, std::string msg, int level)
    {
        if (this->connCb)
        {
            this->connCb(who, msg, level, type);
        }
    }

    static void deliveryComplete(void* context, MQTTAsync_token token)
    {

    }

    static void connectedCb(void* context, char* cause)
    {
        (void) cause;
        // Reconnect procedure here!
        if (context != nullptr)
        {
            auto publisher = (MqttAsyncPublisher*) context;
            publisher->setPendingConnect(false);

            if (publisher->onConnectCb)
                publisher->onConnectCb();

            MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
            int rc;
            opts.onSuccess = MqttAsyncPublisher::onSubscriber;
            opts.onFailure = MqttAsyncPublisher::onSubscriberFailure;
            opts.context = publisher;
            if ((rc = MQTTAsync_subscribe(publisher->client, "STATE", 1, &opts)) != MQTTASYNC_SUCCESS)
            {
            }
        }
    }

    static void connlost(void* context, char* cause)
    {
        (void) cause;
        // Reconnect procedure here!
        if (context != nullptr)
        {
            auto publisher = (MqttAsyncPublisher*) context;
            publisher->setPendingConnect(false);
        }
    }
    static int msgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
    {
        auto publisher = (MqttAsyncPublisher*) context;
        if (publisher->cb)
        {
            publisher->cb();
        }
        MQTTAsync_freeMessage(&message);
        // if (topicLen > 0)
        MQTTAsync_free(topicName);
        return 1;
    }

    // Inherited via IMqttSyncBase
    static void delivered(void* context, MQTTAsync_successData* rsp)
    {
    }

    static void onSubscriber(void* context, MQTTAsync_successData* response)
    {

    }

    static void onSubscriberFailure(void* context, MQTTAsync_failureData* response)
    {

    }

    static void onSend(void* context, MQTTAsync_successData* data)
    {
        auto publisher = (MqttAsyncPublisher*) context;
        if (publisher->sentSuccessCb)
            publisher->sentSuccessCb(data->token);
    }

    static void onSendFailure(void* context, MQTTAsync_failureData* data)
    {
        auto publisher = (MqttAsyncPublisher*) context;
        if (publisher->sentFailCb)
            publisher->sentFailCb(data->token);
    }

    static void onConnectSuccess(void * context, MQTTAsync_successData data)
    {
        
    }

    static void onConnectFailure(void* context, MQTTAsync_failureData data)
    {
        auto publisher = (MqttAsyncPublisher*) context;
        publisher->setPendingConnect(false);
    }
};
}  // namespace mqtt
