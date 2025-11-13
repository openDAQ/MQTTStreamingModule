#include "MqttAsyncClient.h"
#include <future>

namespace mqtt
{

MqttAsyncClient::MqttAsyncClient()
    : MqttAsyncClient("", "", false)
{
}

MqttAsyncClient::MqttAsyncClient(std::string serverUrl, std::string clientId, bool cleanSession)
    : clientId(clientId),
      username(""),
      password(""),
      pendingConnect(false),
      client(nullptr)
{
    setServerURL(serverUrl);
    connOpts = MQTTAsync_connectOptions_initializer;
    connOpts.cleansession = cleanSession ? 1 : 0;
    connOpts.keepAliveInterval = 20;
    connOpts.connectTimeout = 5;
    connOpts.onSuccess = (MQTTAsync_onSuccess*)&MqttAsyncClient::onConnectSuccess;
    connOpts.onFailure = (MQTTAsync_onFailure*)&MqttAsyncClient::onConnectFailure;
    connOpts.automaticReconnect = true;
    connOpts.minRetryInterval = 1;
    connOpts.maxRetryInterval = 10;
    connOpts.context = this;

    disconnOpts = MQTTAsync_disconnectOptions_initializer;
    disconnOpts.onSuccess = (MQTTAsync_onSuccess*)&MqttAsyncClient::onDisconnectSuccess;
    disconnOpts.onFailure = (MQTTAsync_onFailure*)&MqttAsyncClient::onDisconnectFailure;
    disconnOpts.context = this;

    createOpts = MQTTAsync_createOptions_initializer;
}

MqttAsyncClient::~MqttAsyncClient()
{
    if (client != nullptr)
    {
        MQTTAsync_destroy(&client);
    }
}

CmdResult MqttAsyncClient::connect()
{
    if (client != nullptr)
    {
        MQTTAsync_destroy(&client);
    }

    if (serverUrl.empty() || clientId.empty())
    {
        return CmdResult(false, "serverUrl or clientId is empty");
    }

    int rc = MQTTAsync_createWithOptions(&client, serverUrl.c_str(), clientId.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL, &createOpts);

    if (rc != MQTTASYNC_SUCCESS)
    {
        return CmdResult(false, MQTTAsync_strerror(rc));
    }

    rc = MQTTAsync_setCallbacks(client,
                                this,
                                &MqttAsyncClient::onConnectionLost,
                                &MqttAsyncClient::onMsgArrived,
                                &MqttAsyncClient::onDeliveryCompleted);

    if (rc != MQTTASYNC_SUCCESS)
    {
        return CmdResult(false, MQTTAsync_strerror(rc));
    }

    rc = MQTTAsync_setConnected(client, this, &MqttAsyncClient::onConnected);
    if (rc != MQTTASYNC_SUCCESS)
    {
        return CmdResult(false, MQTTAsync_strerror(rc));
    }

    rc = MQTTAsync_connect(client, &connOpts);
    if (rc != MQTTASYNC_SUCCESS)
    {
        return CmdResult(false, MQTTAsync_strerror(rc));
    }
    pendingConnect = true;
    return CmdResult(true);
}

CmdResult MqttAsyncClient::disconnect()
{
    if (client == nullptr)
        return CmdResult(true, "The client is null");

    // It is only the result of the request to disconnect (queuing)
    auto status = MQTTAsync_disconnect(client, &disconnOpts);
    bool result = (status == MQTTASYNC_SUCCESS || status == MQTTASYNC_DISCONNECTED);
    return CmdResult(result, MQTTAsync_strerror(status));
}

bool MqttAsyncClient::syncDisconnect(int timeoutMs)
{
    if (client == nullptr)
        return true;

    if (isConnected() != MqttConnectionStatus::not_connected)
    {
        std::atomic<bool> done{false};
        std::promise<bool> disconnectedPromise;
        auto disconnectedFuture = disconnectedPromise.get_future();
        {
            auto lock = getCbLock();
            onInternalDisconnectCb = [promise = &disconnectedPromise, &done](bool result)
            {
                bool expected = false;
                if (done.compare_exchange_strong(expected, true))
                    promise->set_value(result);
            };
        }
        if (disconnect().success)
        {
            auto status = disconnectedFuture.wait_for(std::chrono::milliseconds(timeoutMs));
        }
        {
            auto lock = getCbLock();
            onInternalDisconnectCb = nullptr;
        }
    }
    if (isConnected() == MqttConnectionStatus::not_connected)
    {
        MQTTAsync_destroy(&client);
        return true;
    }
    else
    {
        return false;
    }
}

MqttConnectionStatus MqttAsyncClient::isConnected() const
{
    if (pendingConnect)
        return MqttConnectionStatus::pending;

    return MQTTAsync_isConnected(client) ? MqttConnectionStatus::connected : MqttConnectionStatus::not_connected;
}

std::scoped_lock<std::recursive_mutex> MqttAsyncClient::getCbLock()
{
    return std::scoped_lock(cbMtx);
}

void MqttAsyncClient::setUsernamePasswrod(std::string username, std::string password)
{
    this->username = username;
    this->password = password;

    connOpts.username = !username.empty() ? username.c_str() : NULL;
    connOpts.password = !password.empty() ? password.c_str() : NULL;
}

CmdResult MqttAsyncClient::publish(const std::string& topic, void* data, size_t dataLen, int qos, bool retained)
{
    std::string tmpErr;
    if (client == nullptr)
    {
        return CmdResult(false, "MQTTAsync is nullptr");
    }

    if (topic.empty())
    {
        return CmdResult(false, "topic is empty");
    }

    if (qos > 2 || qos < 0)
    {
        return CmdResult(false, "QoS is wrong");
    }

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = (MQTTAsync_onSuccess*)&MqttAsyncClient::onSendSuccess;
    opts.onFailure = (MQTTAsync_onFailure*)&MqttAsyncClient::onSendFailure;
    opts.context = this;
    pubmsg.payload = data;
    pubmsg.payloadlen = (int)dataLen;
    pubmsg.qos = qos;
    pubmsg.retained = retained ? 1 : 0;
    int rc = MQTTAsync_sendMessage(client, topic.c_str(), &pubmsg, &opts);
    return CmdResult(rc == MQTTASYNC_SUCCESS, MQTTAsync_strerror(rc), opts.token);
}

CmdResult MqttAsyncClient::subscribe(std::string topic, int qos)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = MqttAsyncClient::onSubscribeSuccess;
    opts.onFailure = MqttAsyncClient::onSubscribeFailure;
    opts.context = this;
    int rc = MQTTAsync_subscribe(client, topic.c_str(), qos, &opts);
    return CmdResult(rc == MQTTASYNC_SUCCESS, MQTTAsync_strerror(rc), opts.token);
}

CmdResult MqttAsyncClient::unsubscribe(std::string topic)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = (MQTTAsync_onSuccess*)&MqttAsyncClient::onUnsubscribeSuccess;
    opts.onFailure = (MQTTAsync_onFailure*)&MqttAsyncClient::onUnsubscribeFailure;
    opts.context = this;
    int rc = MQTTAsync_unsubscribe(client, topic.c_str(), &opts);
    return CmdResult(rc == MQTTASYNC_SUCCESS, MQTTAsync_strerror(rc), opts.token);
}

CmdResult MqttAsyncClient::unsubscribe(const std::vector<std::string>& topics)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = (MQTTAsync_onSuccess*)&MqttAsyncClient::onUnsubscribeSuccess;
    opts.onFailure = (MQTTAsync_onFailure*)&MqttAsyncClient::onUnsubscribeFailure;
    opts.context = this;
    const char* topicArray[topics.size()];
    for (size_t i = 0; i < topics.size(); ++i)
    {
        topicArray[i] = topics[i].c_str();
    }
    int rc = MQTTAsync_unsubscribeMany(client, topics.size(), (char* const*)topicArray, &opts);
    return CmdResult(rc == MQTTASYNC_SUCCESS, MQTTAsync_strerror(rc), opts.token);
}

CmdResult MqttAsyncClient::waitForCompletion(int token, unsigned long toutMs)
{
    int rc = MQTTAsync_waitForCompletion(client, token, toutMs);
    return CmdResult(rc == MQTTASYNC_SUCCESS, MQTTAsync_strerror(rc));
}

void MqttAsyncClient::setServerURL(std::string serverUrl)
{
    if (serverUrl != "")
    {
        serverUrl = "tcp://" + serverUrl;
    }
    this->serverUrl = serverUrl;
}

void MqttAsyncClient::setClientId(std::string clientId)
{
    this->clientId = clientId;
}

std::string MqttAsyncClient::getServerUrl() const
{
    return serverUrl;
}

void MqttAsyncClient::onDeliveryCompleted(void* context, MQTTAsync_token token)
{
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onDeliveryCompletedCb)
            clienttInst->onDeliveryCompletedCb(token);
    }
}

void MqttAsyncClient::onConnected(void* context, char* cause)
{
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        clienttInst->pendingConnect = false;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onConnectedCb)
            clienttInst->onConnectedCb();
    }
}

void MqttAsyncClient::onConnectionLost(void* context, char* cause)
{
    (void)cause;
    // Reconnect procedure here!
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        clienttInst->pendingConnect = false;
    }
}

int MqttAsyncClient::onMsgArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message)
{
    if (context != nullptr && message != nullptr)
    {
        MqttAsyncClient* client = (MqttAsyncClient*)context;
        {
            auto lock = client->getCbLock();
            auto it = (topicName != nullptr) ? client->onMsgArrivedCbs.find(topicName) : client->onMsgArrivedCbs.end();
            if (client->onMsgArrivedCmnCb || it != client->onMsgArrivedCbs.end())
            {
                mqtt::MqttMessage msg;
                msg.setTopic(topicName);
                msg.addData((uint8_t*)message->payload, message->payloadlen);
                msg.setQos(message->qos);
                msg.setRetained(message->retained != 0);
                if (client->onMsgArrivedCmnCb)
                    client->onMsgArrivedCmnCb(*client, msg);
                if (it != client->onMsgArrivedCbs.end() && it->second)
                    it->second(*client, msg);
            }
        }
    }
    if (message != nullptr)
        MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void MqttAsyncClient::onSendSuccess(void* context, MQTTAsync_successData* data)
{
    if (context != nullptr && data != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onSentCb)
            clienttInst->onSentCb(data->token, true);
    }
}

void MqttAsyncClient::onSendFailure(void* context, MQTTAsync_failureData* data)
{
    if (context != nullptr && data != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onSentCb)
            clienttInst->onSentCb(data->token, false);
    }
}

void MqttAsyncClient::onConnectSuccess(void* context, MQTTAsync_successData* data)
{
    // TODO : check when this is called
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        clienttInst->pendingConnect = false;
    }
}

void MqttAsyncClient::onConnectFailure(void* context, MQTTAsync_failureData* data)
{
    // TODO : check when this is called
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        clienttInst->pendingConnect = false;
    }
}

void MqttAsyncClient::onDisconnectSuccess(void* context, MQTTAsync_successData* data)
{
    // TODO : check when this is called
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onInternalDisconnectCb)
            clienttInst->onInternalDisconnectCb(true);
        if (clienttInst->onDisconnectCb)
            clienttInst->onDisconnectCb(true);
    }
}

void MqttAsyncClient::onDisconnectFailure(void* context, MQTTAsync_failureData* data)
{
    // TODO : check when this is called
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onInternalDisconnectCb)
            clienttInst->onInternalDisconnectCb(false);
        if (clienttInst->onDisconnectCb)
            clienttInst->onDisconnectCb(false);
    }
}

void MqttAsyncClient::onSubscribeSuccess(void* context, MQTTAsync_successData* response)
{
}

void MqttAsyncClient::onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
}

void MqttAsyncClient::onUnsubscribeSuccess(void* context, MQTTAsync_successData* response)
{
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onUnsubscribeCb)
            clienttInst->onUnsubscribeCb(response->token, true);
    }
}

void MqttAsyncClient::onUnsubscribeFailure(void* context, MQTTAsync_failureData* response)
{
    if (context != nullptr)
    {
        auto clienttInst = (MqttAsyncClient*)context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onUnsubscribeCb)
            clienttInst->onUnsubscribeCb(response->token, false);
    }
}

void MqttAsyncClient::setConnectedCb(std::function<void()> cb)
{
    auto lock = getCbLock();
    onConnectedCb = cb;
}

void MqttAsyncClient::setMessageArrivedCb(std::string topic, std::function<MsgArrivedCb_type> cb)
{
    auto lock = getCbLock();
    if (!cb)
        onMsgArrivedCbs.erase(topic);
    else
        onMsgArrivedCbs.insert({topic, cb});
}

void MqttAsyncClient::setMessageArrivedCb(std::vector<std::string> topics, std::function<MsgArrivedCb_type> cb)
{
    auto lock = getCbLock();
    for (const auto& topic : topics)
    {
        if (!cb)
            onMsgArrivedCbs.erase(topic);
        else
            onMsgArrivedCbs.insert({topic, cb});
    }
}

void MqttAsyncClient::setMessageArrivedCb(std::function<MsgArrivedCb_type> cb)
{
    auto lock = getCbLock();
    onMsgArrivedCmnCb = cb;
}

void MqttAsyncClient::setDisconnectCb(std::function<void(bool)> cb)
{
    auto lock = getCbLock();
    onDisconnectCb = cb;
}

void MqttAsyncClient::setSentCb(std::function<void(int, bool)> cb)
{
    auto lock = getCbLock();
    onSentCb = cb;
}

void MqttAsyncClient::setUnsubscribeCb(std::function<void(int, bool)> cb)
{
    auto lock = getCbLock();
    onUnsubscribeCb = cb;
}

void MqttAsyncClient::setDeliveryCompletedCb(std::function<void(int)> cb)
{
    auto lock = getCbLock();
    onDeliveryCompletedCb = cb;
}
} // namespace mqtt
