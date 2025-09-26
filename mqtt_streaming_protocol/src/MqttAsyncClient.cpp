#include "MqttAsyncClient.h"

namespace mqtt {

MqttAsyncClient::MqttAsyncClient() : MqttAsyncClient("", "", false) {}

MqttAsyncClient::MqttAsyncClient(std::string serverUrl, std::string clientId, bool cleanSession)
    : clientId(clientId)
    , username("")
    , password("")
    , pendingConnect(false)
    , client(nullptr)
{
    setServerURL(serverUrl);
    connOpts = MQTTAsync_connectOptions_initializer;
    connOpts.cleansession = cleanSession ? 1 : 0;
    connOpts.keepAliveInterval = 20;
    connOpts.connectTimeout = 5;
    connOpts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncClient::onConnectSuccess;
    connOpts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncClient::onConnectFailure;
    connOpts.automaticReconnect = true;
    connOpts.minRetryInterval = 1;
    connOpts.maxRetryInterval = 10;
    connOpts.ssl = &sslOpts;
    connOpts.context = this;

    disconnOpts = MQTTAsync_disconnectOptions_initializer;
    disconnOpts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncClient::onDisconnectSuccess;
    disconnOpts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncClient::onDisconnectFailure;
    disconnOpts.context = this;

    createOpts = MQTTAsync_createOptions_initializer;
    sslOpts = MQTTAsync_SSLOptions_initializer;
}

MqttAsyncClient::~MqttAsyncClient() {
    if (client != nullptr) {
        MQTTAsync_destroy(&client);
    }
}

bool MqttAsyncClient::connect() {
    if (client != nullptr) {
        MQTTAsync_destroy(&client);
    }

    if (serverUrl.empty() || clientId.empty()) {
        return false;
    }

    int rc = MQTTAsync_createWithOptions(&client,
                                         serverUrl.c_str(),
                                         clientId.c_str(),
                                         MQTTCLIENT_PERSISTENCE_NONE,
                                         NULL,
                                         &createOpts);

    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_setCallbacks(client,
                                this,
                                &MqttAsyncClient::onConnectionLost,
                                &MqttAsyncClient::onMsgArrived,
                                &MqttAsyncClient::onDeliveryCompleted);

    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_setConnected(client, this, &MqttAsyncClient::onConnected);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_connect(client, &connOpts);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }
    pendingConnect = true;
    return true;
}

bool MqttAsyncClient::disconnect() {
    // It is only the result of the request to disconnect (queuing)
    return MQTTAsync_disconnect(client, &disconnOpts) == MQTTASYNC_SUCCESS;
}

MqttConnectionStatus MqttAsyncClient::isConnected() const {
    if (pendingConnect)
        return MqttConnectionStatus::pending;

    return MQTTAsync_isConnected(client) ? MqttConnectionStatus::connected
                                         : MqttConnectionStatus::not_connected;
}

std::lock_guard<std::recursive_mutex> MqttAsyncClient::getCbLock() {
    return std::lock_guard<decltype(cbMtx)>(cbMtx);
}

void MqttAsyncClient::setUsernamePasswrod(std::string username, std::string password)
{
    this->username = username;
    this->password = password;

    connOpts.username = !username.empty() ? username.c_str() : NULL;
    connOpts.password = !password.empty() ? password.c_str() : NULL;
}

bool MqttAsyncClient::publish(const std::string &topic,
                              void *data,
                              size_t dataLen,
                              std::string *err,
                              int qos,
                              int *token,
                              bool retained)
{
    std::string tmpErr;
    if (client == nullptr) {
        tmpErr = "MQTTAsync is nullptr";
    }

    if (topic.empty()) {
        tmpErr = "topic is empty";
    }

    if (qos > 2 || qos < 0) {
        tmpErr = "QoS is wrong";
    }

    if (!tmpErr.empty()) {
        if (err != nullptr) {
            *err = std::move(tmpErr);
        }
        return false;
    }

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    int rc;
    opts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncClient::onSendSuccess;
    opts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncClient::onSendFailure;
    opts.context = this;
    pubmsg.payload = data;
    pubmsg.payloadlen = (int) dataLen;
    pubmsg.qos = qos;
    pubmsg.retained = retained ? 1 : 0;
    if ((rc = MQTTAsync_sendMessage(client, topic.c_str(), &pubmsg, &opts)) != MQTTASYNC_SUCCESS) {
        if (err != nullptr) {
            *err = MQTTAsync_strerror(rc);
        }
    }
    if (token != nullptr) {
        *token = opts.token;
    }
    return rc == MQTTASYNC_SUCCESS;
}

bool MqttAsyncClient::subscribe(std::string topic, int qos) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc;
    opts.onSuccess = MqttAsyncClient::onSubscribeSuccess;
    opts.onFailure = MqttAsyncClient::onSubscribeFailure;
    opts.context = this;
    if ((rc = MQTTAsync_subscribe(client, topic.c_str(), qos, &opts)) == MQTTASYNC_SUCCESS) {
        subscriptions.emplace_back(topic, qos);
    }
    return rc == MQTTASYNC_SUCCESS;
}

bool MqttAsyncClient::unsubscribe(std::string topic) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncClient::onUnsubscribeSuccess;
    opts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncClient::onUnsubscribeFailure;
    opts.context = this;
    int rc = MQTTAsync_unsubscribe(client, topic.c_str(), &opts);
    auto it = std::find_if(subscriptions.begin(),
                           subscriptions.end(),
                           [&topic](const MqttSubscription &sub) { return sub.topic == topic; });
    if (it != subscriptions.end())
        subscriptions.erase(it);
    return rc == MQTTASYNC_SUCCESS;
}

bool MqttAsyncClient::unsubscribeAll() {
    for (auto &sub : subscriptions) {
        unsubscribe(sub.topic);
    }
    subscriptions.clear();

    return true;
}

void MqttAsyncClient::setServerURL(std::string serverUrl) {
    if (serverUrl != "") {
        serverUrl = "tcp://" + serverUrl;
    }
    this->serverUrl = serverUrl;
}

void MqttAsyncClient::setClientId(std::string clientId) {
    this->clientId = clientId;
}

std::string MqttAsyncClient::getServerUrl() const { return serverUrl; }

void MqttAsyncClient::onDeliveryCompleted(void *context, MQTTAsync_token token)
{
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onDeliveryCompletedCb)
            clienttInst->onDeliveryCompletedCb(token);
    }
}

void MqttAsyncClient::onConnected(void *context, char *cause) {
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        clienttInst->pendingConnect = false;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onConnectedCb)
            clienttInst->onConnectedCb();
    }
}

void MqttAsyncClient::onConnectionLost(void *context, char *cause) {
    (void) cause;
    // Reconnect procedure here!
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        clienttInst->pendingConnect = false;
    }
}

int MqttAsyncClient::onMsgArrived(void *context,
                                  char *topicName,
                                  int topicLen,
                                  MQTTAsync_message *message)
{
    if (context != nullptr && message != nullptr) {
        MqttAsyncClient *client = (MqttAsyncClient *) context;
        {
            auto lock = client->getCbLock();
            auto it = (topicName != nullptr) ? client->onMsgArrivedCbs.find(topicName)
                                             : client->onMsgArrivedCbs.end();
            if (client->onMsgArrivedCmnCb || it != client->onMsgArrivedCbs.end()) {
                mqtt::MqttMessage msg;
                msg.setTopic(topicName);
                msg.addData((uint8_t *) message->payload, message->payloadlen);
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

void MqttAsyncClient::onSendSuccess(void *context, MQTTAsync_successData *data)
{
    if (context != nullptr && data != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onSentCb)
            clienttInst->onSentCb(data->token, true);
    }
}

void MqttAsyncClient::onSendFailure(void *context, MQTTAsync_failureData *data)
{
    if (context != nullptr && data != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onSentCb)
            clienttInst->onSentCb(data->token, false);
    }
}

void MqttAsyncClient::onConnectSuccess(void *context, MQTTAsync_successData *data)
{
    // TODO : check when this is called
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        clienttInst->pendingConnect = false;
    }
}

void MqttAsyncClient::onConnectFailure(void *context, MQTTAsync_failureData *data)
{
    // TODO : check when this is called
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        clienttInst->pendingConnect = false;
    }
}

void MqttAsyncClient::onDisconnectSuccess(void *context, MQTTAsync_successData *data)
{
    // TODO : check when this is called
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onDisconnectCb)
            clienttInst->onDisconnectCb(true);
    }
}

void MqttAsyncClient::onDisconnectFailure(void *context, MQTTAsync_failureData *data)
{
    // TODO : check when this is called
    if (context != nullptr) {
        auto clienttInst = (MqttAsyncClient *) context;
        auto lock = clienttInst->getCbLock();
        if (clienttInst->onDisconnectCb)
            clienttInst->onDisconnectCb(false);
    }
}

void MqttAsyncClient::onSubscribeSuccess(void *context, MQTTAsync_successData *response) {}

void MqttAsyncClient::onSubscribeFailure(void *context, MQTTAsync_failureData *response) {}

void MqttAsyncClient::onUnsubscribeSuccess(void *context, MQTTAsync_successData *response) {}

void MqttAsyncClient::onUnsubscribeFailure(void *context, MQTTAsync_failureData *response) {}

void MqttAsyncClient::setConnectedCb(std::function<void()> cb)
{
    auto lock = getCbLock();
    onConnectedCb = cb;
}

void MqttAsyncClient::setMessageArrivedCb(std::string topic, std::function<MsgArrivedCb_type> cb)
{
    auto lock = getCbLock();
    onMsgArrivedCbs.insert({topic, cb});
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

void MqttAsyncClient::setDeliveryCompletedCb(std::function<void(int)> cb)
{
    auto lock = getCbLock();
    onDeliveryCompletedCb = cb;
}
} // namespace mqtt
