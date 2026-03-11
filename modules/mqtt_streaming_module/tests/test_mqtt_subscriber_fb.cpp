#include "mqtt_streaming_module/mqtt_subscriber_fb_impl.h"
#include "test_daq_test_helper.h"
#include "test_data.h"
#include <cmath>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>
#include "MqttAsyncClientWrapper.h"
#include <mqtt_streaming_helper/timer.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;
using SDSM = MqttSubscriberFbImpl::DomainSignalMode;

namespace daq::modules::mqtt_streaming_module
{
class MqttSubscriberFbHelper
{
public:
    std::unique_ptr<MqttSubscriberFbImpl> obj;

    void CreateSubFB(std::string topic,
                     bool enablePreview = true,
                     SDSM previewTsMode = SDSM::None)
    {
        const auto fbType = MqttSubscriberFbImpl::CreateType();
        auto config = fbType.createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic);
        config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, enablePreview ? True : False);
        config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(previewTsMode));
        obj = std::make_unique<MqttSubscriberFbImpl>(NullContext(), nullptr, fbType, nullptr, config);
    }

    void CreateSubFB(daq::PropertyObjectPtr config)
    {
        const auto fbType = MqttSubscriberFbImpl::CreateType();
        obj = std::make_unique<MqttSubscriberFbImpl>(NullContext(), nullptr, fbType, nullptr, config);
    }

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

    void onSignalsMessage(mqtt::MqttMessage& msg)
    {
        mqtt::MqttAsyncClient unused;
        obj->onSignalsMessage(unused, msg);
    }
};

class MqttSubscriberFbTest : public testing::Test, public DaqTestHelper, public MqttSubscriberFbHelper
{
};

class MqttSubscriberFbTopicPTest : public ::testing::TestWithParam<std::pair<std::string, bool>>,
                             public DaqTestHelper,
                             public MqttSubscriberFbHelper
{
};

class MqttSubscriberFbConfigPTest : public ::testing::TestWithParam<std::string>,
                             public DaqTestHelper,
                             public MqttSubscriberFbHelper
{
};

class MqttSubscriberFbConfigFilePTest : public ::testing::TestWithParam<std::string>,
                              public DaqTestHelper,
                              public MqttSubscriberFbHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttSubscriberFbTest, DefaultConfig)
{
    daq::PropertyObjectPtr defaultConfig = MqttSubscriberFbImpl::CreateType().createDefaultConfig();

    ASSERT_TRUE(defaultConfig.assigned());

    EXPECT_EQ(defaultConfig.getAllProperties().getCount(), 7u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_JSON_CONFIG));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_JSON_CONFIG).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_JSON_CONFIG).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_JSON_CONFIG_FILE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_JSON_CONFIG_FILE).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG_FILE).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_JSON_CONFIG_FILE).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_TOPIC));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_TOPIC).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_TOPIC).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_QOS));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_QOS).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_QOS).asPtr<IInteger>().getValue(DEFAULT_SUB_QOS), DEFAULT_SUB_QOS);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_QOS).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL).getValueType(), CoreType::ctBool);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL).asPtr<IBoolean>().getValue(False), False);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE).asPtr<IInteger>().getValue(0), 0);
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).getValueType(), CoreType::ctBool);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).asPtr<IBoolean>().getValue(False), False);
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).getVisible());
}

TEST_F(MqttSubscriberFbTest, PropertyVisibility)
{
    daq::PropertyObjectPtr defaultConfig = MqttSubscriberFbImpl::CreateType().createDefaultConfig();

    defaultConfig.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).getVisible());
    ASSERT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE).getVisible());
    defaultConfig.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, False);
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING).getVisible());
    ASSERT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE).getVisible());
}

TEST_F(MqttSubscriberFbTest, Config)
{
    StartUp();
    auto config = MqttSubscriberFbImpl::CreateType().createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, buildTopicName());

    daq::FunctionBlockPtr subFb;
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));

    const auto allProperties = subFb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(subFb.hasProperty(propName));
        ASSERT_EQ(subFb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttSubscriberFbTest, CreationWithDefaultConfig)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME));
    EXPECT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 0u);
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttSubscriberFbTest, CreationWithPartialConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = PropertyObject();
    config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, String(buildTopicName())));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    EXPECT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 0u);
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttSubscriberFbTest, CreationWithCustomConfig)
{
    // If FB has only one property, partial config is equivalent to custom config
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, buildTopicName());
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(SDSM::SystemTime));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    EXPECT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttSubscriberFbTest, PreviewSignal)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING, False);
    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, buildTopicName());
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(SDSM::SystemTime));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getSignals().getCount(), 1u);
    EXPECT_EQ(subFb.getSignals()[0].getDescriptor().getSampleType(), daq::SampleType::Binary);
    subFb.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING, True);
    EXPECT_EQ(subFb.getSignals()[0].getDescriptor().getSampleType(), daq::SampleType::String);
}

TEST_F(MqttSubscriberFbTest, DomainForPreviewSignal)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_IS_STRING, False);
    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, buildTopicName());
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(SDSM::None));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 1u);
    subFb.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(SDSM::SystemTime));
    ASSERT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_TRUE(subFb.getSignals()[0].getDomainSignal().assigned());
    subFb.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL_TS_MODE, static_cast<int>(SDSM::None));
    ASSERT_EQ(subFb.getSignals(daq::search::Any()).getCount(), 1u);
    ASSERT_FALSE(subFb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(MqttSubscriberFbTest, SubscriptionStatusWaitingForData)
{
    StartUp();

    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, buildTopicName());
    daq::FunctionBlockPtr subFb;
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

TEST_P(MqttSubscriberFbTopicPTest, CheckSubscriberFbTopic)
{
    auto [topic, result] = GetParam();
    StartUp();

    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic);
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    auto signals = fb.getSignals();
    ASSERT_EQ(signals.getCount(), 1);
    const auto expectedComponentStatus = result ? "Ok" : "Error";
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", expectedComponentStatus, daqInstance.getContext().getTypeManager()));
}

INSTANTIATE_TEST_SUITE_P(TopicTest,
                         MqttSubscriberFbTopicPTest,
                         ::testing::Values(std::make_pair("", false),
                                           std::make_pair("goodTopic/test", true),
                                           std::make_pair("/goodTopic/test0", true),
                                           std::make_pair("badTopic/+/test/topic", false),
                                           std::make_pair("badTopic/+/+/topic", false),
                                           std::make_pair("badTopic/#", false)));

TEST_F(MqttSubscriberFbTest, RemovingNestedFunctionBlock)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, String(buildTopicName())));
        ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
        ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    daq::FunctionBlockPtr jsonDecoderFb;
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_DEC_VALUE_NAME, String("temp")));
        ASSERT_NO_THROW(jsonDecoderFb = subFb.addFunctionBlock(JSON_DECODER_FB_NAME, config));
    }
    ASSERT_EQ(subFb.getFunctionBlocks().getCount(), 1u);

    ASSERT_NO_THROW(subFb.removeFunctionBlock(jsonDecoderFb));
    ASSERT_EQ(subFb.getFunctionBlocks().getCount(), 0u);
}

TEST_F(MqttSubscriberFbTest, TwoFbCreation)
{
    StartUp();
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, buildTopicName("0")));
        ASSERT_NO_THROW(fb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
        EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    {
        daq::FunctionBlockPtr fb;
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, buildTopicName("1")));
        ASSERT_NO_THROW(fb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
        EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    }
    auto fbs = clientMqttFb.getFunctionBlocks();
    ASSERT_EQ(fbs.getCount(), 2u);
}

TEST_F(MqttSubscriberFbTest, PropertyChanged)
{
    StartUp();

    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    auto topic = buildTopicName("0");
    config.addProperty(StringProperty(PROPERTY_NAME_SUB_TOPIC, topic));
    ASSERT_NO_THROW(fb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto subFb = reinterpret_cast<MqttSubscriberFbImpl*>(*fb);

    ASSERT_EQ(topic, subFb->getSubscribedTopic());
    topic = buildTopicName("1");
    fb.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic);
    ASSERT_EQ(topic, subFb->getSubscribedTopic());
}

TEST_F(MqttSubscriberFbTest, JsonInit0)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG, String(VALID_JSON_1_TOPIC_0));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getFunctionBlocks().getCount(), 3u);
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), value);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(subFb.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().toStdString(), "openDAQ/RefDev0/IO/AI/RefCh0/Sig/AI0");

    lambda(subFb.getFunctionBlocks()[0], "value", "timestamp", "V");
    lambda(subFb.getFunctionBlocks()[1], "value1", "", "");
    lambda(subFb.getFunctionBlocks()[2], "value2", "", "W");

}

TEST_F(MqttSubscriberFbTest, JsonInit1)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG, String(VALID_JSON_1_TOPIC_1));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getFunctionBlocks().getCount(), 3u);
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), value);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(subFb.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().toStdString(), "/mirip/UNet3AC2/sensor/data");

    lambda(subFb.getFunctionBlocks()[0], "temp", "ts", "°C");
    lambda(subFb.getFunctionBlocks()[1], "humi", "ts", "%");
    lambda(subFb.getFunctionBlocks()[2], "tds_value", "ts", "ppm");

}

TEST_P(MqttSubscriberFbConfigPTest, JsonWrongInit)
{
    const auto configJson = GetParam();
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG, String(configJson));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    EXPECT_EQ(subFb.getFunctionBlocks().getCount(), 0u);
    EXPECT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_EQ(subFb.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().toStdString(), "");
}

INSTANTIATE_TEST_SUITE_P(
    JsonConfigTest,
    MqttSubscriberFbConfigPTest,
    ::testing::Values(
        VALID_JSON_3_TOPIC_2,
        WILDCARD_JSON_0,
        WILDCARD_JSON_1,
        INVALID_JSON_1,
        INVALID_JSON_3));

TEST_P(MqttSubscriberFbConfigFilePTest, JsonInitFromFile)
{
    const auto configJson = GetParam();
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG_FILE, String(configJson));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
}

INSTANTIATE_TEST_SUITE_P(JsonConfigTest,
                         MqttSubscriberFbConfigFilePTest,
                         ::testing::Values("data/public-example0.json",
                                           "data/public-example1.json",
                                           "data/public-example2.json",
                                           "data/public-example3.json"));

TEST_F(MqttSubscriberFbTest, JsonInitFromFileWithChecking)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG_FILE, String("data/public-example0.json"));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    ASSERT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    ASSERT_EQ(subFb.getFunctionBlocks().getCount(), 3u);
    auto lambda = [&](FunctionBlockPtr nestedFb, std::string value, std::string ts, std::string symbol)
    {
        EXPECT_EQ(nestedFb.getSignals()[0].getName().toStdString(), value);
        if (!symbol.empty())
            EXPECT_EQ(nestedFb.getSignals()[0].getDescriptor().getUnit().getSymbol().toStdString(), symbol);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_VALUE_NAME).asPtr<IString>().toStdString(), value);
        EXPECT_EQ(nestedFb.getPropertyValue(PROPERTY_NAME_DEC_TS_NAME).asPtr<IString>().toStdString(), ts);
    };
    EXPECT_EQ(subFb.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().toStdString(), "/mirip/UNet3AC2/sensor/data");

    lambda(subFb.getFunctionBlocks()[0], "temp", "ts", "°C");
    lambda(subFb.getFunctionBlocks()[1], "humi", "ts", "%");
    lambda(subFb.getFunctionBlocks()[2], "tds_value", "ts", "ppm");

}

TEST_F(MqttSubscriberFbTest, JsonInitFromFileWrongPath)
{
    StartUp();
    daq::FunctionBlockPtr subFb;
    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_JSON_CONFIG_FILE, String("/justWrongPath/wrongFile.txt"));
    ASSERT_NO_THROW(subFb = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config));
    EXPECT_EQ(subFb.getFunctionBlocks().getCount(), 0u);
    EXPECT_EQ(subFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager()));
    EXPECT_EQ(subFb.getPropertyValue(PROPERTY_NAME_SUB_TOPIC).asPtr<IString>().toStdString(), "");
}

TEST_F(MqttSubscriberFbTest, DataTransfer)
{
    const auto topic = buildTopicName();
    const auto dataToSend = std::vector<std::vector<uint8_t>>{std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05},
                                                              std::vector<uint8_t>{0x11, 0x12, 0x13, 0x14},
                                                              std::vector<uint8_t>{0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                                                              std::vector<uint8_t>{0x31},
                                                              std::vector<uint8_t>{0x41, 0x42, 0x43, 0x44, 0x45}};
    std::vector<std::vector<uint8_t>> dataToReceive;
    std::vector<uint64_t> tsToReceive;
    std::vector<uint64_t> timePoints;

    CreateSubFB(topic, true, SDSM::SystemTime);

    auto signalList = getSignals();
    auto reader = daq::PacketReader(signalList[0]);
    auto getTime = []()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
    };

    timePoints.push_back(getTime());
    for (const auto& data : dataToSend)
    {
        mqtt::MqttMessage msg = {topic, data, 1, 0};
        onSignalsMessage(msg);
        timePoints.push_back(getTime());
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
            const auto domainPacket = dataPacket.getDomainPacket();
            if (domainPacket.assigned())
                tsToReceive.push_back(*(reinterpret_cast<uint64_t*>(domainPacket.getRawData())));
        }
    }
    ASSERT_EQ(dataToSend.size(), dataToReceive.size());
    ASSERT_EQ(dataToSend, dataToReceive);

    ASSERT_EQ(dataToSend.size(), tsToReceive.size());
    ASSERT_EQ(timePoints.size(), tsToReceive.size() + 1);
    for (size_t i = 0; i < tsToReceive.size(); ++i)
    {
        EXPECT_GE(tsToReceive[i], timePoints[i]);
        EXPECT_LE(tsToReceive[i], timePoints[i + 1]);
    }
}

TEST_F(MqttSubscriberFbTest, CheckRawFbFullDataTransfer)
{
    const std::string topic = buildTopicName();

    StartUp();

    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic);
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    auto singal = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config).getSignals()[0];
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
    helper::utils::Timer tmr(3000, true);
    while ((!reader.getEmpty() || !tmr.expired()) && dataToReceive.size() != dataToSend.size())
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

TEST_F(MqttSubscriberFbTest, CheckRawFbFullDataTransferWithReconfiguring)
{
    const std::string topic0 = buildTopicName("0");
    const std::string topic1 = buildTopicName("1");
    const auto dataToSend = std::vector<std::vector<uint8_t>>{std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05},
                                                              std::vector<uint8_t>{0x11, 0x12, 0x13, 0x14}};
    std::vector<std::vector<uint8_t>> dataToReceive;

    StartUp();

    auto config = clientMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic0);
    config.setPropertyValue(PROPERTY_NAME_SUB_PREVIEW_SIGNAL, True);
    auto rawFB = clientMqttFb.addFunctionBlock(SUB_FB_NAME, config);
    auto singal = rawFB.getSignals()[0];
    auto reader = daq::PacketReader(singal);

    MqttAsyncClientWrapper publisher("testPublisherId");
    ASSERT_TRUE(publisher.connect("127.0.0.1"));
    EXPECT_NE(rawFB.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Waiting for data"), std::string::npos);

    mqtt::MqttMessage msg = {topic0, dataToSend[0], 2, 0};
    ASSERT_TRUE(publisher.publishMsg(msg));

    auto readerLambda = [&reader, &dataToReceive]()
    {
        helper::utils::Timer tmr(1000, true);
        while (!reader.getEmpty() || !tmr.expired())
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
    };
    helper::utils::Timer tmr(1000, true);

    bool hasData = false;
    while (tmr.expired() == false && hasData == false)
        hasData = (rawFB.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Data has been received") != std::string::npos);


    EXPECT_TRUE(hasData);

    readerLambda();
    ASSERT_EQ(dataToReceive.size(), 1u);
    ASSERT_EQ(dataToSend[0], dataToReceive[0]);

    dataToReceive.clear();

    ASSERT_NO_THROW(rawFB.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, topic1));
    EXPECT_EQ(rawFB.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_NE(rawFB.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Waiting for data"), std::string::npos);

    msg = {topic1, dataToSend[1], 2, 0};
    ASSERT_TRUE(publisher.publishMsg(msg));
    tmr.restart();

    hasData = false;
    while (tmr.expired() == false && hasData == false)
        hasData = (rawFB.getStatusContainer().getStatusMessage("ComponentStatus").toStdString().find("Data has been received") != std::string::npos);

    EXPECT_TRUE(hasData);

    readerLambda();
    ASSERT_EQ(dataToReceive.size(), 1u);
    ASSERT_EQ(dataToSend[1], dataToReceive[0]);
}

