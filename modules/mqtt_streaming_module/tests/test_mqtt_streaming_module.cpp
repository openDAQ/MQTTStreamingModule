#include "test_daq_test_helper.h"
#include <mqtt_streaming_module/constants.h>
#include <mqtt_streaming_module/module_dll.h>
#include <mqtt_streaming_module/version.h>
#include <mqtt_streaming_module/mqtt_streaming_module_impl.h>
#include <mqtt_streaming_module/helper.h>
#include <opendaq/context_factory.h>
#include <testutils/testutils.h>

using namespace daq;
using namespace daq::modules::mqtt_streaming_module;

namespace daq::modules::mqtt_streaming_module
{
class MqttStreamingClientModuleTest : public testing::Test, public DaqTestHelper
{
};
} // namespace daq::modules::mqtt_streaming_module

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
    ASSERT_EQ(module.getModuleInfo().getName(), daq::modules::mqtt_streaming_module::MODULE_NAME);
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

    ASSERT_EQ(version.getMajor(), MQTT_STREAM_MODULE_MAJOR_VERSION);
    ASSERT_EQ(version.getMinor(), MQTT_STREAM_MODULE_MINOR_VERSION);
    ASSERT_EQ(version.getPatch(), MQTT_STREAM_MODULE_PATCH_VERSION);
}

TEST_F(MqttStreamingClientModuleTest, MqttDeviceAvailable)
{
    auto module = CreateModule();

    DictPtr<daq::IString, daq::IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);
}

TEST_F(MqttStreamingClientModuleTest, GetAvailableComponentTypes)
{
    const auto module = CreateModule();

    // The MQTT client is a device now; the module offers no top-level function block type.
    DictPtr<IString, IFunctionBlockType> functionBlockTypes;
    ASSERT_NO_THROW(functionBlockTypes = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(functionBlockTypes.getCount(), 0u);

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);
    ASSERT_TRUE(deviceTypes.hasKey(CLIENT_DEVICE_TYPE_ID));
    ASSERT_EQ(deviceTypes.get(CLIENT_DEVICE_TYPE_ID).getId(), CLIENT_DEVICE_TYPE_ID);
    ASSERT_EQ(deviceTypes.get(CLIENT_DEVICE_TYPE_ID).getConnectionStringPrefix(), CLIENT_DEVICE_CONN_PREFIX);


    DictPtr<IString, IServerType> serverTypes;
    ASSERT_NO_THROW(serverTypes = module.getAvailableServerTypes());
    ASSERT_EQ(serverTypes.getCount(), 0u);

    {
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
        ASSERT_EQ(versionInfoModule.getMajor(), MQTT_STREAM_MODULE_MAJOR_VERSION);
        ASSERT_EQ(versionInfoModule.getMinor(), MQTT_STREAM_MODULE_MINOR_VERSION);
        ASSERT_EQ(versionInfoModule.getPatch(), MQTT_STREAM_MODULE_PATCH_VERSION);
    }

    // Check module and version info for device types
    for (const auto& devType : deviceTypes)
    {
        ModuleInfoPtr moduleInfo;
        ASSERT_NO_THROW(moduleInfo = devType.second.getModuleInfo());
        ASSERT_NE(moduleInfo, nullptr);
        ASSERT_EQ(moduleInfo.getName(), MODULE_NAME);
        ASSERT_EQ(moduleInfo.getId(), MODULE_ID);

        VersionInfoPtr versionInfo;
        ASSERT_NO_THROW(versionInfo = moduleInfo.getVersionInfo());
        ASSERT_NE(versionInfo, nullptr);
        ASSERT_EQ(versionInfo.getMajor(), MQTT_STREAM_MODULE_MAJOR_VERSION);
        ASSERT_EQ(versionInfo.getMinor(), MQTT_STREAM_MODULE_MINOR_VERSION);
        ASSERT_EQ(versionInfo.getPatch(), MQTT_STREAM_MODULE_PATCH_VERSION);
    }
}
