#include "test_daq_test_helper.h"
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <gtest/gtest.h>
#include <mqtt_streaming_module/constants.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace daq::modules::mqtt_streaming_module
{
class MqttDeviceTest : public testing::Test, public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

// A port nothing is expected to listen on, used for negative connection tests.
static constexpr uint16_t UNREACHABLE_PORT = 1884;

TEST_F(MqttDeviceTest, DefaultMqttDeviceConfig)
{
    const auto module = CreateModule();

    DictPtr<IString, IDeviceType> types;
    ASSERT_NO_THROW(types = module.getAvailableDeviceTypes());
    ASSERT_EQ(types.getCount(), 1u);

    ASSERT_TRUE(types.hasKey(CLIENT_DEVICE_TYPE_ID));
    const auto deviceType = types.get(CLIENT_DEVICE_TYPE_ID);
    ASSERT_EQ(deviceType.getId(), CLIENT_DEVICE_TYPE_ID);
    ASSERT_EQ(deviceType.getName(), CLIENT_DEVICE_TYPE_NAME);
    ASSERT_EQ(deviceType.getConnectionStringPrefix(), CLIENT_DEVICE_CONN_PREFIX);

    auto defaultConfig = deviceType.createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    // BrokerAddress is gone: the host now comes from the connection string.
    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 4u);

    ASSERT_FALSE(defaultConfig.hasProperty("BrokerAddress"));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CLIENT_BROKER_PORT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CLIENT_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CLIENT_PASSWORD));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CLIENT_BROKER_PORT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CLIENT_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CLIENT_PASSWORD).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT).getValueType(), CoreType::ctInt);

    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT), DEFAULT_PORT);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CLIENT_USERNAME), DEFAULT_USERNAME);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CLIENT_PASSWORD), DEFAULT_PASSWORD);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT), DEFAULT_INIT_TIMEOUT);
}

TEST_F(MqttDeviceTest, ModuleExposesNoFunctionBlockTypes)
{
    const auto module = CreateModule();

    // The client is a device now; the module no longer offers any top-level function block.
    DictPtr<IString, IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(fbTypes.getCount(), 0u);
}

TEST_F(MqttDeviceTest, MissingPasswordProperty)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString()));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));

    ASSERT_FALSE(device.hasProperty(PROPERTY_NAME_CLIENT_PASSWORD));
}

TEST_F(MqttDeviceTest, CreatingMqttDeviceWithDefaultConfig)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString()));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));

    // The client now lives under the instance's devices, not its function blocks.
    ASSERT_EQ(instance.getFunctionBlocks().getCount(), 0u);

    auto devices = instance.getDevices();
    bool contain = false;
    DevicePtr deviceFromList;
    for (const auto& dev : devices)
    {
        contain = (dev.getLocalId().toStdString().find(MQTT_LOCAL_CLIENT_DEVICE_ID_PREFIX) != std::string::npos);
        if (contain)
        {
            deviceFromList = dev;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(deviceFromList.assigned());
    ASSERT_TRUE(deviceFromList == device);
}

TEST_F(MqttDeviceTest, CreatingMqttDeviceWithCustomConfig)
{
    const auto instance = Instance();
    DevicePtr device;
    auto config = DaqMqttDeviceConfig();
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString(), config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, CreatingMqttDeviceWithEmptyConfig)
{
    const auto instance = Instance();
    DevicePtr device;
    auto config = PropertyObject();
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString(), config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, CreatingMqttDeviceWithPartialConfig)
{
    const auto instance = Instance();
    DevicePtr device;
    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT, 1000));
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString(), config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getPropertyValue(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT), 1000);
}

TEST_F(MqttDeviceTest, CreatingSeveralMqttDevices)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString("127.0.0.1", 1883)));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    DevicePtr anotherDevice;
    ASSERT_NO_THROW(anotherDevice = instance.addDevice(MqttConnectionString("127.0.0.1", 1883)));
    ASSERT_EQ(anotherDevice.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(instance.getDevices().getCount(), 2u);
}

TEST_F(MqttDeviceTest, RemovingMqttDevice)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(MqttConnectionString(), DaqMqttDeviceConfig()));
    ASSERT_EQ(instance.getDevices().getCount(), 1u);
    ASSERT_NO_THROW(instance.removeDevice(device));
    ASSERT_EQ(instance.getDevices().getCount(), 0u);
}

TEST_F(MqttDeviceTest, CheckMqttDeviceFunctionalBlocks)
{
    StartUp();
    DictPtr<IString, IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = mqttDevice.getAvailableFunctionBlockTypes());
    ASSERT_EQ(fbTypes.getCount(), 2u);
    ASSERT_TRUE(fbTypes.hasKey(SUB_FB_NAME));
    ASSERT_TRUE(fbTypes.hasKey(PUB_FB_NAME));
}

// ---------------------------------------------------------------------------
// Connection string handling
// ---------------------------------------------------------------------------

TEST_F(MqttDeviceTest, ConnectionStringWithPort)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1:1883"));

    ASSERT_EQ(device.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT), 1883);
    ASSERT_EQ(device.getInfo().getConnectionString(), "daq.mqtt://127.0.0.1:1883");
}

TEST_F(MqttDeviceTest, ConnectionStringWithoutPortFallsBackToProperty)
{
    const auto instance = Instance();
    auto config = DaqMqttDeviceConfig();
    config.setPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT, 1883);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1", config));

    ASSERT_EQ(device.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT), 1883);
    // The normalised connection string always carries the effective port.
    ASSERT_EQ(device.getInfo().getConnectionString(), "daq.mqtt://127.0.0.1:1883");
}

TEST_F(MqttDeviceTest, ConnectionStringPortBeatsProperty)
{
    const auto instance = Instance();
    auto config = DaqMqttDeviceConfig();
    // Points at a port nothing listens on; the connection string must win, so the device connects.
    config.setPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT, UNREACHABLE_PORT);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1:1883", config));

    // The property reflects the port actually used, not the one that came in through the config.
    ASSERT_EQ(device.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT), 1883);
    ASSERT_EQ(device.getInfo().getConnectionString(), "daq.mqtt://127.0.0.1:1883");
}

TEST_F(MqttDeviceTest, ConnectionStringIpv6)
{
    const auto instance = Instance();
    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://[::1]:1883"));

    ASSERT_EQ(device.getInfo().getConnectionString(), "daq.mqtt://[::1]:1883");
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, UnknownConnectionStringPrefixThrows)
{
    const auto instance = Instance();
    ASSERT_ANY_THROW(instance.addDevice("daq.nosuchproto://127.0.0.1:1883"));
}

TEST_F(MqttDeviceTest, UnreachableBrokerThrows)
{
    const auto instance = Instance();
    auto config = DaqMqttDeviceConfig();
    config.setPropertyValue(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT, 500);

    ASSERT_ANY_THROW(instance.addDevice(MqttConnectionString("127.0.0.1", UNREACHABLE_PORT), config));
    ASSERT_EQ(instance.getDevices().getCount(), 0u);
}

// ---------------------------------------------------------------------------
// Device info and connection status
// ---------------------------------------------------------------------------

TEST_F(MqttDeviceTest, DeviceInfoContent)
{
    StartUp();

    const auto info = mqttDevice.getInfo();
    ASSERT_TRUE(info.assigned());
    ASSERT_EQ(info.getConnectionString(), MqttConnectionString());
    ASSERT_EQ(info.getName(), CLIENT_DEVICE_TYPE_NAME);

    const auto deviceType = info.getDeviceType();
    ASSERT_TRUE(deviceType.assigned());
    ASSERT_EQ(deviceType.getId(), CLIENT_DEVICE_TYPE_ID);
    ASSERT_EQ(deviceType.getConnectionStringPrefix(), CLIENT_DEVICE_CONN_PREFIX);

    const auto connectionInfo = info.getConfigurationConnectionInfo();
    ASSERT_TRUE(connectionInfo.assigned());
    ASSERT_EQ(connectionInfo.getProtocolId(), CLIENT_DEVICE_TYPE_ID);
    ASSERT_EQ(connectionInfo.getProtocolType(), ProtocolType::Unknown);
    ASSERT_EQ(connectionInfo.getConnectionType(), "TCP/IP");
    ASSERT_EQ(connectionInfo.getPort(), DEFAULT_PORT);
    ASSERT_EQ(connectionInfo.getPrefix(), CLIENT_DEVICE_CONN_PREFIX);
    ASSERT_EQ(connectionInfo.getConnectionString(), MqttConnectionString());
    ASSERT_EQ(connectionInfo.getAddresses().getCount(), 1u);
    ASSERT_EQ(connectionInfo.getAddresses()[0], DEFAULT_BROKER_ADDRESS);
}

TEST_F(MqttDeviceTest, ConfigurationStatusConnected)
{
    StartUp();

    const auto statuses = mqttDevice.getConnectionStatusContainer();
    ASSERT_TRUE(statuses.assigned());
    ASSERT_TRUE(statuses.getStatuses().hasKey("ConfigurationStatus"));
    ASSERT_EQ(statuses.getStatus("ConfigurationStatus"),
              Enumeration("ConnectionStatusType", "Connected", daqInstance.getContext().getTypeManager()));
}

TEST_F(MqttDeviceTest, ConnectionStatusKeyedByConnectionString)
{
    StartUp();

    // The container is keyed by connection string; the alias is what clients read.
    const auto statuses = mqttDevice.getConnectionStatusContainer().getStatuses();
    ASSERT_EQ(statuses.getCount(), 1u);
    ASSERT_TRUE(statuses.hasKey("ConfigurationStatus"));
}
