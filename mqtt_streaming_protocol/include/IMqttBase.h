#pragma once

#include <functional>
#include <mutex>
#include <string>

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

    void setOnConnected(std::function<void()> cb) {
        auto lock = getCbLock();
        onConnectedCb = cb;
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

    virtual ~IMqttBase() = default;

protected:
    bool enableSSL;
    bool useCertificates;
    bool verifyServerCert;
    std::string trustStorePath;
    std::string clientCertPath;
    std::string privKeyPath;
    std::string privKeyPass;

    std::recursive_mutex cbMtx;

    std::function<void()> onConnectedCb;

    std::lock_guard<std::recursive_mutex> getCbLock() {
        return std::lock_guard<std::recursive_mutex>(cbMtx);
    }
};
}  // namespace mqtt
