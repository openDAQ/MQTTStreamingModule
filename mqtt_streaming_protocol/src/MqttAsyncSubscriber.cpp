#include "MqttAsyncSubscriber.h"

namespace mqtt {

MqttAsyncSubscriber::MqttAsyncSubscriber()
    : MqttAsyncSubscriber("", "", false, false, false, false, "", "", "", "")
{}

MqttAsyncSubscriber::MqttAsyncSubscriber(std::string serverUrl,
                                         std::string clientId,
                                         bool cleanSession,
                                         bool enableSSL,
                                         bool useCertificates,
                                         bool verifyServerCert,
                                         std::string trustStorePath,
                                         std::string clientCertPath,
                                         std::string privKeyPath,
                                         std::string privKeyPass)
    : IMqttBase(enableSSL,
                useCertificates,
                verifyServerCert,
                trustStorePath,
                clientCertPath,
                privKeyPath,
                privKeyPass)
    , clientId(clientId)
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
    connOpts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncSubscriber::onConnectSuccess;
    connOpts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncSubscriber::onConnectFailure;
    connOpts.automaticReconnect = true;
    connOpts.minRetryInterval = 1;
    connOpts.maxRetryInterval = 10;
    connOpts.ssl = &sslOpts;
    connOpts.context = this;
    createOpts = MQTTAsync_createOptions_initializer;
    sslOpts = MQTTAsync_SSLOptions_initializer;
}

MqttAsyncSubscriber::~MqttAsyncSubscriber() {
    unsubscribeAll();
    if (client != nullptr) {
        disconnect();
        MQTTAsync_destroy(&client);
    }
}

bool MqttAsyncSubscriber::connect() {
    if (client != nullptr) {
        // Signal stop reconnecting
        disconnect();
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
                                &MqttAsyncSubscriber::onConnectionLost,
                                &MqttAsyncSubscriber::onMsgArrived,
                                &MqttAsyncSubscriber::onDeliveryCompleted);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_setConnected(client, this, &MqttAsyncSubscriber::onConnected);
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

bool MqttAsyncSubscriber::disconnect() {
    return MQTTAsync_disconnect(client, NULL) == MQTTASYNC_SUCCESS;
}

MqttConnectionStatus MqttAsyncSubscriber::isConnected() {
    if (pendingConnect)
        return MqttConnectionStatus::pending;

    return MQTTAsync_isConnected(client) ? MqttConnectionStatus::connected
                                         : MqttConnectionStatus::not_connected;
}

void MqttAsyncSubscriber::setServerURL(std::string serverUrl) {
    if (serverUrl[0] == ':' || serverUrl == "") {
        serverUrl = "";
    } else {
        std::string ssl("ssl://");
        std::string tcp("tcp://");
        // Remove any protocol if available
        if ((serverUrl.size() >= ssl.size()
             && std::equal(ssl.begin(), ssl.end(), serverUrl.begin()))) {
            serverUrl.erase(0, ssl.length());
        }
        if ((serverUrl.size() >= tcp.size()
             && std::equal(tcp.begin(), tcp.end(), serverUrl.begin()))) {
            serverUrl.erase(0, tcp.length());
        }

        if (enableSSL) {
            serverUrl = "ssl://" + serverUrl;
            sslOpts.enableServerCertAuth = useCertificates && verifyServerCert
                                           && trustStorePath != "";
            sslOpts.trustStore = (trustStorePath != "") ? trustStorePath.c_str() : nullptr;
            sslOpts.keyStore = (useCertificates) ? clientCertPath.c_str() : nullptr;
            sslOpts.privateKey = (useCertificates) ? privKeyPath.c_str() : nullptr;
        } else {
            serverUrl = "tcp://" + serverUrl;
        }
    }
    this->serverUrl = serverUrl;
}

void MqttAsyncSubscriber::setClientId(std::string clientId) {
    this->clientId = clientId;
}

void MqttAsyncSubscriber::setUsernamePasswrod(std::string username, std::string password)
{
    this->username = username;
    this->password = password;

    connOpts.username = !username.empty() ? username.c_str() : NULL;
    connOpts.password = !password.empty() ? password.c_str() : NULL;
}

MqttClientType MqttAsyncSubscriber::getClientType() const {
    return MqttClientType::async;
}

bool MqttAsyncSubscriber::unsubscribeAll() {
    for (auto &sub : subscriptions) {
        unsubscribe(sub.topic);
    }
    subscriptions.clear();

    return true;
}

bool MqttAsyncSubscriber::subscribe(std::string topic, int qos) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc;
    opts.onSuccess = MqttAsyncSubscriber::onSubscribeSuccess;
    opts.onFailure = MqttAsyncSubscriber::onSubscribeFailure;
    opts.context = this;
    if ((rc = MQTTAsync_subscribe(client, topic.c_str(), qos, &opts)) == MQTTASYNC_SUCCESS) {
        subscriptions.emplace_back(topic, qos);
    }
    return rc == MQTTASYNC_SUCCESS;
}

bool MqttAsyncSubscriber::unsubscribe(std::string topic) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = MqttAsyncSubscriber::onSubscribeSuccess;
    opts.onFailure = MqttAsyncSubscriber::onSubscribeFailure;
    opts.context = this;
    int rc = MQTTAsync_unsubscribe(client, topic.c_str(), &opts);
    auto it = std::find_if(subscriptions.begin(),
                           subscriptions.end(),
                           [&topic](const MqttSubscription &sub) { return sub.topic == topic; });
    if (it != subscriptions.end())
        subscriptions.erase(it);
    return rc == MQTTASYNC_SUCCESS;
}

void MqttAsyncSubscriber::setMessageArrivedCb(
    std::function<void(const IMqttSubscriber &, mqtt::MqttMessage &)> cb)
{
    auto lock = getCbLock();
    onMsgArrivedCmnCb = cb;
}

void MqttAsyncSubscriber::setMessageArrivedCb(
    std::string topic, std::function<void(const IMqttSubscriber &, mqtt::MqttMessage &)> cb)
{
    auto lock = getCbLock();
    onMsgArrivedCbs.insert({topic, cb});
}

bool MqttAsyncSubscriber::reconnect() { return true; }

std::string MqttAsyncSubscriber::getServerUrl() const { return serverUrl; }

void MqttAsyncSubscriber::onDeliveryCompleted(void *context, MQTTAsync_token token) {}

int MqttAsyncSubscriber::onMsgArrived(void *context,
                                    char *topicName,
                                    int topicLen,
                                    MQTTAsync_message *message)
{
    if (context != nullptr && message != nullptr) {
        MqttAsyncSubscriber *subscriber = (MqttAsyncSubscriber *) context;
        {
            auto lock = subscriber->getCbLock();
            auto it = (topicName != nullptr) ?
                subscriber->onMsgArrivedCbs.find(topicName)
                : subscriber->onMsgArrivedCbs.end();
            if (subscriber->onMsgArrivedCmnCb || it != subscriber->onMsgArrivedCbs.end()) {
                mqtt::MqttMessage msg;
                msg.setTopic(topicName);
                msg.addData((uint8_t *) message->payload, message->payloadlen);
                if (subscriber->onMsgArrivedCmnCb)
                    subscriber->onMsgArrivedCmnCb(*subscriber, msg);
                if (it != subscriber->onMsgArrivedCbs.end() && it->second)
                    it->second(*subscriber, msg);
            }
        }
    }
    if (message != nullptr)
        MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void MqttAsyncSubscriber::onConnected(void *context, char *cause) {
    if (context != nullptr) {
        auto subscriber = (MqttAsyncSubscriber *) context;
        subscriber->pendingConnect = false;
        auto lock = subscriber->getCbLock();
        if (subscriber->onConnectedCb)
            subscriber->onConnectedCb();
    }
}
void MqttAsyncSubscriber::onConnectionLost(void *context, char *cause) {
    // Reconnect procedure here!
}

void MqttAsyncSubscriber::onSubscribeSuccess(void *context, MQTTAsync_successData *response) {}

void MqttAsyncSubscriber::onSubscribeFailure(void *context, MQTTAsync_failureData *response) {}

void MqttAsyncSubscriber::onConnectSuccess(void *context, MQTTAsync_successData data)
{
    // This callback only fires onec when call to MQTTAsync_connect is called.
}

void MqttAsyncSubscriber::onConnectFailure(void *context, MQTTAsync_failureData data)
{
    if (context != nullptr) {
        auto subscriber = (MqttAsyncSubscriber *) context;
        subscriber->pendingConnect = false;
    }
}

} // namespace mqtt
