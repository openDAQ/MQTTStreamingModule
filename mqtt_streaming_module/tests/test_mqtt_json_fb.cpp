#include "mqtt_streaming_module/mqtt_json_receiver_fb_impl.h"
#include "test_daq_test_helper.h"
#include "test_data.h"
#include <cmath>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace daq::modules::mqtt_streaming_module
{
class MqttJsonFbHelper
{
public:
    std::unique_ptr<MqttJsonReceiverFbImpl> obj;

    auto getSignals()
    {
        auto signalList = List<ISignal>();
        obj->getSignals(&signalList);
        return signalList;
    }

    std::string buildTopicName(const std::string& postfix = "")
    {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + postfix;
    }

    std::string buildClientId()
    {
        return std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "_ClientId";
    }
};

class MqttJsonFbTest : public testing::Test, public DaqTestHelper, public MqttJsonFbHelper
{
};

class MqttJsonFbTopicPTest : public ::testing::TestWithParam<std::pair<std::string, bool>>,
                             public DaqTestHelper,
                             public MqttJsonFbHelper
{
};

class MqttJsonFbConfigPTest : public ::testing::TestWithParam<std::string>,
                             public DaqTestHelper,
                             public MqttJsonFbHelper
{
};

class MqttJsonFbConfigFilePTest : public ::testing::TestWithParam<std::string>,
                              public DaqTestHelper,
                              public MqttJsonFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttJsonFbTest, DefaultConfig)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = rootMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(JSON_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 3u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_JSON_CONFIG));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_JSON_CONFIG).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_JSON_CONFIG).asPtr<IString>().getLength(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_JSON_CONFIG_FILE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_JSON_CONFIG_FILE).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_JSON_CONFIG_FILE).asPtr<IString>().getLength(), 0u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_TOPIC));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_TOPIC).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().getLength(), 0u);
}

TEST_F(MqttJsonFbTest, Config)
{
    StartUp();
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_TOPIC, buildTopicName());
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));

    const auto allProperties = jsonFb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(jsonFb.hasProperty(propName));
        ASSERT_EQ(jsonFb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttJsonFbTest, CreationWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME));
    EXPECT_EQ(jsonFb.getSignals().getCount(), 0u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Warning", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttJsonFbTest, CreationWithPartialConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = PropertyObject();
    config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, String(buildTopicName())));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    EXPECT_EQ(jsonFb.getSignals().getCount(), 0u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttJsonFbTest, CreationWithCustomConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_TOPIC, String(buildTopicName()));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    EXPECT_EQ(jsonFb.getSignals().getCount(), 0u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttJsonFbTopicPTest, CheckJsonFbTopic)
{
    auto [topic, result] = GetParam();
    StartUp();

    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    auto signals = fb.getSignals();
    ASSERT_EQ(signals.getCount(), 0);
    const auto expectedComponentStatus = result ? "Ok" : "Warning";
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", expectedComponentStatus, daqInstance.getContext().getTypeManager()));
    if (result)
    {
        EXPECT_NE(fb.getStatusContainer().getStatus("SubscriptionStatus"),
                  EnumerationWithIntValue(MQTT_FB_SUB_STATUS_TYPE,
                                          static_cast<Int>(MqttBaseFb::SubscriptionStatus::InvalidTopicName),
                                          daqInstance.getContext().getTypeManager()));
    }
    else
    {
        EXPECT_EQ(fb.getStatusContainer().getStatus("SubscriptionStatus"),
                  EnumerationWithIntValue(MQTT_FB_SUB_STATUS_TYPE,
                                          static_cast<Int>(MqttBaseFb::SubscriptionStatus::InvalidTopicName),
                                          daqInstance.getContext().getTypeManager()));
    }
}

INSTANTIATE_TEST_SUITE_P(TopicTest,
                         MqttJsonFbTopicPTest,
                         ::testing::Values(std::make_pair("", false),
                                           std::make_pair("goodTopic/test", true),
                                           std::make_pair("/goodTopic/test0", true),
                                           std::make_pair("badTopic/+/test/topic", false),
                                           std::make_pair("badTopic/+/+/topic", false),
                                           std::make_pair("badTopic/#", false)));

TEST_F(MqttJsonFbTest, RemovingNestedFunctionBlock)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, String(buildTopicName())));
        ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
        ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    daq::FunctionBlockPtr jsonDecoderFb;
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_VALUE_NAME, String("temp")));
        ASSERT_NO_THROW(jsonDecoderFb = jsonFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
    }
    ASSERT_EQ(jsonFb.getFunctionBlocks().getCount(), 1u);

    ASSERT_NO_THROW(jsonFb.removeFunctionBlock(jsonDecoderFb));
    ASSERT_EQ(jsonFb.getFunctionBlocks().getCount(), 0u);
}

TEST_F(MqttJsonFbTest, TwoFbCreation)
{
    StartUp();
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, buildTopicName("0")));
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
        EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, buildTopicName("1")));
        ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
        EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    auto fbs = rootMqttFb.getFunctionBlocks();
    ASSERT_EQ(fbs.getCount(), 2u);
}

TEST_F(MqttJsonFbTest, PropertyChanged)
{
    StartUp();

    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    auto topic = buildTopicName("0");
    config.addProperty(StringProperty(PROPERTY_NAME_TOPIC, topic));
    ASSERT_NO_THROW(fb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto jsonFb = reinterpret_cast<MqttJsonReceiverFbImpl*>(*fb);

    ASSERT_EQ(topic, jsonFb->getSubscribedTopic());
    topic = buildTopicName("1");
    fb.setPropertyValue(PROPERTY_NAME_TOPIC, topic);
    ASSERT_EQ(topic, jsonFb->getSubscribedTopic());
}

TEST_F(MqttJsonFbTest, JsonInit0)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG, String(VALID_JSON_1_TOPIC_0));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getFunctionBlocks().getCount(), 3u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string name, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), name);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(jsonFb.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().toStdString(), "openDAQ/RefDev0/IO/AI/RefCh0/Sig/AI0");

    lambda(jsonFb.getFunctionBlocks()[0], "AI0", "value", "timestamp", "V");
    lambda(jsonFb.getFunctionBlocks()[1], "AI1", "value1", "", "");
    lambda(jsonFb.getFunctionBlocks()[2], "AI2", "value2", "", "W");

}

TEST_F(MqttJsonFbTest, JsonInit1)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG, String(VALID_JSON_1_TOPIC_1));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getFunctionBlocks().getCount(), 3u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string name, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), name);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(jsonFb.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().toStdString(), "/mirip/UNet3AC2/sensor/data");

    lambda(jsonFb.getFunctionBlocks()[0], "temp", "temp", "ts", "°C");
    lambda(jsonFb.getFunctionBlocks()[1], "humidity", "humi", "ts", "%");
    lambda(jsonFb.getFunctionBlocks()[2], "tds", "tds_value", "ts", "ppm");

}

TEST_P(MqttJsonFbConfigPTest, JsonWrongInit)
{
    const auto configJson = GetParam();
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG, String(configJson));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    EXPECT_EQ(jsonFb.getFunctionBlocks().getCount(), 0u);
    EXPECT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_EQ(jsonFb.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().toStdString(), "");
}

INSTANTIATE_TEST_SUITE_P(
    JsonConfigTest,
    MqttJsonFbConfigPTest,
    ::testing::Values(
        VALID_JSON_3_TOPIC_2,
        WILDCARD_JSON_0,
        WILDCARD_JSON_1,
        INVALID_JSON_1,
        INVALID_JSON_3));

TEST_P(MqttJsonFbConfigFilePTest, JsonInitFromFile)
{
    const auto configJson = GetParam();
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG_FILE, String(configJson));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

INSTANTIATE_TEST_SUITE_P(JsonConfigTest,
                         MqttJsonFbConfigFilePTest,
                         ::testing::Values("data/public-example0.json",
                                           "data/public-example1.json",
                                           "data/public-example2.json",
                                           "data/public-example3.json"));

TEST_F(MqttJsonFbTest, JsonInitFromFileWithChecking)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG_FILE, String("data/public-example0.json"));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    ASSERT_EQ(jsonFb.getFunctionBlocks().getCount(), 3u);
    ASSERT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string name, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), name);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(jsonFb.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().toStdString(), "/mirip/UNet3AC2/sensor/data");

    lambda(jsonFb.getFunctionBlocks()[0], "temp", "temp", "ts", "°C");
    lambda(jsonFb.getFunctionBlocks()[1], "humidity", "humi", "ts", "%");
    lambda(jsonFb.getFunctionBlocks()[2], "tds", "tds_value", "ts", "ppm");

}

TEST_F(MqttJsonFbTest, JsonInitFromFileWrongPath)
{
    StartUp();
    daq::FunctionBlockPtr jsonFb;
    auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(JSON_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_JSON_CONFIG_FILE, String("/justWrongPath/wrongFile.txt"));
    ASSERT_NO_THROW(jsonFb = rootMqttFb.addFunctionBlock(JSON_FB_NAME, config));
    EXPECT_EQ(jsonFb.getFunctionBlocks().getCount(), 0u);
    EXPECT_EQ(jsonFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_EQ(jsonFb.getPropertyValue(PROPERTY_NAME_TOPIC).asPtr<IString>().toStdString(), "");
}
