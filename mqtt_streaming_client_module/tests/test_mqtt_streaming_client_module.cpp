#include "mqtt_streaming_client_module/mqtt_raw_receiver_fb_impl.h"
#include <coreobjects/authentication_provider_factory.h>
#include <gmock/gmock.h>
#include <mqtt_streaming_client_module/constants.h>
#include <mqtt_streaming_client_module/module_dll.h>
#include <mqtt_streaming_client_module/version.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/instance_factory.h>
#include <opendaq/reader_factory.h>
#include <testutils/testutils.h>

#include <coretypes/common.h>
#include <opendaq/module_ptr.h>

#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <opendaq/context_factory.h>
#include <opendaq/device_info_factory.h>
#include "MqttAsyncClientWrapper.h"

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;
namespace daq::modules::mqtt_streaming_client_module
{
class MqttStreamingClientModuleTest : public testing::Test
{
public:
    std::unique_ptr<MqttRawReceiverFbImpl> obj;

    void onSignalsMessage(mqtt::MqttMessage& msg)
    {
        mqtt::MqttAsyncClient unused;
        obj->onSignalsMessage(unused, msg);
    }

    void CreateRawFB(std::vector<std::string> topics)
    {
        auto config = PropertyObject();
        config.addProperty(ListProperty(PROPERTY_NAME_SIGNAL_LIST, List<IString>()));
        const auto fbType = FunctionBlockType(RAW_FB_NAME, RAW_FB_NAME, "", config);
        auto topicList = List<IString>();
        for (auto& topic : topics)
        {
            addToList(topicList, std::move(topic));
        }
        config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
        obj = std::make_unique<MqttRawReceiverFbImpl>(NullContext(), nullptr, fbType, "localId", nullptr, config);
    }

    std::string buildTopicName() {
        return std::string("test/topic/") + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name());
    }
};
} // namespace daq::modules::mqtt_streaming_client_module

static ModulePtr CreateModule()
{
    ModulePtr module;
    createModule(&module, NullContext());
    return module;
}

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

TEST_F(MqttStreamingClientModuleTest, DefaultDeviceConfig)
{
    const auto module = CreateModule();

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);

    ASSERT_TRUE(deviceTypes.hasKey(DaqMqttDeviceTypeId));
    auto defaultConfig = deviceTypes.get(DaqMqttDeviceTypeId).createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 5u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_BROKER_PORT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_MQTT_PASSWORD));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_INIT_DELAY));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_BROKER_PORT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_MQTT_PASSWORD).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_INIT_DELAY).getValueType(), CoreType::ctInt);

    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS), DEFAULT_BROKER_ADDRESS);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT), DEFAULT_PORT);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME), DEFAULT_USERNAME);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD), DEFAULT_PASSWORD);
    ASSERT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_INIT_DELAY), DEFAULT_INIT_DELAY);
}

TEST_F(MqttStreamingClientModuleTest, CreatingDevice)
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

TEST_F(MqttStreamingClientModuleTest, CreatingSeveralDevices)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1"));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getInfo().getName(), MQTT_DEVICE_NAME);
    daq::GenericDevicePtr<daq::IDevice> anotherDevice;
    ASSERT_THROW(anotherDevice = instance.addDevice("daq.mqtt://127.0.0.1"), AlreadyExistsException);
}

TEST_F(MqttStreamingClientModuleTest, RemovingDevice)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.mqtt://127.0.0.1"));
    ASSERT_NO_THROW(instance.removeDevice(device));
}

TEST_F(MqttStreamingClientModuleTest, CheckDeviceFunctionalBlocks)
{
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
    ASSERT_GE(fbTypes.getCount(), 2);
    ASSERT_TRUE(fbTypes.hasKey(RAW_FB_NAME));
    ASSERT_TRUE(fbTypes.hasKey(JSON_FB_NAME));
}

TEST_F(MqttStreamingClientModuleTest, DefaultRawFbConfig)
{
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    daq::FunctionBlockTypePtr fbt;
    daq::PropertyObjectPtr defaultConfig;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
    ASSERT_NO_THROW(fbt = fbTypes.get(RAW_FB_NAME));
    ASSERT_NO_THROW(defaultConfig = fbt.createDefaultConfig());

    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 1u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_SIGNAL_LIST));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_SIGNAL_LIST).getValueType(), CoreType::ctList);
    ASSERT_TRUE(defaultConfig.getPropertyValue(PROPERTY_NAME_SIGNAL_LIST).asPtr<IList>().empty());
}

TEST_F(MqttStreamingClientModuleTest, CreateRawFunctionalBlocks)
{
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = device.addFunctionBlock(RAW_FB_NAME));
    ASSERT_EQ(rawFb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(rawFb.getName(), RAW_FB_NAME);
    auto fbs = device.getFunctionBlocks();
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

TEST_F(MqttStreamingClientModuleTest, CheckRawFbEmptySignalList)
{
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = device.addFunctionBlock(RAW_FB_NAME));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), 0u);
}

TEST_F(MqttStreamingClientModuleTest, CheckRawFbSignalList)
{
    constexpr uint NUM_TOPICS = 5u;
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");

    const auto topic = buildTopicName();
    auto topicList = List<IString>();
    for (int i = 0; i < NUM_TOPICS; ++i)
    {
        addToList(topicList, fmt::format("{}_{}", topic, i));
    }
    auto config = device.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = device.addFunctionBlock(RAW_FB_NAME, config));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), NUM_TOPICS);
}

TEST_F(MqttStreamingClientModuleTest, CheckRawFbSignalListWithWildcard)
{
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");

    auto topicList = List<IString>();
    addToList(topicList, "");
    addToList(topicList, "goodTopic/test/topic");
    addToList(topicList, "badTopic/+/test/topic");
    addToList(topicList, "badTopic/+/+/topic");
    addToList(topicList, "badTopic/#");
    addToList(topicList, "goodTopic/test/newTopic");

    auto config = device.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = device.addFunctionBlock(RAW_FB_NAME, config));
    auto signals = rawFb.getSignals();
    ASSERT_EQ(signals.getCount(), 2u);
}

TEST_F(MqttStreamingClientModuleTest, CheckRawFbConfig)
{
    constexpr uint NUM_TOPICS = 5u;
    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");

    const auto topic = buildTopicName();
    auto topicList = List<IString>();
    for (int i = 0; i < NUM_TOPICS; ++i)
    {
        addToList(topicList, fmt::format("{}_{}", topic, i));
    }
    auto config = device.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
    daq::FunctionBlockPtr rawFb;
    ASSERT_NO_THROW(rawFb = device.addFunctionBlock(RAW_FB_NAME, config));

    const auto allProperties = rawFb.getAllProperties();
    ASSERT_EQ(allProperties.getCount(), config.getAllProperties().getCount());

    for (const auto& pror : config.getAllProperties())
    {
        const auto propName = pror.getName();
        ASSERT_TRUE(rawFb.hasProperty(propName));
        ASSERT_EQ(rawFb.getPropertyValue(propName), config.getPropertyValue(propName));
    }
}

TEST_F(MqttStreamingClientModuleTest, CheckRawFbDataTransfer)
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

TEST_F(MqttStreamingClientModuleTest, CheckRawFbFullDataTransfer)
{
    const std::string topic = buildTopicName();

    const auto instance = Instance();
    auto device = instance.addDevice("daq.mqtt://127.0.0.1");

    auto topicList = List<IString>();
    addToList(topicList, topic);
    auto config = device.getAvailableFunctionBlockTypes().get(RAW_FB_NAME).createDefaultConfig();

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
    auto singal = device.addFunctionBlock(RAW_FB_NAME, config).getSignals()[0];
    auto reader = daq::PacketReader(singal);

    MqttAsyncClientWrapper publisher(std::make_shared<mqtt::MqttAsyncClient>(), "testPublisherId");
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
