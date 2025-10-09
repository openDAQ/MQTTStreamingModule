#pragma once
#include <string>
#include <functional>
#include "MQTTClientType.h"

enum class MqttClientType
{
    sync,
    async
}; 

namespace mqtt
{

    enum class MqttConnectionStatus
    {
        not_connected,
        connected,
        pending,
    };

class IMqttBase
{
public:
    IMqttBase(bool enableSSL,
              bool useCertificates,
              bool verifyServerCert,
              std::string trustStorePath,
              std::string clientCertPath,
              std::string privKeyPath,
              std::string privKeyPass)
        : enableSSL(enableSSL)
        , useCertificates(useCertificates)
        , verifyServerCert(verifyServerCert)
        , trustStorePath(trustStorePath)
        , clientCertPath(clientCertPath)
        , privKeyPath(privKeyPath)
        , privKeyPass(privKeyPass)
    {
    }
    virtual bool connect() = 0;
    virtual bool reconnect() = 0;
    virtual bool disconnect() = 0;
    void setConnectionStatusCB(std::function<void(std::string who, std::string msg, int, MQTTClientType)> cb){
        connCb = cb;
    };
    void setOnConnect(std::function<void()> cb) {
        onConnectCb = cb;
    }
    virtual void setUsernamePasswrod(std::string username, std::string password) = 0;
    virtual void setServerURL(std::string serverUrl) = 0;
    virtual std::string getServerUrl() const = 0;
    virtual void setClientId(std::string clientId) = 0;
    virtual void setSSLConnectionProperties(bool enableSSL,
                                            bool useCertificates,
                                            bool verifyServerCert,
                                            std::string trustStorePath,
                                            std::string clientCertPath,
                                            std::string privKeyPath,
                                            std::string privKeyPass)
    {
        this->enableSSL = enableSSL;
        this->useCertificates = useCertificates;
        this->verifyServerCert = verifyServerCert;
        this->trustStorePath = trustStorePath;
        this->clientCertPath = clientCertPath;
        this->privKeyPath = privKeyPath;
        this->privKeyPass = privKeyPass;
    }
    virtual MqttConnectionStatus isConnected() = 0;
    virtual MqttClientType getClientType() const = 0;
    virtual ~IMqttBase()
    {
    }

protected:
    bool enableSSL;
    bool useCertificates;
    bool verifyServerCert;
    std::string trustStorePath;
    std::string clientCertPath;
    std::string privKeyPath;
    std::string privKeyPass;
    std::function<void(std::string who, std::string msg, int, MQTTClientType)> connCb;
    std::function<void()> onConnectCb;
};
}  // namespace mqtt
