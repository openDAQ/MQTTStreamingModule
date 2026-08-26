#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/version_info_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <mqtt_streaming_module/mqtt_streaming_module_impl.h>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_client_device_impl.h>
#include <mqtt_streaming_module/version.h>
#include <opendaq/address_info_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/mirrored_signal_config_ptr.h>
#include <opendaq/search_filter_factory.h>

#include <regex>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

static const std::regex RegexIpv6Hostname(R"(^(.+://)?(\[[a-fA-F0-9:]+(?:\%[a-zA-Z0-9_\.-~]+)?\])(?::(\d+))?(/.*)?$)");
static const std::regex RegexIpv4Hostname(R"(^(.+://)?([^:/\s]+)(?::(\d+))?(/.*)?$)");

MqttStreamingModule::MqttStreamingModule(ContextPtr context)
    : Module(MODULE_NAME,
             daq::VersionInfo(MQTT_STREAM_MODULE_MAJOR_VERSION,
                              MQTT_STREAM_MODULE_MINOR_VERSION,
                              MQTT_STREAM_MODULE_PATCH_VERSION),
             std::move(context),
             MODULE_ID)
{
    loggerComponent = this->context.getLogger().getOrAddComponent(SHORT_MODULE_NAME);
}

DictPtr<IString, IDeviceType> MqttStreamingModule::onGetAvailableDeviceTypes()
{
    auto result = Dict<IString, IDeviceType>();

    auto deviceType = createDeviceType();
    result.set(deviceType.getId(), deviceType);
    return result;
}

DevicePtr MqttStreamingModule::onCreateDevice(const StringPtr& connectionString,
                                              const ComponentPtr& parent,
                                              const PropertyObjectPtr& config)
{
    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context is not available.");
    if (!connectionString.assigned())
        DAQ_THROW_EXCEPTION(ArgumentNullException, "Connection string is not assigned.");

    PropertyObjectPtr deviceConfig = populateDefaultConfig(config);
    auto conParam = parseConnectionString(connectionString);

    // A port in the connection string wins over the Port property; the property is the fallback.
    if (conParam.port == 0)
    {
        conParam.port = static_cast<uint16_t>(deviceConfig.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT).asPtr<IInteger>());
    }
    else
    {
        deviceConfig.setPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT, conParam.port);
    }

    const auto formedConnectionString = formatConnectionString(conParam);

    StringPtr localId;
    {
        std::scoped_lock lock(sync);
        localId = String(fmt::format("{}{}", MQTT_LOCAL_CLIENT_DEVICE_ID_PREFIX, deviceIndex++));
    }

    DevicePtr device = createWithImplementation<IDevice, MqttClientDeviceImpl>(
        context, parent, localId, formedConnectionString, conParam.host, deviceConfig);

    const auto deviceType = createDeviceType();
    ServerCapabilityConfigPtr connectionInfo = device.getInfo().getConfigurationConnectionInfo();
    connectionInfo.setProtocolId(deviceType.getId());
    connectionInfo.setProtocolName(deviceType.getId());
    connectionInfo.setProtocolType(ProtocolType::Unknown);
    connectionInfo.setConnectionType("TCP/IP");
    connectionInfo.addAddress(conParam.host);
    connectionInfo.setPort(conParam.port);
    connectionInfo.setPrefix(CLIENT_DEVICE_CONN_PREFIX);
    connectionInfo.setConnectionString(formedConnectionString);

    LOG_I("MQTT device (GlobalId: {}) created", device.getGlobalId());

    return device;
}

MqttStreamingModule::BrokerAddress MqttStreamingModule::parseConnectionString(const StringPtr& connectionString)
{
    const std::string url = connectionString.toStdString();
    MqttStreamingModule::BrokerAddress conParam;
    std::smatch match;
    bool parsed = std::regex_search(url, match, RegexIpv6Hostname);
    if (!parsed)
        parsed = std::regex_search(url, match, RegexIpv4Hostname);

    if (!parsed)
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Could not parse connection string: {}", connectionString);

    const std::string prefix = match[1].matched ? match[1].str() : "";
    const auto expectedPrefix = std::string(CLIENT_DEVICE_CONN_PREFIX) + "://";
    if (prefix != expectedPrefix)
        DAQ_THROW_EXCEPTION(InvalidParameterException,
                            "Connection string \"{}\" does not start with \"{}\"",
                            connectionString,
                            expectedPrefix);


    conParam.host = match[2].str();
    if (conParam.host.empty())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Connection string \"{}\" carries no broker host", connectionString);

    if (match[3].matched)
    {
        const auto portValue = std::stoi(match[3].str());
        if (portValue < 1 || portValue > 65535)
            DAQ_THROW_EXCEPTION(InvalidParameterException, "Port {} in connection string is out of range", portValue);
        conParam.port = static_cast<uint16_t>(portValue);
    }

    return conParam;
}

StringPtr MqttStreamingModule::formatConnectionString(const MqttStreamingModule::BrokerAddress& conParam)
{
    return String(fmt::format("{}://{}:{}", CLIENT_DEVICE_CONN_PREFIX, conParam.host, conParam.port));
}

DeviceTypePtr MqttStreamingModule::createDeviceType()
{
    return MqttClientDeviceImpl::CreateType();
}

PropertyObjectPtr MqttStreamingModule::populateDefaultConfig(const PropertyObjectPtr& config)
{
    const auto defConfig = MqttClientDeviceImpl::CreateDefaultConfig();
    if (!config.assigned())
        return defConfig;
    for (const auto& prop : defConfig.getAllProperties())
    {
        const auto name = prop.getName();
        if (config.hasProperty(name))
            defConfig.setPropertyValue(name, config.getPropertyValue(name));
    }

    return defConfig;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
