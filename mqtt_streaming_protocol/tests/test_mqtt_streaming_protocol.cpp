#include "MqttAsyncPublisher.h"
#include "MqttAsyncSubscriber.h"
#include <future>
#include <gmock/gmock.h>
#include <testutils/testutils.h>

using namespace mqtt;

class MqttStreamingProtocolTest : public ::testing::Test {
protected:
    MqttAsyncPublisher publisher;

    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;
    int successTimeout = 5000;
    int failureTimeout = 3000;

    bool createConnection(std::string url, std::string id) {
        publisher.setServerURL(url);
        publisher.setClientId(id);

        connectedPromise = std::promise<bool>();
        connectedFuture = connectedPromise.get_future();

        publisher.setOnConnected([this]() {
            connectedPromise.set_value(true);
        });
        return publisher.connect();
    }
};

TEST_F(MqttStreamingProtocolTest, Connection)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
}

TEST_F(MqttStreamingProtocolTest, WrongUrlConnection)
{
    auto ok = createConnection("", "testPublisherId");
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
    auto ok = createConnection("127.0.0.1:1888", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(failureTimeout));

    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, Connected)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(publisher.isConnected() == MqttConnectionStatus::connected);
}

TEST_F(MqttStreamingProtocolTest, Disconnection)
{
    auto ok = createConnection("127.0.0.1", "testPublisherId");
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(std::chrono::milliseconds(successTimeout));

    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(publisher.isConnected() == MqttConnectionStatus::connected);

    auto disconnectionOk = publisher.disconnect();
    ASSERT_TRUE(disconnectionOk);
}

TEST_F(MqttStreamingProtocolTest, NotConnected)
{
    ASSERT_TRUE(publisher.isConnected() == MqttConnectionStatus::not_connected);
}
