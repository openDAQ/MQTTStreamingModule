#include "MqttAsyncPublisher.h"

namespace mqtt {

MqttAsyncPublisher::MqttAsyncPublisher()
    : MqttAsyncPublisher("", "", false, false, false, false, "", "", "", "")
{}

MqttAsyncPublisher::MqttAsyncPublisher(std::string serverUrl,
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
    connOpts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncPublisher::onConnectSuccess;
    connOpts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncPublisher::onConnectFailure;
    connOpts.automaticReconnect = true;
    connOpts.minRetryInterval = 1;
    connOpts.maxRetryInterval = 10;
    connOpts.ssl = &sslOpts;
    connOpts.context = this;
    createOpts = MQTTAsync_createOptions_initializer;
    sslOpts = MQTTAsync_SSLOptions_initializer;
}

MqttAsyncPublisher::~MqttAsyncPublisher() {
    if (client != nullptr) {
        disconnect();
        MQTTAsync_destroy(&client);
    }
}

bool MqttAsyncPublisher::connect() {
    if (client != nullptr) {
        // Signal stop reconnecting
        disconnect();
        MQTTAsync_destroy(&client);
    }

    pendingConnect = true;
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
                                &MqttAsyncPublisher::onConnectionLost,
                                &MqttAsyncPublisher::onMsgArrived,
                                &MqttAsyncPublisher::onDeliveryCompleted);

    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_setConnected(client, this, &MqttAsyncPublisher::onConnected);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    rc = MQTTAsync_connect(client, &connOpts);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }

    return true;
}

bool MqttAsyncPublisher::disconnect() {
    return MQTTAsync_disconnect(client, NULL) == MQTTASYNC_SUCCESS;
}

MqttConnectionStatus MqttAsyncPublisher::isConnected() {
    if (pendingConnect)
        return MqttConnectionStatus::pending;

    return MQTTAsync_isConnected(client) ? MqttConnectionStatus::connected
                                         : MqttConnectionStatus::not_connected;
}

void MqttAsyncPublisher::setUsernamePasswrod(std::string username, std::string password)
{
    this->username = username;
    this->password = password;

    connOpts.username = !username.empty() ? username.c_str() : NULL;
    connOpts.password = !password.empty() ? password.c_str() : NULL;
}

void MqttAsyncPublisher::publishBirthCertificates() {}

bool MqttAsyncPublisher::publish(const std::string &topic, void *data,
                                 size_t dataLen, std::string *err, int qos,
                                 int *token, bool retained) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    int rc;
    opts.onSuccess = (MQTTAsync_onSuccess *) &MqttAsyncPublisher::onSend;
    opts.onFailure = (MQTTAsync_onFailure *) &MqttAsyncPublisher::onSendFailure;
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

void MqttAsyncPublisher::setServerURL(std::string serverUrl) {
    if (serverUrl[0] == ':') {
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

void MqttAsyncPublisher::setClientId(std::string clientId) {
    this->clientId = clientId;
}

bool MqttAsyncPublisher::reconnect() { return false; }

MqttClientType MqttAsyncPublisher::getClientType() const {
    return MqttClientType::async;
}

std::string MqttAsyncPublisher::getServerUrl() const { return serverUrl; }

void MqttAsyncPublisher::onDeliveryCompleted(void *context, MQTTAsync_token token) {}

void MqttAsyncPublisher::onConnected(void *context, char *cause) {
    if (context != nullptr) {
        auto publisher = (MqttAsyncPublisher *) context;
        publisher->pendingConnect = false;
        auto lock = publisher->getCbLock();
        if (publisher->onConnectedCb)
            publisher->onConnectedCb();
    }
}

void MqttAsyncPublisher::onConnectionLost(void *context, char *cause) {
    (void) cause;
    // Reconnect procedure here!
    if (context != nullptr) {
        auto publisher = (MqttAsyncPublisher *) context;
        publisher->pendingConnect = false;
    }
}

int MqttAsyncPublisher::onMsgArrived(void *context,
                                     char *topicName,
                                     int topicLen,
                                     MQTTAsync_message *message)
{
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void MqttAsyncPublisher::onSend(void *context, MQTTAsync_successData *data) {
    auto publisher = (MqttAsyncPublisher *) context;
    auto lock = publisher->getCbLock();
    if (publisher->onSentSuccessCb)
        publisher->onSentSuccessCb(data->token);
}

void MqttAsyncPublisher::onSendFailure(void *context, MQTTAsync_failureData *data)
{
    auto publisher = (MqttAsyncPublisher *) context;
    auto lock = publisher->getCbLock();
    if (publisher->onSentFailCb)
        publisher->onSentFailCb(data->token);
}

void MqttAsyncPublisher::onConnectSuccess(void *context, MQTTAsync_successData data)
{
    // TODO : check when this is called
}

void MqttAsyncPublisher::onConnectFailure(void *context, MQTTAsync_failureData data)
{
    // TODO : check when this is called
    auto publisher = (MqttAsyncPublisher *) context;
    publisher->pendingConnect = false;
}
} // namespace mqtt
