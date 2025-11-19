#include "MqttAsyncClient.h"
#include "MqttAsyncClientWrapper.h"
#include "Timer.h"
#include "timestampConverter.h"
#include <future>
#include <gmock/gmock.h>
#include <testutils/testutils.h>
#include <thread>

using namespace mqtt;
using namespace std::chrono;

class MqttStreamingProtocolTest : public ::testing::Test,  public MqttAsyncClientWrapper {
protected:
    void SetUp() override {
        clientId = std::string("clientId_") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    std::string buildTopicName() {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }
};

using JsonTimestampParsingIntPTest = ::testing::TestWithParam<std::pair<uint64_t, uint64_t>>;
using JsonTimestampParsingStrPTest = ::testing::TestWithParam<std::pair<std::string, uint64_t>>;

TEST_F(MqttStreamingProtocolTest, Connection)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(Timer(successTimeout).remain());
    instance->setConnectedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
}

TEST_F(MqttStreamingProtocolTest, Reconnection)
{
    auto ok = connect("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    ok = connect("127.0.0.1", clientId);
    ASSERT_TRUE(ok);
}

TEST_F(MqttStreamingProtocolTest, WrongUrlConnection)
{
    auto ok = createConnection("", clientId);
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(Timer(failureTimeout).remain());
    instance->setConnectedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, WrongIdConnection)
{
    auto ok = createConnection("127.0.0.1", "");
    ASSERT_FALSE(ok);

    auto status = connectedFuture.wait_for(Timer(failureTimeout).remain());
    instance->setConnectedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, WrongPortConnection)
{
    auto ok = createConnection("127.0.0.1:1888", clientId);
    ASSERT_TRUE(ok);

    auto status = connectedFuture.wait_for(Timer(failureTimeout).remain());
    instance->setConnectedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::timeout);
}

TEST_F(MqttStreamingProtocolTest, Connected)
{
    auto ok = connect("127.0.0.1", clientId);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);
}

TEST_F(MqttStreamingProtocolTest, Disconnection)
{
    auto ok = createConnection("127.0.0.1", clientId);
    ASSERT_TRUE(ok);

    Timer timer(successTimeout);
    auto status = connectedFuture.wait_for(timer.remain());
    instance->setConnectedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(connectedFuture.get());
    ASSERT_TRUE(instance->isConnected() == MqttConnectionStatus::connected);

    // It is necessary to give the client time to disconnect.
    std::atomic<bool> done{false};
    std::promise<bool> disconnectedPromise;
    auto disconnectedFuture = disconnectedPromise.get_future();
    instance->setDisconnectCb([promise = &disconnectedPromise, &done](bool result) {
        bool expected = false;
        if (done.compare_exchange_strong(expected, true)) {
            promise->set_value(result);
        }
    });

    auto result = instance->disconnect();
    ASSERT_TRUE(result.success);

    status = disconnectedFuture.wait_for(timer.remain());
    instance->setDisconnectCb(nullptr);
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

    CmdResult result;
    std::atomic<bool> sendDone{false};
    std::promise<bool> sendPromise;
    auto sendFuture = sendPromise.get_future();
    instance->setSentCb([promise = &sendPromise, token = &result.token, &sendDone](int receivedToken, bool result) {
        bool expected = false;
        if (receivedToken == *token) {
            if (sendDone.compare_exchange_strong(expected, true)) {
                promise->set_value(true);
            }
        }
    });

    std::atomic<bool> deliveryDone{false};
    std::promise<bool> deliveryPromise;
    auto deliveryFuture = deliveryPromise.get_future();
    instance->setDeliveryCompletedCb(
        [promise = &deliveryPromise, token = &result.token, &deliveryDone](int receivedToken) {
            bool expected = false;
            if (receivedToken == *token) {
                if (deliveryDone.compare_exchange_strong(expected, true)) {
                    promise->set_value(true);
                }
            }
        });

    const std::string topic = buildTopicName();
    const std::string data = "test data";

    result = instance->publish(topic, (void *)(data.c_str()), data.size(), 1, false);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.token != 0);


    Timer timer(successTimeout);
    auto status = sendFuture.wait_for(timer.remain());
    instance->setSentCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(sendFuture.get());

    status = deliveryFuture.wait_for(timer.remain());
    instance->setDeliveryCompletedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(deliveryFuture.get());
}

TEST_F(MqttStreamingProtocolTest, PublishingRetainedWithNullData)
{
    {
        auto ok = connect("127.0.0.1", clientId);
        ASSERT_TRUE(ok);
    }
    const std::string topic = buildTopicName();
    {
        auto ok = removeRetainedTopic(topic);
        ASSERT_TRUE(ok);
    }
}

TEST_F(MqttStreamingProtocolTest, PublishingRetainedWithReceivingControl)
{
    ASSERT_TRUE(connect("127.0.0.1", clientId));

    const std::string topic = buildTopicName();
    ASSERT_TRUE(removeRetainedTopic(topic));

    const std::string text = "test data";
    const MqttMessage msg(topic, std::vector<uint8_t>(text.begin(), text.end()), 1, true);

    ASSERT_TRUE(publishMsg(msg));
    ASSERT_TRUE(instance->disconnect().success);


    std::this_thread::sleep_for(milliseconds(500)); // Give some time to the broker to store the retained message)

    MqttAsyncClientWrapper subscriber("testSubscriberId");
    ASSERT_TRUE(subscriber.connect("127.0.0.1"));

    std::promise<MqttMessage> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();
    std::atomic<bool> done{false};
    subscriber.instance
        ->setMessageArrivedCb(msg.getTopic(),
                              [&msg,
                               &done,
                               promise = &receivedPromise](const mqtt::MqttAsyncClient &subscriber,
                                                           mqtt::MqttMessage &receivedMsg) {
                                  if (receivedMsg.getData().empty()) {
                                      return;
                                  }
                                  bool expected = false;
                                  if (done.compare_exchange_strong(expected, true)) {
                                      promise->set_value(receivedMsg);
                                  }
                              });

    Timer receiveTimer(successTimeout);
    auto result = subscriber.instance->subscribe(msg.getTopic(), msg.getQos());
    ASSERT_TRUE(result.success);
    auto status = receivedFuture.wait_for(receiveTimer.remain());
    instance->setMessageArrivedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);

    auto receivedMsg = receivedFuture.get();
    ASSERT_TRUE(receivedMsg == msg);
    // ASSERT_TRUE(receivedMsg.getRetained());

}

TEST_F(MqttStreamingProtocolTest, PublishingWithReceivingControl)
{
    ASSERT_TRUE(connect("127.0.0.1", clientId));

    const std::string topic = buildTopicName();
    ASSERT_TRUE(removeRetainedTopic(topic));

    const std::string text = "test data";
    const MqttMessage msg(topic, std::vector<uint8_t>(text.begin(), text.end()), 1, false);
    MqttAsyncClientWrapper subscriber("testSubscriberId");
    ASSERT_TRUE(subscriber.connect("127.0.0.1"));

    std::promise<MqttMessage> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();
    std::atomic<bool> done{false};
    subscriber.instance
        ->setMessageArrivedCb(msg.getTopic(),
                              [&msg,
                               &done,
                               promise = &receivedPromise](const mqtt::MqttAsyncClient &subscriber,
                                                           mqtt::MqttMessage &receivedMsg) {
                                  if (receivedMsg.getData().empty()) {
                                      return;
                                  }
                                  bool expected = false;
                                  if (done.compare_exchange_strong(expected, true)) {
                                      promise->set_value(receivedMsg);
                                  }
                              });

    Timer receiveTimer(successTimeout);
    auto result = subscriber.instance->subscribe(msg.getTopic(), msg.getQos());
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(publishMsg(msg));

    auto status = receivedFuture.wait_for(receiveTimer.remain());
    instance->setMessageArrivedCb(nullptr);
    ASSERT_TRUE(status == std::future_status::ready);

    auto receivedMsg = receivedFuture.get();
    ASSERT_TRUE(receivedMsg == msg);
}

TEST_F(MqttStreamingProtocolTest, PublishingWithoutConnection)
{
    const std::string topic = buildTopicName();
    const std::string data = "test data";
    auto result = instance->publish(topic, (void *)(data.c_str()), data.size(), 1, false);
    ASSERT_FALSE(result.success);
}

TEST_P(JsonTimestampParsingIntPTest, IntTimestampParsing)
{
    auto [input, output] = GetParam();
    ASSERT_EQ(mqtt::utils::numericToMicroseconds(input), output);
}

INSTANTIATE_TEST_SUITE_P(IntTimestampParsing,
                         JsonTimestampParsingIntPTest,
                         ::testing::Values(std::pair<uint64_t, uint64_t>(1761664976, 1761664976000000ULL),         // seconds
                                           std::pair<uint64_t, uint64_t>(1761664976123, 1761664976123000ULL),      // milliseconds
                                           std::pair<uint64_t, uint64_t>(1761664976123456, 1761664976123456ULL),   // microseconds
                                           std::pair<uint64_t, uint64_t>(1761664976123456789, 1761664976123456ULL) // nanoseconds
                                           ));

TEST_P(JsonTimestampParsingStrPTest, StrTimestampParsing)
{
    auto [input, output] = GetParam();
    ASSERT_EQ(mqtt::utils::toUnixTicks(input), output);
}

INSTANTIATE_TEST_SUITE_P(StrTimestampParsing,
                         JsonTimestampParsingStrPTest,
                         ::testing::Values(std::pair<std::string, uint64_t>("2025-10-28 15:22:56", 1761664976000000ULL),
                                           std::pair<std::string, uint64_t>(" 2025-10-28 15:22:56 ", 1761664976000000ULL),
                                           std::pair<std::string, uint64_t>(" 2025-10-28 15:22:56.123", 1761664976123000ULL),
                                           std::pair<std::string, uint64_t>(" 2025-10-28T15:22:56Z ", 1761664976000000ULL),
                                           std::pair<std::string, uint64_t>(" 1761664976", 1761664976000000ULL),         // seconds
                                           std::pair<std::string, uint64_t>("1761664976123 ", 1761664976123000ULL),      // milliseconds
                                           std::pair<std::string, uint64_t>(" 1761664976123456 ", 1761664976123456ULL),   // microseconds
                                           std::pair<std::string, uint64_t>("  1761664976123456789  ", 1761664976123456ULL) // nanoseconds
                                           ));
