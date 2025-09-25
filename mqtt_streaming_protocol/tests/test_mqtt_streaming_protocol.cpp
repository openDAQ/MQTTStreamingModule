#include "MqttAsyncClient.h"
#include <future>
#include <gmock/gmock.h>
#include <testutils/testutils.h>

using namespace mqtt;

class MqttStreamingProtocolTest : public ::testing::Test {
protected:
    std::shared_ptr<MqttAsyncClient> instance;

    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;

    int successTimeout = 5000;
    int failureTimeout = 3000;
    std::string clientId = "testMqttClientId";

    void SetUp() override {
        instance = std::make_shared<MqttAsyncClient>();
    }

    void TearDown() override {
        instance.reset();
    }

    bool createConnection(std::string url, std::string id) {
        instance->setServerURL(url);
        instance->setClientId(id);

        connectedPromise = std::promise<bool>();
        connectedFuture = connectedPromise.get_future();

        instance->setConnectedCb([this]() {
            connectedPromise.set_value(true);
        });
        return instance->connect();
    }
};

TEST_F(MqttStreamingProtocolTest, Connection)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
}

TEST_F(MqttStreamingProtocolTest, Reconnection)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());

    ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
}

TEST_F(MqttStreamingProtocolTest, WrongUrlConnection)
{
    auto ok = createConnection("", clientId);
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, WrongIdConnection)
{
    auto ok = createConnection("127.0.0.1", "");
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, WrongPortConnection)
{
    auto ok = createConnection("127.0.0.1:1888", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, Connected)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);
}

TEST_F(MqttStreamingProtocolTest, Disconnection)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);

    auto disconnectionOk = instance->disconnect();
    ASSERT_TRUE(disconnectionOk);
    // It is necessary to give the client time to disconnect.
    std::promise<bool> disconnectedPromise;
    auto disconnectedFuture = disconnectedPromise.get_future();
    instance->setDisconnectCb([promise = &disconnectedPromise](bool result) {
        promise->set_value(result);
    });
    status = disconnectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(disconnectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}

TEST_F(MqttStreamingProtocolTest, NotConnected)
{
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}
