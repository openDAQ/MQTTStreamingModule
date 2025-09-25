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

    bool connect(std::string url, std::string id) {
        bool res = createConnection(url, id);
        if (res) {
            auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));
            res = (status == std::future_status::ready && connectedFuture.get() == true);
        }
        return res;
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

    // It is necessary to give the client time to disconnect.
    std::promise<bool> disconnectedPromise;
    auto disconnectedFuture = disconnectedPromise.get_future();
    instance->setDisconnectCb([promise = &disconnectedPromise](bool result) {
        promise->set_value(result);
    });

    auto disconnectionOk = instance->disconnect();
    ASSERT_TRUE(disconnectionOk);

    status = disconnectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(disconnectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}

TEST_F(MqttStreamingProtocolTest, NotConnected)
{
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}

TEST_F(MqttStreamingProtocolTest, PublishingWithoutDataControl)
{
    auto ok = connect("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    int token = 0;
    std::promise<bool> sendPromise;
    auto sendFuture = sendPromise.get_future();
    instance->setSentCb([promise = &sendPromise, token = &token](int receivedToken, bool result) {
        if (receivedToken == *token)
            promise->set_value(result);
    });

    std::promise<bool> deliveryPromise;
    auto deliveryFuture = deliveryPromise.get_future();
    instance->setDeliveryCompletedCb([promise = &deliveryPromise, token = &token](int receivedToken) {
        if (receivedToken == *token)
            promise->set_value(true);
    });

    const std::string topic = "test/topic";
    const std::string data = "test data";

    const auto start{std::chrono::steady_clock::now()};
    ok = instance->publish(topic, (void *)(data.c_str()), data.size(), nullptr, 1, &token, false);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(token != 0);


    auto tout = std::chrono::milliseconds(successTimeout);
    auto status = sendFuture.wait_for(tout);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(sendFuture.get());

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::chrono::milliseconds newTout = (elapsed_ms >= tout) ? std::chrono::milliseconds(0)
                                                             : tout - elapsed_ms;
    status = deliveryFuture.wait_for(newTout);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(deliveryFuture.get());
}

TEST_F(MqttStreamingProtocolTest, PublishingWithoutConnection)
{
    const std::string topic = "test/topic";
    const std::string data = "test data";
    int token = 0;
    auto ok = instance->publish(topic, (void *)(data.c_str()), data.size(), nullptr, 1, &token, false);
    ASSERT_FALSE(ok);
}
