#pragma once

#include "IMqttSubscriber.h"

#include "MQTTAsync.h"
#include "MqttMessage.h"
#include "MQTTClientType.h"
#include <thread>
#include <iostream>
#include <mutex>
#include <atomic>

namespace mqtt
{

struct MqttSubscription {
    std::string topic;
    int qos;
};

class MqttAsyncSubscriber : public virtual IMqttSubscriber
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
                        std::string privKeyPass)
        : IMqttBase(enableSSL, useCertificates, verifyServerCert, trustStorePath, clientCertPath, privKeyPath, privKeyPass)
    {
        this->serverUrl = serverUrl;
        this->clientId = clientId;
        this->username = "";
        this->password = "";
        this->connOpts = MQTTAsync_connectOptions_initializer;
        this->connOpts.cleansession = cleanSession ? 1 : 0;
        this->connOpts.keepAliveInterval = 20;
        this->connOpts.connectTimeout = 5;
        this->connOpts.onSuccess = (MQTTAsync_onSuccess*) &MqttAsyncSubscriber::onConnectSuccess;
        this->connOpts.onFailure = (MQTTAsync_onFailure*) &MqttAsyncSubscriber::onConnectFailure;
        this->connOpts.automaticReconnect = true;
        this->connOpts.minRetryInterval = 1;
        this->connOpts.maxRetryInterval = 10;
        this->createOpts = MQTTAsync_createOptions_initializer;
        this->ssl_opts = MQTTAsync_SSLOptions_initializer;
        this->connOpts.ssl = &this->ssl_opts;

        this->willOpts = MQTTAsync_willOptions_initializer;
        this->willOpts.topicName = "STATE";
        this->willOpts.qos = 1;
        this->willOpts.retained = 1;
        this->willOpts.message = "offline";
        this->connOpts.will = &this->willOpts;
        this->client = nullptr;
        setServerURL(serverUrl);
    }

    ~MqttAsyncSubscriber()
    {
        if (this->client != nullptr)
        {
            disconnect();
            MQTTAsync_destroy(&this->client);
        }
    }

    virtual bool connect() override;
    virtual bool disconnect() override;
    virtual MqttConnectionStatus isConnected() override;
    virtual void setServerURL(std::string serverUrl) override;
    virtual void setClientId(std::string clientId) override;
    virtual void setUsernamePasswrod(std::string username, std::string password);

    virtual MqttClientType getClientType() const
    {
        return MqttClientType::async;
    }

    virtual bool unsubscribeAll() override
    {
        for (auto& sub : subscriptions)
        {
            unsubscribe(sub.topic);
        }
        subscriptions.clear();

        return true;
    }

    virtual bool subscribe(std::string topic, int qos) override
    {
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        int rc;
        opts.onSuccess = MqttAsyncSubscriber::onSubscriber;
        opts.onFailure = MqttAsyncSubscriber::onSubscriberFailure;
        opts.context = this;
        if ((rc = MQTTAsync_subscribe(client, topic.c_str(), qos, &opts)) == MQTTASYNC_SUCCESS)
        {
            MqttSubscription newSubscription = { topic, qos };
            subscriptions.emplace_back(newSubscription);
        }
        return rc == MQTTASYNC_SUCCESS;
    }

    virtual bool unsubscribe(std::string topic) override
    {
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        int rc;
        opts.onSuccess = MqttAsyncSubscriber::onSubscriber;
        opts.onFailure = MqttAsyncSubscriber::onSubscriberFailure;
        opts.context = this;
        rc = MQTTAsync_unsubscribe(this->client, topic.c_str(), &opts);
        auto it = std::find_if(subscriptions.begin(), subscriptions.end(), [&topic](MqttSubscription sub) { return sub.topic == topic; });
        if (it != subscriptions.end())
            subscriptions.erase(it);
        return rc == MQTTASYNC_SUCCESS;
    }

    virtual void setMessageArrivedCb(std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) override
    {
        auto lock = getCbLock();
        this->commonCb = cb;
    }

    virtual void setMessageArrivedCb(std::string topic, std::function<void(const IMqttSubscriber&, mqtt::MqttMessage&)> cb) override
    {
        auto lock = getCbLock();
        this->cbs.insert({topic, cb});
    }

    std::lock_guard<std::recursive_mutex> getCbLock() override
    {
        return std::lock_guard<std::recursive_mutex>(cbMtx);
    }

    virtual bool reconnect() override
    {
        return true;
    }

    std::string getClientId() const
    {
        return this->clientId;
    }

    std::string getServerUrl() const override
    {
        return this->serverUrl;
    }

private:
    std::atomic<bool> initialConnectionThreadStart = false;
    std::string serverUrl;
    std::string clientId;
    std::string username;
    std::string password;
    std::vector<MqttSubscription> subscriptions;
    std::recursive_mutex cbMtx;
    bool penddingConnect;
    const MQTTClientType type = MQTTClientType::subscriber;

    MQTTAsync client;
    MQTTAsync_connectOptions connOpts;
    MQTTAsync_createOptions createOpts;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    MQTTAsync_willOptions willOpts;

    void setPendingConnect(bool penddingConnect)
    {
        this->penddingConnect = penddingConnect;
    }

    void writeConnectionStatusMsg(std::string who, std::string msg, int level)
    {
        auto lock = getCbLock();
        if (this->connCb)
        {
            this->connCb(who, msg, level, type);
        }
    }

    // Inherited via IMqttSyncBase

    static int msgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
    {
        MqttAsyncSubscriber* subscriber = (MqttAsyncSubscriber*) context;
        {
            auto lock = subscriber->getCbLock();
            auto it = subscriber->cbs.find(topicName);
            if (subscriber->commonCb || it != subscriber->cbs.end())
            {
                mqtt::MqttMessage msg;
                // Get the topic
                msg.setTopic(topicName);
                // Copy the payload
                msg.addData((uint8_t*) message->payload, message->payloadlen);
                if (subscriber->commonCb)
                    subscriber->commonCb(*subscriber, msg);
                if(it != subscriber->cbs.end() && it->second)
                    it->second(*subscriber, msg);

            }
        }
        MQTTAsync_freeMessage(&message);
        if (topicLen > 0)
            MQTTAsync_free(topicName);
        return 1;
    }

    static void connectedCb(void* context, char* cause)
    {
        if (context != nullptr)
        {
            try
            {
                auto subscriber = (MqttAsyncSubscriber*) context;
                subscriber->setPendingConnect(false);
                auto lock = subscriber->getCbLock();
                if (subscriber->onConnectCb)
                    subscriber->onConnectCb();
            }
            catch (std::exception ex)
            {
            }
        }
    }
    static void connlost(void* context, char* cause)
    {
        // Reconnect procedure here!
        if (context != nullptr)
        {
            
        }
    }

    // Inherited via IMqttSyncSubscriber
    static void delivered(void* context, MQTTAsync_successData* rsp)
    {
    }

    static void onSubscriber(void* context, MQTTAsync_successData* response)
    {
    }

    static void onSubscriberFailure(void* context, MQTTAsync_failureData* response)
    {
    }

    static void onSend(void* context, MQTTAsync_successData data)
    {
    }

    static void onSendFailure(void* context, MQTTAsync_failureData data)
    {

    }

    static void onConnectSuccess(void* context, MQTTAsync_successData data)
    {
        // This callback only fires onec when call to MQTTAsync_connect is called.
    }

    static void onConnectFailure(void* context, MQTTAsync_failureData data)
    {
        auto subscriber = (MqttAsyncSubscriber*) context;
        subscriber->setPendingConnect(false);
    }

    static void deliveryComplete(void* context, MQTTAsync_token token)
    {
    }
};
}  // namespace mqtt
