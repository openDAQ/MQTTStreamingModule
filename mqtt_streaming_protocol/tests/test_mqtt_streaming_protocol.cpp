#include "MqttAsyncPublisher.h"
#include "MqttAsyncSubscriber.h"
#include <future>
#include <gmock/gmock.h>
#include <testutils/testutils.h>

using namespace mqtt;

class MqttStreamingProtocolTest : public ::testing::TestWithParam<std::shared_ptr<IMqttBase>> {
protected:
    std::shared_ptr<IMqttBase> instance;

    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;

    int successTimeout = 5000;
    int failureTimeout = 3000;

    void SetUp() override {
        instance = GetParam();
    }

    void TearDown() override {
        instance->disconnect();
    }

    bool createConnection(std::string url, std::string id) {
        instance->setServerURL(url);
        instance->setClientId(id);

        connectedPromise = std::promise<bool>();
        connectedFuture = connectedPromise.get_future();

        instance->setOnConnected([this]() {
            connectedPromise.set_value(true);
        });
        return instance->connect();
    }
};

TEST_P(MqttStreamingProtocolTest, Connection)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
}

TEST_P(MqttStreamingProtocolTest, WrongUrlConnection)
{
    auto ok = createConnection("", "testPublisherId");
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_P(MqttStreamingProtocolTest, WrongIdConnection)
{
    auto ok = createConnection("127.0.0.1", "");
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_P(MqttStreamingProtocolTest, WrongPortConnection)
{
    auto ok = createConnection("127.0.0.1:1888", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_P(MqttStreamingProtocolTest, Connected)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);
}

TEST_P(MqttStreamingProtocolTest, Disconnection)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);

    auto disconnectionOk = instance->disconnect();
    ASSERT_TRUE(disconnectionOk);
    // It is necessary to give the client time to disconnect.
    // ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}

TEST_P(MqttStreamingProtocolTest, NotConnected)
{
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::not_connected);
}

// Instantiate with values
INSTANTIATE_TEST_SUITE_P(PublisherGroup,
                         MqttStreamingProtocolTest,
                         ::testing::Values(std::make_shared<MqttAsyncPublisher>()));

INSTANTIATE_TEST_SUITE_P(SubscriberGroup,
                         MqttStreamingProtocolTest,
                         ::testing::Values(std::make_shared<MqttAsyncSubscriber>()));
