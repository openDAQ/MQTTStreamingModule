#include <mqtt_streaming_server_module/module_dll.h>
#include <mqtt_streaming_server_module/version.h>
#include <mqtt_streaming_server_module/constants.h>
#include <opendaq/context_factory.h>
#include <opendaq/instance_factory.h>
#include <opendaq/instance_ptr.h>
#include <opendaq/module_manager_factory.h>
#include <opendaq/module_ptr.h>
#include <coretypes/common.h>
#include <gmock/gmock.h>
#include <testutils/testutils.h>
#include <coreobjects/authentication_provider_factory.h>

using MqttStreamingServerModuleTest = testing::Test;
using namespace daq;
using namespace daq::modules::mqtt_streaming_server_module;

static ModulePtr CreateModule(ContextPtr context = NullContext(), ModuleManagerPtr manager = nullptr)
{
    ModulePtr module;
    createMqttStreamingServerModule(&module, context);
    return module;
}

static InstancePtr CreateTestInstance()
{
    const auto logger = Logger();
    const auto moduleManager = ModuleManager("[[none]]");
    const auto authenticationProvider = AuthenticationProvider();
    const auto context = Context(Scheduler(logger), logger, TypeManager(), moduleManager, authenticationProvider);

    const ModulePtr daqMqttStreamingServerModule = CreateModule(context, moduleManager);
    moduleManager.addModule(daqMqttStreamingServerModule);

    auto instance = InstanceCustom(context, "localInstance");
    for (const auto& deviceInfo : instance.getAvailableDevices())
        instance.addDevice(deviceInfo.getConnectionString());

    for (const auto& [id, _] : instance.getAvailableFunctionBlockTypes())
        instance.addFunctionBlock(id);

    return instance;
}

static PropertyObjectPtr CreateServerConfig(const InstancePtr& instance)
{
    auto config = instance.getAvailableServerTypes().get(SERVER_ID_AND_CAPABILITY).createDefaultConfig();
    return config;
}

TEST_F(MqttStreamingServerModuleTest, CreateModule)
{
    IModule* module = nullptr;
    ErrCode errCode = createModule(&module, NullContext());
    ASSERT_TRUE(OPENDAQ_SUCCEEDED(errCode));

    ASSERT_NE(module, nullptr);
    module->releaseRef();
}

TEST_F(MqttStreamingServerModuleTest, ModuleName)
{
    auto module = CreateModule();
    ASSERT_EQ(module.getModuleInfo().getName(), MODULE_NAME);
}

TEST_F(MqttStreamingServerModuleTest, VersionAvailable)
{
    auto module = CreateModule();
    ASSERT_TRUE(module.getModuleInfo().getVersionInfo().assigned());
}

TEST_F(MqttStreamingServerModuleTest, VersionCorrect)
{
    auto module = CreateModule();
    auto version = module.getModuleInfo().getVersionInfo();

    ASSERT_EQ(version.getMajor(), MQTT_STREAM_SRV_MODULE_MAJOR_VERSION);
    ASSERT_EQ(version.getMinor(), MQTT_STREAM_SRV_MODULE_MINOR_VERSION);
    ASSERT_EQ(version.getPatch(), MQTT_STREAM_SRV_MODULE_PATCH_VERSION);
}

TEST_F(MqttStreamingServerModuleTest, GetAvailableComponentTypes)
{
    const auto module = CreateModule();

    DictPtr<IString, IFunctionBlockType> functionBlockTypes;
    ASSERT_NO_THROW(functionBlockTypes = module.getAvailableFunctionBlockTypes());
    ASSERT_EQ(functionBlockTypes.getCount(), 0u);

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 0u);

    DictPtr<IString, IServerType> serverTypes;
    ASSERT_NO_THROW(serverTypes = module.getAvailableServerTypes());
    ASSERT_EQ(serverTypes.getCount(), 1u);
    ASSERT_TRUE(serverTypes.hasKey(SERVER_ID_AND_CAPABILITY));
    ASSERT_EQ(serverTypes.get(SERVER_ID_AND_CAPABILITY).getId(), SERVER_ID_AND_CAPABILITY);

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
    ASSERT_EQ(versionInfoModule.getMajor(), MQTT_STREAM_SRV_MODULE_MAJOR_VERSION);
    ASSERT_EQ(versionInfoModule.getMinor(), MQTT_STREAM_SRV_MODULE_MINOR_VERSION);
    ASSERT_EQ(versionInfoModule.getPatch(), MQTT_STREAM_SRV_MODULE_PATCH_VERSION);

    // Check module version info for server types
    for (const auto& serverType : serverTypes)
    {
        ModuleInfoPtr moduleInfoServerType;
        ASSERT_NO_THROW(moduleInfoServerType = serverType.second.getModuleInfo());
        ASSERT_NE(moduleInfoServerType, nullptr);
        ASSERT_EQ(moduleInfoServerType.getName(), MODULE_NAME);
        ASSERT_EQ(moduleInfoServerType.getId(), MODULE_ID);

        VersionInfoPtr versionInfoServerType;
        ASSERT_NO_THROW(versionInfoServerType = moduleInfoServerType.getVersionInfo());
        ASSERT_NE(versionInfoServerType, nullptr);
        ASSERT_EQ(versionInfoServerType.getMajor(), MQTT_STREAM_SRV_MODULE_MAJOR_VERSION);
        ASSERT_EQ(versionInfoServerType.getMinor(), MQTT_STREAM_SRV_MODULE_MINOR_VERSION);
        ASSERT_EQ(versionInfoServerType.getPatch(), MQTT_STREAM_SRV_MODULE_PATCH_VERSION);
    }
}

TEST_F(MqttStreamingServerModuleTest, ServerConfig)
{
    auto module = CreateModule();

    DictPtr<IString, IServerType> serverTypes = module.getAvailableServerTypes();
    ASSERT_TRUE(serverTypes.hasKey(SERVER_ID_AND_CAPABILITY));
    auto config = serverTypes.get(SERVER_ID_AND_CAPABILITY).createDefaultConfig();
    ASSERT_TRUE(config.assigned());

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS), DEFAULT_BROKER_ADDRESS);

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_MQTT_BROKER_PORT));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT), DEFAULT_PORT);

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_MQTT_USERNAME));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME), DEFAULT_USERNAME);

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_MQTT_PASSWORD));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD), DEFAULT_PASSWORD);

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_MAX_PACKET_READ_COUNT));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_MAX_PACKET_READ_COUNT), DEFAULT_MAX_PACKET_READ_COUNT);

    ASSERT_TRUE(config.hasProperty(PROPERTY_NAME_POLLING_PERIOD));
    ASSERT_EQ(config.getPropertyValue(PROPERTY_NAME_POLLING_PERIOD), DEFAULT_POLLING_PERIOD);
}

TEST_F(MqttStreamingServerModuleTest, CreateServer)
{
    auto device = CreateTestInstance();
    auto module = CreateModule(device.getContext());
    auto config = CreateServerConfig(device);

    ASSERT_NO_THROW(module.createServer(SERVER_ID_AND_CAPABILITY, device.getRootDevice(), config));
}

TEST_F(MqttStreamingServerModuleTest, CreateServerFromInstance)
{
    auto device = CreateTestInstance();
    auto config = CreateServerConfig(device);

    ASSERT_NO_THROW(device.addServer(SERVER_ID_AND_CAPABILITY, config));
}
