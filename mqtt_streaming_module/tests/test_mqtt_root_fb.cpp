#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace daq::modules::mqtt_streaming_module
{
class MqttFbTest : public testing::Test, public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

TEST_F(MqttFbTest, DefaultMqttFbConfig)
{
    const auto module = CreateModule();

    DictPtr<IString, IFunctionBlockType> types;
    ASSERT_NO_THROW(types = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(types.getCount(), 1u);

    ASSERT_TRUE(types.hasKey(ROOT_FB_NAME));
    auto defaultConfig = types.get(ROOT_FB_NAME).createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 5u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_PORT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_PASSWORD));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CONNECT_TIMEOUT));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_PORT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_PASSWORD).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CONNECT_TIMEOUT).getValueType(), CoreType::ctInt);

    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS), DEFAULT_BROKER_ADDRESS);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT), DEFAULT_PORT);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME), DEFAULT_USERNAME);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD), DEFAULT_PASSWORD);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CONNECT_TIMEOUT), DEFAULT_INIT_TIMEOUT);
}

TEST_F(MqttFbTest, CreatingMqttFbWithDefaultConfig)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));

    auto fbs = instance.getFunctionBlocks();
    bool contain = false;
    daq::FunctionBlockPtr fbFromList;
    for (const auto& fbInst : fbs)
    {
        contain = (fbInst.getName() == std::string(MQTT_LOCAL_ROOT_FB_ID_PREFIX) + std::to_string(0));
        if (contain)
        {
            fbFromList = fbInst;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(fbFromList.assigned());
    ASSERT_TRUE(fbFromList == fb);
}

TEST_F(MqttFbTest, CreatingMqttFbWithCustomConfig)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    auto config = DaqMqttFbConfig();
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttFbTest, CreatingMqttFbWithEmptyConfig)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttFbTest, CreatingMqttFbWithPartialConfig)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_CONNECT_TIMEOUT, 1000));
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttFbTest, CreatingSeveralMqttFbs)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME, DaqMqttFbConfig("127.0.0.1", 1883)));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    daq::FunctionBlockPtr anotherFb;
    ASSERT_NO_THROW(anotherFb = instance.addFunctionBlock(ROOT_FB_NAME, DaqMqttFbConfig("127.0.0.1", 1884)));
    ASSERT_EQ(anotherFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(instance.getFunctionBlocks().getCount(), 2u);
}

TEST_F(MqttFbTest, RemovingMqttFb)
{
    const auto instance = Instance();
    daq::FunctionBlockPtr fb;
    auto config = DaqMqttFbConfig();
    ASSERT_NO_THROW(fb = instance.addFunctionBlock(ROOT_FB_NAME, config));
    ASSERT_NO_THROW(instance.removeFunctionBlock(fb));
}

TEST_F(MqttFbTest, CheckMqttFbFunctionalBlocks)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = rootMqttFb.getAvailableFunctionBlockTypes());
    ASSERT_GE(fbTypes.getCount(), 2);
    ASSERT_TRUE(fbTypes.hasKey(SUB_FB_NAME));
    ASSERT_TRUE(fbTypes.hasKey(PUB_FB_NAME));
}
