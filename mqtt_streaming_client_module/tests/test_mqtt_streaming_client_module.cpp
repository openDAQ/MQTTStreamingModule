#include "test_daq_test_helper.h"
#include <coretypes/common.h>
#include <mqtt_streaming_client_module/constants.h>
#include <mqtt_streaming_client_module/module_dll.h>
#include <mqtt_streaming_client_module/version.h>
#include <opendaq/context_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

namespace daq::modules::mqtt_streaming_client_module
{
class MqttStreamingClientModuleTest : public testing::Test, public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_client_module

TEST_F(MqttStreamingClientModuleTest, CreateModule)
{
    IModule* module = nullptr;
    ErrCode errCode = createModule(&module, NullContext());
    ASSERT_TRUE(OPENDAQ_SUCCEEDED(errCode));

    ASSERT_NE(module, nullptr);
    module->releaseRef();
}

TEST_F(MqttStreamingClientModuleTest, ModuleName)
{
    auto module = CreateModule();
    ASSERT_EQ(module.getModuleInfo().getName(), daq::modules::mqtt_streaming_client_module::MODULE_NAME);
}

TEST_F(MqttStreamingClientModuleTest, VersionAvailable)
{
    auto module = CreateModule();
    ASSERT_TRUE(module.getModuleInfo().getVersionInfo().assigned());
}

TEST_F(MqttStreamingClientModuleTest, VersionCorrect)
{
    auto module = CreateModule();
    auto version = module.getModuleInfo().getVersionInfo();

    ASSERT_EQ(version.getMajor(), MQTT_STREAM_CLI_MODULE_MAJOR_VERSION);
    ASSERT_EQ(version.getMinor(), MQTT_STREAM_CLI_MODULE_MINOR_VERSION);
    ASSERT_EQ(version.getPatch(), MQTT_STREAM_CLI_MODULE_PATCH_VERSION);
}

TEST_F(MqttStreamingClientModuleTest, EnumerateDevices)
{
    auto module = CreateModule();

    ListPtr<IDeviceInfo> deviceInfo;
    ASSERT_NO_THROW(deviceInfo = module.getAvailableDevices());
}

TEST_F(MqttStreamingClientModuleTest, CreateDeviceConnectionStringNull)
{
    auto module = CreateModule();

    DevicePtr device;
    ASSERT_THROW(device = module.createDevice(nullptr, nullptr), ArgumentNullException);
}

TEST_F(MqttStreamingClientModuleTest, GetAvailableComponentTypes)
{
    const auto module = CreateModule();

    DictPtr<IString, IFunctionBlockType> functionBlockTypes;
    ASSERT_NO_THROW(functionBlockTypes = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(functionBlockTypes.getCount(), 0u);

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);
    ASSERT_TRUE(deviceTypes.hasKey(DaqMqttDeviceTypeId));
    ASSERT_EQ(deviceTypes.get(DaqMqttDeviceTypeId).getId(), DaqMqttDeviceTypeId);

    DictPtr<IString, IServerType> serverTypes;
    ASSERT_NO_THROW(serverTypes = module.getAvailableServerTypes());
    ASSERT_EQ(serverTypes.getCount(), 0u);

    // Check module info for module
    ModuleInfoPtr moduleInfo;
    ASSERT_NO_THROW(moduleInfo = module.getModuleInfo());
    ASSERT_NE(moduleInfo, nullptr);
    ASSERT_EQ(moduleInfo.getName(), MODULE_NAME);
    ASSERT_EQ(moduleInfo.getId(), MODULE_ID);

    // Check version info for module
    VersionInfoPtr versionInfoModule;
    ASSERT_NO_THROW(versionInfoModule = moduleInfo.getVersionInfo());
    ASSERT_NE(versionInfoModule, nullptr);
    ASSERT_EQ(versionInfoModule.getMajor(), MQTT_STREAM_CLI_MODULE_MAJOR_VERSION);
    ASSERT_EQ(versionInfoModule.getMinor(), MQTT_STREAM_CLI_MODULE_MINOR_VERSION);
    ASSERT_EQ(versionInfoModule.getPatch(), MQTT_STREAM_CLI_MODULE_PATCH_VERSION);

    // Check module and version info for device types
    for (const auto& deviceType : deviceTypes)
    {
        ModuleInfoPtr moduleInfoDeviceType;
        ASSERT_NO_THROW(moduleInfoDeviceType = deviceType.second.getModuleInfo());
        ASSERT_NE(moduleInfoDeviceType, nullptr);
        ASSERT_EQ(moduleInfoDeviceType.getName(), MODULE_NAME);
        ASSERT_EQ(moduleInfoDeviceType.getId(), MODULE_ID);

        VersionInfoPtr versionInfoDeviceType;
        ASSERT_NO_THROW(versionInfoDeviceType = moduleInfoDeviceType.getVersionInfo());
        ASSERT_NE(versionInfoDeviceType, nullptr);
        ASSERT_EQ(versionInfoDeviceType.getMajor(), MQTT_STREAM_CLI_MODULE_MAJOR_VERSION);
        ASSERT_EQ(versionInfoDeviceType.getMinor(), MQTT_STREAM_CLI_MODULE_MINOR_VERSION);
        ASSERT_EQ(versionInfoDeviceType.getPatch(), MQTT_STREAM_CLI_MODULE_PATCH_VERSION);
    }
}
