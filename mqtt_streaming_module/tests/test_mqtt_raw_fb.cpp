#include "MqttAsyncClientWrapper.h"
#include "mqtt_streaming_module/mqtt_raw_receiver_fb_impl.h"
#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace daq::modules::mqtt_streaming_module
{
class MqttRawFbTest : public testing::Test, public DaqTestHelper
{
public:
    std::unique_ptr<MqttRawReceiverFbImpl> obj;

    void onSignalsMessage(mqtt::MqttMessage& msg)
    {
        mqtt::MqttAsyncClient unused;
        obj->onSignalsMessage(unused, msg);
    }

    void CreateRawFB(std::string topic)
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, ""));
        const auto fbType = FunctionBlockType(RAW_FB_NAME, RAW_FB_NAME, "", config);
        config.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
        obj = std::make_unique<MqttRawReceiverFbImpl>(NullContext(), nullptr, fbType, "localId", nullptr, config);
    }

    std::string buildTopicName()
    {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }
};

class MqttRawFbPTest : public ::testing::TestWithParam<std::pair<std::string, bool>>,
                                       public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttRawFbTest, DefaultRawFbConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = rootMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(RAW_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 1u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_TOPIC));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_TOPIC).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().getLength(), 0u);
}

TEST_F(MqttRawFbTest, CreateRawFunctionalBlocks)
{
    StartUp();
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME));
    ASSERT_EQ(rawFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(rawFb.getName(), RAW_FB_NAME);
    auto fbs = rootMqttFb.getFunctionBlocks();
    bool contain = false;
    daq::GenericFunctionBlockPtr<daq::IFunctionBlock> fbFromList;
    for (const auto& fb : fbs)
    {
        contain = (fb.getName() == RAW_FB_NAME);
        if (contain)
        {
            fbFromList = fb;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(fbFromList.assigned());
    ASSERT_EQ(fbFromList.getName(), rawFb.getName());
    ASSERT_TRUE(fbFromList == rawFb);
}

TEST_F(MqttRawFbTest, CheckRawFbWithEmptyConfig)
{
    StartUp();
    daq::FunctionBlockPtr rawFb;
    auto config = PropertyObject();
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
}

TEST_F(MqttRawFbTest, CheckRawFbWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
}

TEST_F(MqttRawFbTest, CheckRawFbWithPartialConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr rawFb;
    auto config = PropertyObject();
    config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, ""));
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config));
}

TEST_F(MqttRawFbTest, CheckRawFbWithCustomConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr rawFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_TOPIC, buildTopicName());
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config));
}

TEST_P(MqttRawFbPTest, CheckRawFbTopic)
{
    auto [topic, result] = GetParam();
    StartUp();

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), result ? 1u : 0u);
}

INSTANTIATE_TEST_SUITE_P(TopicTest,
                         MqttRawFbPTest,
                         ::testing::Values(std::make_pair("", false),
                                           std::make_pair("goodTopic/test", true),
                                           std::make_pair("/goodTopic/test0", true),
                                           std::make_pair("badTopic/+/test/topic", false),
                                           std::make_pair("badTopic/+/+/topic", false),
                                           std::make_pair("badTopic/#", false)));

TEST_F(MqttRawFbTest, CheckRawFbConfig)
{
    StartUp();

    const auto topic = buildTopicName();
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config));

    const auto allProperties = rawFb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(rawFb.hasProperty(propName));
        ASSERT_EQ(rawFb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttRawFbTest, CheckRawFbDataTransfer)
{
    const auto topic = buildTopicName();
    const auto dataToSend = std::vector<std::vector<uint8_t>>{std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05},
                                                              std::vector<uint8_t>{0x11, 0x12, 0x13, 0x14},
                                                              std::vector<uint8_t>{0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                                                              std::vector<uint8_t>{0x31},
                                                              std::vector<uint8_t>{0x41, 0x42, 0x43, 0x44, 0x45}};
    std::vector<std::vector<uint8_t>> dataToReceive;

    CreateRawFB({topic});

    auto signalList = List<ISignal>();
    obj->getSignals(&signalList);
    auto reader = daq::PacketReader(signalList[0]);

    for (const auto& data : dataToSend)
    {
        mqtt::MqttMessage msg = {topic, data, 1, 0};
        onSignalsMessage(msg);
    }

    while (!reader.getEmpty())
    {
        auto packet = reader.read();
        if (const auto eventPacket = packet.asPtrOrNull<IEventPacket>(); eventPacket.assigned())
        {
            continue;
        }
        if (const auto dataPacket = packet.asPtrOrNull<IDataPacket>(); dataPacket.assigned())
        {
            std::vector<uint8_t> readData(dataPacket.getDataSize());
            memcpy(readData.data(), dataPacket.getData(), dataPacket.getDataSize());
            dataToReceive.push_back(std::move(readData));
        }
    }
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_EQ(dataToSend, dataToReceive);
}

TEST_F(MqttRawFbTest, CheckRawFbFullDataTransfer)
{
    const std::string topic = buildTopicName();

    StartUp();

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
    auto singal = rootMqttFb.addFunctionBlock(RAW_FB_NAME, config).getSignals()[0];
    auto reader = daq::PacketReader(singal);

    MqttAsyncClientWrapper publisher("testPublisherId");
    ASSERT_TRUE(publisher.connect("127.0.0.1"));

    const auto dataToSend = std::vector<std::vector<uint8_t>>{std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05},
                                                              std::vector<uint8_t>{0x11, 0x12, 0x13, 0x14},
                                                              std::vector<uint8_t>{0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                                                              std::vector<uint8_t>{0x31},
                                                              std::vector<uint8_t>{0x41, 0x42, 0x43, 0x44, 0x45}};
    std::vector<std::vector<uint8_t>> dataToReceive;

    for (const auto& data : dataToSend)
    {
        mqtt::MqttMessage msg = {topic, data, 1, 0};
        ASSERT_TRUE(publisher.publishMsg(msg));
    }

    while (!reader.getEmpty())
    {
        auto packet = reader.read();
        if (const auto eventPacket = packet.asPtrOrNull<IEventPacket>(); eventPacket.assigned())
        {
            continue;
        }
        if (const auto dataPacket = packet.asPtrOrNull<IDataPacket>(); dataPacket.assigned())
        {
            std::vector<uint8_t> readData(dataPacket.getDataSize());
            memcpy(readData.data(), dataPacket.getData(), dataPacket.getDataSize());
            dataToReceive.push_back(std::move(readData));
        }
    }

    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_EQ(dataToSend, dataToReceive);
}
