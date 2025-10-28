#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/instance_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

namespace daq::modules::mqtt_streaming_client_module
{
class MqttDeviceTest : public testing::Test, public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_client_module

TEST_F(MqttDeviceTest, DefaultDeviceConfig)
{
    const auto module = CreateModule();

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);

    ASSERT_TRUE(deviceTypes.hasKey(DaqMqttDeviceTypeId));
    auto defaultConfig = deviceTypes.get(DaqMqttDeviceTypeId).createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 6u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_PORT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_PASSWORD));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CONNECT_TIMEOUT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_DISCOVERY_TIMEOUT));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_PORT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_PASSWORD).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CONNECT_TIMEOUT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_DISCOVERY_TIMEOUT).getValueType(), CoreType::ctInt);

    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS), DEFAULT_BROKER_ADDRESS);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT), DEFAULT_PORT);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME), DEFAULT_USERNAME);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD), DEFAULT_PASSWORD);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CONNECT_TIMEOUT), DEFAULT_INIT_DELAY);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_DISCOVERY_TIMEOUT), DEFAULT_DISCOVERY_DELAY);
}

TEST_F(MqttDeviceTest, CreatingDeviceWithDefaultConfig)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1"));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getInfo().getName(), MQTT_DEVICE_NAME);
    auto devices = instance.getDevices();
    bool contain = false;
    daq::GenericDevicePtr<daq::IDevice> deviceFromList;
    for (const auto& d : devices)
    {
        contain = (d.getName() == MQTT_DEVICE_NAME);
        if (contain)
        {
            deviceFromList = d;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(deviceFromList.assigned());
    ASSERT_EQ(deviceFromList.getInfo().getName(), device.getInfo().getName());
    ASSERT_TRUE(deviceFromList == device);
}

TEST_F(MqttDeviceTest, CreatingDeviceWithCustomConfig)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = DaqMqttDeviceConfig(100);
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, CreatingDeviceWithEmptyConfig)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = PropertyObject();
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, CreatingDeviceWithPartialConfig)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_DISCOVERY_TIMEOUT, 100));
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, CreatingSeveralDevices)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = DaqMqttDeviceConfig(100);
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getInfo().getName(), MQTT_DEVICE_NAME);
    daq::GenericDevicePtr<daq::IDevice> anotherDevice;
    ASSERT_THROW(anotherDevice = instance.addDevice("daq.mqtt://127.0.0.1", config), AlreadyExistsException);
}

TEST_F(MqttDeviceTest, RemovingDevice)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = DaqMqttDeviceConfig(100);
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));
    ASSERT_NO_THROW(instance.removeDevice(device));
}

TEST_F(MqttDeviceTest, CheckDeviceFunctionalBlocks)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
    ASSERT_GE(fbTypes.getCount(), 2);
    ASSERT_TRUE(fbTypes.hasKey(RAW_FB_NAME));
    ASSERT_TRUE(fbTypes.hasKey(JSON_FB_NAME));
}
