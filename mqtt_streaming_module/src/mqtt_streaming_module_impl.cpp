#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/version_info_factory.h>
#include <mqtt_streaming_module/constants.h>
#include <mqtt_streaming_module/mqtt_streaming_module_impl.h>
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_root_fb_impl.h>
#include <mqtt_streaming_module/version.h>
#include <opendaq/address_info_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opendaq/mirrored_signal_config_ptr.h>
#include <opendaq/search_filter_factory.h>

#include <mqtt_streaming_module/mqtt_json_receiver_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

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

DictPtr<IString, IFunctionBlockType> MqttStreamingModule::onGetAvailableFunctionBlockTypes()
{
    auto result = Dict<IString, IFunctionBlockType>();

    auto fbType = createFbType();
    result.set(fbType.getId(), fbType);
    return result;
}

FunctionBlockPtr
MqttStreamingModule::onCreateFunctionBlock(const StringPtr& id,
                                           const ComponentPtr& parent,
                                           const StringPtr& localId,
                                           const PropertyObjectPtr& config)
{
    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context is not available.");

    FunctionBlockPtr fb = createWithImplementation<IFunctionBlock, MqttRootFbImpl>(context, parent, config);

    LOG_I("MQTT function block (GlobalId: {}) created", fb.getGlobalId());

    return fb;
}

FunctionBlockTypePtr MqttStreamingModule::createFbType()
{
    return MqttRootFbImpl::CreateType();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
