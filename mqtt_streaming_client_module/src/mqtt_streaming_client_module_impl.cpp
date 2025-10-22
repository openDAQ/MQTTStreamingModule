#include <chrono>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/version_info_factory.h>
#include <mqtt_streaming_client_module/constants.h>
#include <mqtt_streaming_client_module/mqtt_streaming_client_module_impl.h>
#include <mqtt_streaming_client_module/mqtt_streaming_device_impl.h>
#include <mqtt_streaming_client_module/version.h>
#include <opendaq/address_info_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opendaq/mirrored_signal_config_ptr.h>
#include <opendaq/search_filter_factory.h>
#include <regex>

#include <mqtt_streaming_client_module/mqtt_receiver_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

static const std::regex RegexIpv6Hostname(R"(^(.+://)?(\[[a-fA-F0-9:]+(?:\%[a-zA-Z0-9_\.-~]+)?\])(?::(\d+))?(/.*)?$)");
static const std::regex RegexIpv4Hostname(R"(^(.+://)?([^:/\s]+)(?::(\d+))?(/.*)?$)");

MqttStreamingClientModule::MqttStreamingClientModule(ContextPtr context)
    : Module(MODULE_NAME,
             daq::VersionInfo(MQTT_STREAM_CLI_MODULE_MAJOR_VERSION,
                              MQTT_STREAM_CLI_MODULE_MINOR_VERSION,
                              MQTT_STREAM_CLI_MODULE_PATCH_VERSION),
             std::move(context),
             MODULE_ID)
{
    loggerComponent = this->context.getLogger().getOrAddComponent(SHORT_MODULE_NAME);
}

ListPtr<IDeviceInfo> MqttStreamingClientModule::onGetAvailableDevices()
{
    auto availableDevices = List<IDeviceInfo>();
    return availableDevices;
}

DictPtr<IString, IDeviceType> MqttStreamingClientModule::onGetAvailableDeviceTypes()
{
    auto result = Dict<IString, IDeviceType>();

    auto deviceType = createDeviceType();
    result.set(deviceType.getId(), deviceType);
    return result;
}

DevicePtr
MqttStreamingClientModule::onCreateDevice(const StringPtr& connectionString, const ComponentPtr& parent, const PropertyObjectPtr& config)
{
    if (device.assigned())
        DAQ_THROW_EXCEPTION(AlreadyExistsException, "Only one MQTT streaming device can be created per module instance.");
    if (!connectionString.assigned())
        DAQ_THROW_EXCEPTION(ArgumentNullException);

    PropertyObjectPtr configPtr = config;
    if (!configPtr.assigned())
        configPtr = createDefaultConfig();
    else
        configPtr = populateDefaultConfig(configPtr);

    if (!acceptsConnectionParameters(connectionString, configPtr))
        DAQ_THROW_EXCEPTION(InvalidParameterException);

    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context is not available.");

    std::string hostType;
    extractConnectionParams(connectionString, configPtr, hostType);

    std::string host = configPtr.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS);
    Int port = configPtr.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT);

    std::scoped_lock lock(sync);

    device = createWithImplementation<IDevice, MqttStreamingDeviceImpl>(context, parent, configPtr);

    // Set the connection info for the device
    ServerCapabilityConfigPtr connectionInfo = device.getInfo().getConfigurationConnectionInfo();

    const auto addressInfo = AddressInfoBuilder()
                                 .setAddress(host)
                                 .setReachabilityStatus(AddressReachabilityStatus::Reachable)
                                 .setType(hostType)
                                 .setConnectionString(connectionString)
                                 .build();

    connectionInfo.setProtocolId(DaqMqttDeviceTypeId)
        .setProtocolName(PROTOCOL_NAME)
        .setProtocolType(ProtocolType::Streaming)
        .setConnectionType(CONNECTION_TYPE)
        .addAddress(host)
        .setPort(port)
        .setPrefix(DaqMqttDevicePrefix)
        .setConnectionString(connectionString)
        .addAddressInfo(addressInfo)
        .freeze();

    return device;
}

PropertyObjectPtr MqttStreamingClientModule::populateDefaultConfig(const PropertyObjectPtr& config)
{
    const auto defConfig = createDefaultConfig();
    for (const auto& prop : defConfig.getAllProperties())
    {
        const auto name = prop.getName();
        if (config.hasProperty(name))
            defConfig.setPropertyValue(name, config.getPropertyValue(name));
    }

    return defConfig;
}

void MqttStreamingClientModule::extractConnectionParams(const StringPtr& connectionString,
                                                        const PropertyObjectPtr& config,
                                                        std::string& hostType)
{
    std::string urlString = connectionString.toStdString();
    std::smatch match;

    std::string prefix = "";
    std::string path = "/";
    std::string host;
    int port = 0;

    bool parsed = false;
    parsed = std::regex_search(urlString, match, RegexIpv6Hostname);
    hostType = "IPv6";

    if (!parsed)
    {
        parsed = std::regex_search(urlString, match, RegexIpv4Hostname);
        hostType = "IPv4";
    }

    if (parsed)
    {
        prefix = match[1];
        host = match[2];

        if (match[3].matched)
            port = std::stoi(match[3]);

        if (match[4].matched)
            path = match[4];
    }
    else
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Host name not found in url: {}", connectionString);

    if (prefix != std::string(DaqMqttDevicePrefix) + "://")
        DAQ_THROW_EXCEPTION(InvalidParameterException, "MQTT does not support connection string with prefix {}", prefix);

    if (!config.assigned())
    {
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Config is missing");
    }
    if (port != 0 && config.hasProperty(PROPERTY_NAME_MQTT_BROKER_PORT))
        config.setPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT, Int(port));

    if (config.hasProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS))
        config.setPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS, host);
}

bool MqttStreamingClientModule::acceptsConnectionParameters(const StringPtr& connectionString, const PropertyObjectPtr& config)
{
    std::string connStr = connectionString;
    auto found = connStr.find(std::string(DaqMqttDevicePrefix) + "://");
    return found == 0;
}

Bool MqttStreamingClientModule::onCompleteServerCapability(const ServerCapabilityPtr& source, const ServerCapabilityConfigPtr& target)
{
    if (target.getProtocolId() != DaqMqttProtocolId)
        return false;

    if (source.getConnectionType() != CONNECTION_TYPE) // ???
        return false;

    if (!source.getAddresses().assigned() || !source.getAddresses().getCount())
    {
        LOG_W("Source server capability address is not available when filling in missing MQTT capability information.")
        return false;
    }

    const auto addrInfos = source.getAddressInfo();
    if (!addrInfos.assigned() || !addrInfos.getCount())
    {
        LOG_W("Source server capability addressInfo is not available when filling in missing MQTT capability "
              "information.")
        return false;
    }

    auto port = target.getPort();
    if (port == -1)
    {
        port = DEFAULT_PORT;
        target.setPort(port);
        LOG_W("MQTT server capability is missing port. Defaulting to {}.", port)
    }

    const auto path = target.hasProperty("Path") ? target.getPropertyValue("Path") : "";
    const auto targetAddress = target.getAddresses();
    for (const auto& addrInfo : addrInfos)
    {
        const auto address = addrInfo.getAddress();
        if (auto it = std::find(targetAddress.begin(), targetAddress.end(), address); it != targetAddress.end())
            continue;

        const auto prefix = target.getPrefix();
        StringPtr connectionString;
        if (source.getPrefix() == prefix)
            connectionString = addrInfo.getConnectionString();
        else
            connectionString = fmt::format("{}://{}:{}{}", prefix, address, port, path);

        const auto targetAddrInfo = AddressInfoBuilder()
                                        .setAddress(address)
                                        .setReachabilityStatus(addrInfo.getReachabilityStatus())
                                        .setType(addrInfo.getType())
                                        .setConnectionString(connectionString)
                                        .build();

        target.addAddressInfo(targetAddrInfo).setConnectionString(connectionString).addAddress(address);
    }

    return true;
}

DeviceTypePtr MqttStreamingClientModule::createDeviceType()
{
    return DeviceTypeBuilder()
        .setId(DaqMqttDeviceTypeId)
        .setName("MQTT enabled device")
        .setDescription("Network device connected over MQTT protocol")
        .setConnectionStringPrefix(DaqMqttDevicePrefix)
        .setDefaultConfig(createDefaultConfig())
        .build();
}

PropertyObjectPtr MqttStreamingClientModule::createDefaultConfig()
{
    auto config = PropertyObject();

    config.addProperty(StringProperty(PROPERTY_NAME_MQTT_BROKER_ADDRESS, DEFAULT_BROKER_ADDRESS));
    config.addProperty(StringProperty(PROPERTY_NAME_MQTT_USERNAME, DEFAULT_USERNAME));
    config.addProperty(StringProperty(PROPERTY_NAME_MQTT_PASSWORD, DEFAULT_PASSWORD));
    config.addProperty(IntProperty(PROPERTY_NAME_MQTT_BROKER_PORT, DEFAULT_PORT));
    config.addProperty(IntProperty(PROPERTY_NAME_INIT_DELAY, DEFAULT_INIT_DELAY));

    return config;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
