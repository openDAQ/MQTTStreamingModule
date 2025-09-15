#include "MqttAsyncSubscriber.h"

namespace mqtt {
    MqttAsyncSubscriber::MqttAsyncSubscriber() :
        MqttAsyncSubscriber("", "", false, false, false, false, "", "", "", "")
    {
    }

    bool MqttAsyncSubscriber::connect()
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
    int rc = MQTTAsync_createWithOptions(
        &this->client, this->serverUrl.c_str(), this->clientId.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL, &this->createOpts);

    if (rc != MQTTASYNC_SUCCESS)
    {
        return false;
    }

    rc = MQTTAsync_setCallbacks(
        this->client, this, &MqttAsyncSubscriber::connlost, &MqttAsyncSubscriber::msgArrived, &MqttAsyncSubscriber::deliveryComplete);
    if (rc != MQTTASYNC_SUCCESS)
    {
        return false;
    }

    rc = MQTTAsync_setConnected(this->client, this, &MqttAsyncSubscriber::connectedCb);
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

bool MqttAsyncSubscriber::disconnect()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    return MQTTAsync_disconnect(this->client, NULL) == MQTTASYNC_SUCCESS;
}

MqttConnectionStatus MqttAsyncSubscriber::isConnected()
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    if (this->penddingConnect)
        return MqttConnectionStatus::pending;
    return MQTTAsync_isConnected(this->client) ? MqttConnectionStatus::connected : MqttConnectionStatus::not_connected;
}

void MqttAsyncSubscriber::setServerURL(std::string serverUrl)
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

void MqttAsyncSubscriber::setClientId(std::string clientId)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    this->clientId = clientId;
}

void MqttAsyncSubscriber::setUsernamePasswrod(std::string username, std::string password)
{
    std::lock_guard<std::recursive_mutex> guard(mtx);
    this->username = username;
    this->password = password;

    this->connOpts.username = !username.empty() ? this->username.c_str() : NULL;
    this->connOpts.password = !password.empty() ? this->password.c_str() : NULL;
}
}
