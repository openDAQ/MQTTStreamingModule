#include "MqttAsyncPublisher.h"

#include <thread>

namespace mqtt {

MqttAsyncPublisher::MqttAsyncPublisher() : 
    MqttAsyncPublisher("", "", false, false, false, false, "", "", "", "")
{

}

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
    : IMqttBase(enableSSL, useCertificates, verifyServerCert, trustStorePath, clientCertPath, privKeyPath, privKeyPass)
    , penddingConnect(false)
{
    this->serverUrl = serverUrl;
    this->clientId = clientId;
    this->username = "";
    this->password = "";
    this->connOpts = MQTTAsync_connectOptions_initializer;
    this->connOpts.cleansession = cleanSession ? 1 : 0;
    this->connOpts.keepAliveInterval = 20;
    this->connOpts.connectTimeout = 5;
    this->connOpts.onSuccess = (MQTTAsync_onSuccess*) &MqttAsyncPublisher::onConnectSuccess;
    this->connOpts.onFailure = (MQTTAsync_onFailure*) &MqttAsyncPublisher::onConnectFailure;
    this->connOpts.automaticReconnect = true;
    this->connOpts.minRetryInterval = 1;
    this->connOpts.maxRetryInterval = 10;
    this->createOpts = MQTTAsync_createOptions_initializer;

#ifdef OPENDAQ_MQTT_MODULE_ENABLE_SSL
    this->ssl_opts = MQTTAsync_SSLOptions_initializer;
    this->connOpts.ssl = &this->ssl_opts;
#endif

    this->client = nullptr;
    setServerURL(serverUrl);
}

MqttAsyncPublisher::~MqttAsyncPublisher()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    if (this->client != nullptr)
    {
        disconnect();
        MQTTAsync_destroy(&this->client);
    }
}

bool MqttAsyncPublisher::connect()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    if (this->client != nullptr)
    {
        // Signal stop reconnecting
        disconnect();
        MQTTAsync_destroy(&this->client);
    }

    penddingConnect = true;
    setServerURL(this->serverUrl);
    int rc = MQTTAsync_createWithOptions(&this->client, this->serverUrl.c_str(), this->clientId.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL, &this->createOpts);

    if (rc != MQTTASYNC_SUCCESS)
    {
        return false;
    }

    rc = MQTTAsync_setCallbacks(this->client, this, &MqttAsyncPublisher::connlost, &MqttAsyncPublisher::msgArrived, &MqttAsyncPublisher::deliveryComplete);
    if (rc != MQTTASYNC_SUCCESS)
    {
        return false;
    }

    rc = MQTTAsync_setConnected(this->client, this, &MqttAsyncPublisher::connectedCb);
    if (rc != MQTTASYNC_SUCCESS)
    {
        return false;
    }

    this->connOpts.context = this;
    if ((rc = MQTTAsync_connect(this->client, &this->connOpts)) != MQTTASYNC_SUCCESS)
    {
        return false;
    }

 
    return true;
}

bool MqttAsyncPublisher::disconnect()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    return MQTTAsync_disconnect(this->client, NULL) == MQTTASYNC_SUCCESS;
}

MqttConnectionStatus MqttAsyncPublisher::isConnected()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    if (this->penddingConnect)
        return MqttConnectionStatus::pending;
    return MQTTAsync_isConnected(this->client) ? MqttConnectionStatus::connected : MqttConnectionStatus::not_connected;
}

void MqttAsyncPublisher::setUsernamePasswrod(std::string username, std::string password)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    this->username = username;
    this->password = password;

    this->connOpts.username = !username.empty() ? this->username.c_str() : NULL;
    this->connOpts.password = !password.empty() ? this->password.c_str() : NULL;
}

void MqttAsyncPublisher::publishBirthCertificates()
{
}

bool MqttAsyncPublisher::publish(const std::string& topic, void* data, size_t dataLen, std::string* err, int qos, int* token, bool retained)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    int rc;
    opts.onSuccess = (MQTTAsync_onSuccess*) &MqttAsyncPublisher::onSend;
    opts.onFailure = (MQTTAsync_onFailure*) &MqttAsyncPublisher::onSendFailure;
    opts.context = this;
    pubmsg.payload = data;
    pubmsg.payloadlen = (int) dataLen;
    pubmsg.qos = qos;
    pubmsg.retained = retained ? 1 : 0;
    if ((rc = MQTTAsync_sendMessage(client, topic.c_str(), &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
    {
        if (err != nullptr)
        {
            *err = MQTTAsync_strerror(rc);
        }
    }
    if (token != nullptr)
    {
        *token = opts.token;
    }
    return rc == MQTTASYNC_SUCCESS;
}

void MqttAsyncPublisher::setStateCB(std::function<void()> cb)
{
    this->cb = cb;
}

void MqttAsyncPublisher::setServerURL(std::string serverUrl)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    this->serverUrl = serverUrl;
    if (serverUrl[0] == ':' || serverUrl == "")
    {
        this->serverUrl = "";
        return;
    }
    std::string ssl("ssl://");
    std::string tcp("tcp://");
    // Remove any protocol if available
    if ((this->serverUrl.size() >= ssl.size() && std::equal(ssl.begin(), ssl.end(), this->serverUrl.begin())))
    {
        this->serverUrl.erase(0, ssl.length());
    }
    if ((this->serverUrl.size() >= tcp.size() && std::equal(tcp.begin(), tcp.end(), this->serverUrl.begin())))
    {
        this->serverUrl.erase(0, tcp.length());
    }

    if (enableSSL)
    {
        this->serverUrl = "ssl://" + this->serverUrl;
        this->ssl_opts.enableServerCertAuth = this->useCertificates && this->verifyServerCert && trustStorePath != "";
        if (trustStorePath != "")
        {
            this->ssl_opts.trustStore = trustStorePath.c_str();
        }
        else
        {
            this->ssl_opts.trustStore = nullptr;
        }
        if (this->useCertificates)
        {
            this->ssl_opts.keyStore = clientCertPath.c_str();
            this->ssl_opts.privateKey = privKeyPath.c_str();
        }
        else
        {
            this->ssl_opts.keyStore = nullptr;
            this->ssl_opts.privateKey = nullptr;
        }
    }
    else
    {
        this->serverUrl = "tcp://" + this->serverUrl;
    }
}

void MqttAsyncPublisher::setClientId(std::string clientId)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    this->clientId = clientId;
}
bool MqttAsyncPublisher::reconnect()
{
    return false;
}
}
