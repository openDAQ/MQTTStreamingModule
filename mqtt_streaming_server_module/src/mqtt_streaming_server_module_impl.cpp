#include <mqtt_streaming_server_module/mqtt_streaming_server_impl.h>
#include <mqtt_streaming_server_module/mqtt_streaming_server_module_impl.h>
#include <mqtt_streaming_server_module/version.h>
#include <mqtt_streaming_server_module/constants.h>
#include <coreobjects/property_object_class_ptr.h>
#include <coretypes/version_info_factory.h>
#include <chrono>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE

MqttStreamingServerModule::MqttStreamingServerModule(ContextPtr context)
    : Module(MODULE_NAME,
             daq::VersionInfo(MQTT_STREAM_SRV_MODULE_MAJOR_VERSION,
                              MQTT_STREAM_SRV_MODULE_MINOR_VERSION,
                              MQTT_STREAM_SRV_MODULE_PATCH_VERSION),
             std::move(context),
             MODULE_ID)
{
}

DictPtr<IString, IServerType> MqttStreamingServerModule::onGetAvailableServerTypes()
{
    auto result = Dict<IString, IServerType>();

    auto serverType = MqttStreamingServerImpl::createType(context);
    result.set(serverType.getId(), serverType);

    return result;
}

ServerPtr MqttStreamingServerModule::onCreateServer(const StringPtr& serverType,
                                                      const PropertyObjectPtr& serverConfig,
                                                      const DevicePtr& rootDevice)
{
    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context parameter cannot be null.");

    PropertyObjectPtr config = serverConfig;
    if (!config.assigned())
        config = MqttStreamingServerImpl::createDefaultConfig(context);
    else
        config = MqttStreamingServerImpl::populateDefaultConfig(config, context);

    ServerPtr server(MqttStreamingServer_Create(rootDevice, config, context));
    return server;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE
