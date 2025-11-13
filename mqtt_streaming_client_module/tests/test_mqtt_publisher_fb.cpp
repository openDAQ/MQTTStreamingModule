#include "MqttAsyncClientWrapper.h"
#include "Timer.h"
#include "mqtt_streaming_client_module/mqtt_publisher_fb_impl.h"
#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

template<typename T>
class ReferenceDomainOffsetHelper
{
public:
    ModulePtr module;
    DataDescriptorPtr domainSignalDescriptor;
    DataDescriptorPtr signalDescriptor;
    SignalConfigPtr signal0;
    SignalConfigPtr signal1;
    SignalConfigPtr signalWithoutDomain;
    SignalConfigPtr domainSignal;
    ContextPtr context;
    SampleType sampleType;

    ReferenceDomainOffsetHelper()
    {
        sampleType = SampleTypeFromType<T>::SampleType;
        // Create domain signal
        auto logger = Logger();
        context = Context(Scheduler(logger), logger, TypeManager(), nullptr, nullptr);
        domainSignalDescriptor = DataDescriptorBuilder()
                                          .setUnit(Unit("s", -1, "seconds", "time"))
                                          .setSampleType(SampleType::UInt64)
                                          .setRule(LinearDataRule(5, 3))
                                          .setOrigin("1970")
                                          .setTickResolution(Ratio(1, 1000))
                                          .setReferenceDomainInfo(ReferenceDomainInfoBuilder().setReferenceDomainOffset(100).build())
                                          .build();
        domainSignal = SignalWithDescriptor(context, domainSignalDescriptor, nullptr, "DomainSignal");
        // Create signal with descriptor
        signalDescriptor =
            DataDescriptorBuilder().setSampleType(sampleType).build();
        signal0 = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal0");
        signal1 = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal1");
        // Set domain signal of signal
        signal0.setDomainSignal(domainSignal);
        signal1.setDomainSignal(domainSignal);

        signalWithoutDomain = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal");
        // Create module
        createModule(&module, context);
    }

    T generateData(size_t i)
    {
        T sampleData;
        if constexpr (std::is_integral_v<T>)
        {
            sampleData = i;
            if constexpr (std::is_signed_v<T>)
            {
                sampleData *= ((i % 2 == 0) ? 1 : -1);
            }
        }
        else if constexpr (std::is_fundamental_v<T>)
        {
            sampleData = static_cast<T>(i) * 1.1 * ((i % 2 == 0) ? 1 : -1);
        }
        return sampleData;
    }

    void send(int sampleCount, uint divider = 1)
    {
        if (divider == 0)
            divider = 1;
        for (size_t i = 0; i < sampleCount * divider; i++)
        {
            auto sendPacket = [this](SignalConfigPtr signal, T data, DataPacketPtr domainPacket)
            {
                auto dataPacket = DataPacketWithDomain(domainPacket, signalDescriptor, 1);
                copyData(dataPacket, data);
                signal.sendPacket(dataPacket);
            };
            auto domainPacket = DataPacket(domainSignalDescriptor, 1, i);
            domainSignal.sendPacket(domainPacket);
            if (i % divider == 0)
                sendPacket(signal0, generateData(i / divider), domainPacket);

            sendPacket(signal1, generateData(i), domainPacket);
        }
    }
protected:
    bool checkType(SampleType type)
    {
        switch (type)
        {
        case SampleType::Float32:
        case SampleType::Float64:
        case SampleType::UInt8:
        case SampleType::Int8:
        case SampleType::UInt16:
        case SampleType::Int16:
        case SampleType::UInt32:
        case SampleType::Int32:
        case SampleType::UInt64:
        case SampleType::Int64:
        case SampleType::RangeInt64:
        case SampleType::ComplexFloat32:
        case SampleType::ComplexFloat64:
            return true;
        case SampleType::String:
        case SampleType::Binary:
        case SampleType::Struct:
        case SampleType::Invalid:
        case SampleType::Null:
        case SampleType::_count:
            return false;
        }
        return true;
    }
    bool copyData(T& destination, const DataPacketPtr source)
    {
        const auto dataType = source.getDataDescriptor().getSampleType();
        if (checkType(dataType) && getSampleSize(dataType) != sizeof(destination))
            return false;
        if constexpr (std::is_same_v<T, std::string>)
        {
            return false;
        }
        else
        {
            memcpy(&destination, source.getData(), sizeof(destination));
        }
        return true;
    }

    bool copyData(DataPacketPtr destination, const T& source)
    {
        const auto dataType = destination.getDataDescriptor().getSampleType();
        if (checkType(dataType) && getSampleSize(dataType) != sizeof(source))
            return false;
        if constexpr (std::is_same_v<T, std::string>)
        {
            return false;
        }
        else
        {
            memcpy(destination.getData(), &source, sizeof(source));
        }
        return true;
    }
};

namespace daq::modules::mqtt_streaming_client_module
{
class MqttPublisherFbHelper
{
public:
    std::unique_ptr<MqttPublisherFbImpl> obj;

    void CreatePublisherFB()
    {
        auto config = PropertyObject();
        const auto fbType = MqttPublisherFbImpl::CreateType();
        obj = std::make_unique<MqttPublisherFbImpl>(NullContext(), nullptr, fbType, nullptr, config);
    }

    auto getSignals()
    {
        auto signalList = List<ISignal>();
        obj->getSignals(&signalList);
        return signalList;
    }

    std::string buildTopicName()
    {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    std::string buildClientId()
    {
        return std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "_ClientId";
    }

private:

};

class MqttPublisherFbTest : public testing::Test, public DaqTestHelper, public MqttPublisherFbHelper
{
};
class MqttPublisherFbPTest : public ::testing::TestWithParam<uint>, public DaqTestHelper, public MqttPublisherFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_client_module

TEST_F(MqttPublisherFbTest, DefaultConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(PUB_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 4u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_TOPIC_MODE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_TOPIC_MODE).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE).asPtr<IInteger>(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_SHARED_TS));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_SHARED_TS).asPtr<IBoolean>(), False);
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_GROUP_VALUES));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_GROUP_VALUES).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES).asPtr<IBoolean>(), False);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES).getValueType(), CoreType::ctBool);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES).asPtr<IBoolean>(), False);


}

TEST_F(MqttPublisherFbTest, PropertyVisability)
{
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt = MqttPublisherFbImpl::CreateType();
    daq::PropertyObjectPtr defaultConfig = fbt.createDefaultConfig();

    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1); // Set to Multi topic
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_PUB_SHARED_TS).getVisible());
}

TEST_F(MqttPublisherFbTest, Config)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_PUB_TOPIC_MODE, 1);
    config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_GROUP_VALUES, True);
    config.setPropertyValue(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, True);
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    const auto allProperties = fb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(fb.hasProperty(propName));
        ASSERT_EQ(fb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
    MqttPublisherFbImpl* ptr = reinterpret_cast<MqttPublisherFbImpl*>(fb.getObject());
    ASSERT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->getFbConfig().topicMode, TopicMode::Multi);
    EXPECT_TRUE(ptr->getFbConfig().sharedTs);
    EXPECT_TRUE(ptr->getFbConfig().groupValues);
    EXPECT_TRUE(ptr->getFbConfig().useSignalNames);
}

TEST_F(MqttPublisherFbTest, Creation)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, TwoFbCreation)
{
    StartUp();
    {
        daq::FunctionBlockPtr fb;
        ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    {
        daq::FunctionBlockPtr fb;
        ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    auto fbs = device.getFunctionBlocks();
    ASSERT_EQ(fbs.getCount(), 2u);
}

TEST_F(MqttPublisherFbTest, CreationWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
    auto signals = fb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, CreationWithPartialConfig)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    config.addProperty(BoolProperty(PROPERTY_NAME_PUB_USE_SIGNAL_NAMES, True));
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, ConnectToPort)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
    // Create helper
    auto help = ReferenceDomainOffsetHelper<double>();

    ASSERT_EQ(fb.getInputPorts().getCount(), 1u);
    fb.getInputPorts()[0].connect(help.signal0);
    ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    fb.getInputPorts()[1].connect(help.signal0);
    ASSERT_EQ(fb.getInputPorts().getCount(), 3u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    // disconnection
    fb.getInputPorts()[1].disconnect();
    ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    // connection without a domain signal
    fb.getInputPorts()[1].connect(help.signalWithoutDomain);
    ASSERT_EQ(fb.getInputPorts().getCount(), 3u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    // disconnection
    fb.getInputPorts()[1].disconnect();
    ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttPublisherFbTest, Transfer)
{
    StartUp();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));

    int sampleCnt = 15;
    auto help = ReferenceDomainOffsetHelper<int64_t>();
    fb.getInputPorts()[0].connect(help.signal0);

    MqttAsyncClientWrapper subscriber(std::make_shared<mqtt::MqttAsyncClient>(),
                                      std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "testSubscriberId");
    ASSERT_TRUE(subscriber.connect("127.0.0.1"));

    const std::string topic = help.signal0.getGlobalId().toStdString();
    std::promise<bool> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();
    std::atomic<bool> done{false};
    std::atomic<int> cnt{sampleCnt};
    subscriber.instance
        ->setMessageArrivedCb(topic,
                              [&done,
                               &cnt,
                               promise = &receivedPromise](const mqtt::MqttAsyncClient &subscriber,
                                                           mqtt::MqttMessage &receivedMsg) {
                                  if (receivedMsg.getData().empty()) {
                                      return;
                                  }
                                  bool expected = false;
                                  if (--cnt <= 0 && done.compare_exchange_strong(expected, true)) {
                                      promise->set_value(true);
                                  }
                              });

    Timer receiveTimer(3000);
    auto result = subscriber.instance->subscribe(topic, 2);
    ASSERT_TRUE(result.success);
    help.send(sampleCnt);
    auto status = receivedFuture.wait_for(receiveTimer.remain());
    subscriber.instance->setMessageArrivedCb(nullptr);
    result = subscriber.instance->unsubscribe(topic);
    if (result.success)
        subscriber.instance->waitForCompletion(result.token, 1000);
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(receivedFuture.get());
}

TEST_P(MqttPublisherFbPTest, MultiTransfer)
{
    const auto divider = GetParam();
    StartUp();
    daq::FunctionBlockPtr fb;
    auto config = device.getAvailableFunctionBlockTypes().get(PUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_PUB_SHARED_TS, True);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME, config));

    int sampleCnt = 15;
    auto help = ReferenceDomainOffsetHelper<uint64_t>();
    fb.getInputPorts()[0].connect(help.signal0);
    fb.getInputPorts()[1].connect(help.signal1);

    MqttAsyncClientWrapper subscriber(std::make_shared<mqtt::MqttAsyncClient>(),
                                      std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "testSubscriberId");
    {
        auto result = subscriber.connect("127.0.0.1");
        ASSERT_TRUE(result);
    }

    const std::string topic = fb.getGlobalId();
    std::promise<bool> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();
    std::atomic<bool> done{false};
    std::atomic<int> cnt{sampleCnt - 2}; // -2 because of MultiReadr bug
    subscriber.instance
        ->setMessageArrivedCb(topic,
                              [&done,
                               &cnt,
                               promise = &receivedPromise](const mqtt::MqttAsyncClient &subscriber,
                                                           mqtt::MqttMessage &receivedMsg) {
                                  if (receivedMsg.getData().empty()) {
                                      return;
                                  }
                                  bool expected = false;
                                  if (--cnt <= 0 && done.compare_exchange_strong(expected, true)) {
                                      promise->set_value(true);
                                  }
                              });

    Timer receiveTimer(3000);
    {
        auto result = subscriber.instance->subscribe(topic, 2);
        ASSERT_TRUE(result.success);
    }
    help.send(sampleCnt, divider);
    auto status = receivedFuture.wait_for(receiveTimer.remain());
    subscriber.instance->setMessageArrivedCb(nullptr);
    {
        auto result = subscriber.instance->unsubscribe(topic);
        if (result.success)
            subscriber.instance->waitForCompletion(result.token, 1000);
    }
    ASSERT_TRUE(status == std::future_status::ready);
    ASSERT_TRUE(receivedFuture.get());
}

INSTANTIATE_TEST_SUITE_P(SignalUnit, MqttPublisherFbPTest, ::testing::Values(3, 4, 5));
