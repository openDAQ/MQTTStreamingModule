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
    SignalConfigPtr signal;
    SignalConfigPtr signalWithoutDomain;
    uint64_t sampleCount;
    SignalConfigPtr domainSignal;
    ContextPtr context;
    SampleType sampleType;

    ReferenceDomainOffsetHelper(uint64_t count = 5)
    {
        sampleType = SampleTypeFromType<T>::SampleType;
        // Save desired sample count for later
        sampleCount = count;
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
        signal = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal");
        // Set domain signal of signal
        signal.setDomainSignal(domainSignal);

        signalWithoutDomain = SignalWithDescriptor(context, signalDescriptor, nullptr, "Signal");
        // Create module
        createModule(&module, context);
    }

    std::vector<std::pair<T, uint64_t>> send()
    {
        std::vector<std::pair<T, uint64_t>> output;
        output.reserve(sampleCount);
        for (size_t i = 0; i < sampleCount; i++)
        {
            auto domainPacket = DataPacket(domainSignalDescriptor, 1, i);
            auto dataPacket = DataPacketWithDomain(domainPacket, signalDescriptor, 1);
            T sampleData = i;
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
            uint64_t ts = domainPacket.getLastValue().getValue(1);
            output.push_back({sampleData, ts});
            copyData(dataPacket, sampleData);
            domainSignal.sendPacket(domainPacket);
            signal.sendPacket(dataPacket);
        }
        return output;
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
        obj = std::make_unique<MqttPublisherFbImpl>(NullContext(), nullptr, fbType, "localId", nullptr, config);
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
    ASSERT_EQ(fb.getName(), PUB_FB_NAME);
    auto fbs = device.getFunctionBlocks();
    bool contain = false;
    daq::GenericFunctionBlockPtr<daq::IFunctionBlock> fbFromList;
    for (const auto& fb : fbs)
    {
        contain = (fb.getName() == PUB_FB_NAME);
        if (contain)
        {
            fbFromList = fb;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(fbFromList.assigned());
    ASSERT_EQ(fbFromList.getName(), fb.getName());
    ASSERT_TRUE(fbFromList == fb);
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
    fb.getInputPorts()[0].connect(help.signal);
    ASSERT_EQ(fb.getInputPorts().getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    fb.getInputPorts()[1].connect(help.signal);
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

// TEST_F(MqttPublisherFbTest, Transfer)
// {
//     StartUp();
//     daq::FunctionBlockPtr fb;
//     ASSERT_NO_THROW(fb = device.addFunctionBlock(PUB_FB_NAME));
//     // Create helper
//     auto help = ReferenceDomainOffsetHelper();

//     // Set input (port) and output (signal) of the function block
//     fb.getInputPorts()[0].connect(help.signal);
//     help.send();
//     std::this_thread::sleep_for(std::chrono::milliseconds(1000));
// }
