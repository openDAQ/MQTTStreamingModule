#include "MqttAsyncClient.h"
#include <future>
#include <gmock/gmock.h>
#include <testutils/testutils.h>
#include <thread>

using namespace mqtt;
using namespace std::chrono;

class Timer
{
public:
    Timer(int ms)
    {
        start = steady_clock::now();
        timeout = milliseconds(ms);
    }
    milliseconds remain() const {
        auto now = steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<milliseconds>(now - start);
        milliseconds newTout = (elapsed_ms >= timeout) ? milliseconds(0) : timeout - elapsed_ms;
        return newTout;
    }
    bool expired() {
        return remain() == milliseconds(0);
    }
    explicit operator milliseconds() const noexcept
    {
        return remain();
    }
protected:
    std::chrono::steady_clock::time_point start;
    std::chrono::milliseconds timeout;
};

class MqttAsyncClientWrapper
{
public:
    MqttAsyncClientWrapper() = default;
    MqttAsyncClientWrapper(std::shared_ptr<MqttAsyncClient> instance, std::string clientId = "")
        : instance(instance)
    {
        if (!clientId.empty()) {
            this->clientId = clientId;
        }
    }
    bool createConnection(const std::string& url, const std::string& id) {
        instance->setServerURL(url);
        instance->setClientId(id);

        connectedDone = false;
        connectedPromise = std::promise<bool>();
        connectedFuture = connectedPromise.get_future();

        instance->setConnectedCb([this]() {
            bool expected = false;
            if (connectedDone.compare_exchange_strong(expected, true)) {
                connectedPromise.set_value(true);
            }
        });
        return instance->connect();
    }

    bool connect(const std::string& url) {
        return connect(url, clientId);
    }
    bool connect(const std::string& url, const std::string& id) {
        bool res = createConnection(url, id);
        if (res) {
            auto status = connectedFuture.wait_for(milliseconds(successTimeout));
            instance->setConnectedCb(nullptr);
            res = (status == std::future_status::ready && connectedFuture.get() == true);
        }
        return res;
    }

    bool disconnect() {
        if (instance->isConnected() != MqttConnectionStatus::connected) {
            return true;
        }
        std::atomic<bool> done{false};
        std::promise<bool> disconnectedPromise;
        auto disconnectedFuture = disconnectedPromise.get_future();
        instance->setDisconnectCb([promise = &disconnectedPromise, &done](bool result) {
            bool expected = false;
            if (done.compare_exchange_strong(expected, true)) {
                promise->set_value(result);
            }
        });

        auto disconnectionOk = instance->disconnect();
        if (!disconnectionOk) {
            return false;
        }

        auto status = disconnectedFuture.wait_for(milliseconds(successTimeout));
        instance->setDisconnectCb(nullptr);
        return (status == std::future_status::ready && disconnectedFuture.get() == true);
    }

    bool removeRetainedTopic(const std::string& topic) {
        return publishMsg(topic, "", true);
    }

    bool publishMsg(const std::string& topic, const std::string& data, bool retained = false) {
        const MqttMessage msg(topic, std::vector<uint8_t>(data.begin(), data.end()), 1, retained);
        return publishMsg(msg);
    }

    bool publishMsg(const MqttMessage& msg) {
        int token = 0;

        std::promise<int> deliveryPromise;
        auto deliveryFuture = deliveryPromise.get_future();
        instance->setDeliveryCompletedCb([promise = &deliveryPromise](int deliveredToken) {
            promise->set_value(deliveredToken);
        });

        Timer sendTimer(successTimeout);
        auto ok = instance->publish(msg.getTopic(),
                                    (void *) (msg.getData().data()),
                                    msg.getData().size(),
                                    nullptr,
                                    msg.getQos(),
                                    &token,
                                    msg.getRetained());
        if (!ok || token == 0) {
            instance->setDeliveryCompletedCb(nullptr);
            return false;
        }

        auto status = deliveryFuture.wait_for(sendTimer.remain());
        instance->setDeliveryCompletedCb(nullptr);
        return (status == std::future_status::ready && deliveryFuture.get() == token);
    }

    std::string buildTopicName() {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    std::shared_ptr<MqttAsyncClient> instance;
    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;
    std::atomic<bool> connectedDone{false};

    int successTimeout = 5000;
    int failureTimeout = 3000;
    std::string clientId = "testMqttClientId";
};

class MqttStreamingProtocolTest : public ::testing::Test,  public MqttAsyncClientWrapper {
protected:
    void SetUp() override {
        instance = std::make_shared<MqttAsyncClient>();
    }

    void TearDown() override {
        instance.reset();
    }
};

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

    auto disconnectionOk = instance->disconnect();
    ASSERT_TRUE(disconnectionOk);

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

    int token = 0;
    std::atomic<bool> sendDone{false};
    std::promise<bool> sendPromise;
    auto sendFuture = sendPromise.get_future();
    instance->setSentCb([promise = &sendPromise, token = &token, &sendDone](int receivedToken, bool result) {
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
        [promise = &deliveryPromise, token = &token, &deliveryDone](int receivedToken) {
            bool expected = false;
            if (receivedToken == *token) {
                if (deliveryDone.compare_exchange_strong(expected, true)) {
                    promise->set_value(true);
                }
            }
        });

    const std::string topic = buildTopicName();
    const std::string data = "test data";

    ok = instance->publish(topic, (void *)(data.c_str()), data.size(), nullptr, 1, &token, false);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(token != 0);


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
    ASSERT_TRUE(instance->disconnect());


    std::this_thread::sleep_for(milliseconds(500)); // Give some time to the broker to store the retained message)

    MqttAsyncClientWrapper subscriber(std::make_shared<MqttAsyncClient>(), "testSubscriberId");
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
    auto ok = subscriber.instance->subscribe(msg.getTopic(), msg.getQos());
    ASSERT_TRUE(ok);
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
    MqttAsyncClientWrapper subscriber(std::make_shared<MqttAsyncClient>(), "testSubscriberId");
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
    auto ok = subscriber.instance->subscribe(msg.getTopic(), msg.getQos());
    ASSERT_TRUE(ok);
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
    int token = 0;
    auto ok = instance->publish(topic, (void *)(data.c_str()), data.size(), nullptr, 1, &token, false);
    ASSERT_FALSE(ok);
}
