#include "mqtt_streaming_client_module/constants.h"
#include "mqtt_streaming_client_module/mqtt_json_receiver_fb_impl.h"
#include "mqtt_streaming_client_module/mqtt_publisher_fb_impl.h"
#include "mqtt_streaming_client_module/mqtt_raw_receiver_fb_impl.h"
#include <mqtt_streaming_client_module/mqtt_streaming_device_impl.h>

#include <coreobjects/property_object_protected_ptr.h>
#include <coretypes/function_factory.h>
#include <opendaq/component_deserialize_context_factory.h>
#include <opendaq/component_status_container_private_ptr.h>
#include <opendaq/deserialize_component_ptr.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/function_block_type_factory.h>

#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

std::atomic<int> MqttStreamingDeviceImpl::localIndex = 0;

constexpr int MQTT_CLIENT_SYNC_DISCONNECT_TOUT = 3000;

MqttStreamingDeviceImpl::MqttStreamingDeviceImpl(const ContextPtr& ctx, const ComponentPtr& parent, const PropertyObjectPtr& config)
    : Device(ctx, parent, getLocalId()),
      connectionStatus(Enumeration("ConnectionStatusType", "Connected", this->context.getTypeManager())),
      subscriber(std::make_shared<mqtt::MqttAsyncClient>())
{
    this->name = MQTT_DEVICE_NAME;

    connectionSettings.mqttUrl = config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS).asPtr<IString>().toStdString();
    connectionSettings.port = config.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT);
    connectionSettings.username = config.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME).asPtr<IString>().toStdString();
    connectionSettings.password = config.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD).asPtr<IString>().toStdString();
    connectionSettings.clientId = globalId.toStdString();

    connectionString =
        std::string(DaqMqttDevicePrefix) + "://" + connectionSettings.mqttUrl + ":" + std::to_string(connectionSettings.port);

    int connectTimeout = config.getPropertyValue(PROPERTY_NAME_CONNECT_TIMEOUT);
    int discoveryTimeout = config.getPropertyValue(PROPERTY_NAME_DISCOVERY_TIMEOUT);

    initComponentStatus();
    initBaseFunctionalBlocks();
    initMqttSubscriber();

    if (!waitForConnection(connectTimeout))
    {
        LOG_E("MQTT: could not connect to MQTT broker within {} ms", connectTimeout);
        DAQ_THROW_EXCEPTION(CreateFailedException, "could not connect to MQTT broker within {} ms", connectTimeout);
    }

    LOG_I("MQTT: Connection established");
    receiveSignalTopics(discoveryTimeout);
    // Build function block types based on received signal lists only once
    buildFunctionBlockTypes();
}

void MqttStreamingDeviceImpl::removed()
{
    Device::removed();
    LOG_I("MQTT: disconnecting from the MQTT broker...", connectionSettings.mqttUrl + ":" + std::to_string(connectionSettings.port));
    bool disRes = subscriber->syncDisconnect(MQTT_CLIENT_SYNC_DISCONNECT_TOUT);
    if (!disRes)
    {
        LOG_E("MQTT: disconnection was unsuccessful");
    }
    else
    {
        LOG_I("MQTT: disconnection was successful");
    }
}

DeviceInfoPtr MqttStreamingDeviceImpl::onGetInfo()
{
    return DeviceInfo(connectionString, MQTT_DEVICE_NAME);
}


void MqttStreamingDeviceImpl::initBaseFunctionalBlocks()
{
    baseFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for manual JSON configuration
    {
        const auto fbType = MqttJsonReceiverFbImpl::CreateType();
        baseFbTypes.set(fbType.getId(), fbType);
    }
    // Add a function block type for raw MQTT messages
    {
        const auto fbType = MqttRawReceiverFbImpl::CreateType();
        baseFbTypes.set(fbType.getId(), fbType);
    }

    // Add a function block type for MQTT publisher
    {
        const auto fbType = MqttPublisherFbImpl::CreateType();
        baseFbTypes.set(fbType.getId(), fbType);
    }
}

void MqttStreamingDeviceImpl::initMqttSubscriber()
{
    const auto serverUrl = connectionSettings.mqttUrl + ((connectionSettings.port > 0) ? ":" + std::to_string(connectionSettings.port) : "");
    subscriber->setServerURL(serverUrl);
    subscriber->setClientId(connectionSettings.clientId);
    subscriber->setUsernamePasswrod(connectionSettings.username, connectionSettings.password);

    connectedDone = false;
    connectedPromise = std::promise<bool>();
    connectedFuture = connectedPromise.get_future();

    subscriber->setConnectedCb(
        [this]
        {
            bool expected = false;
            if (connectedDone.compare_exchange_strong(expected, true))
            {
                connectedPromise.set_value(true);
            }
        });

    LOG_I("MQTT: Trying to connect to the MQTT broker ({})", serverUrl);
    subscriber->connect();
}

bool MqttStreamingDeviceImpl::waitForConnection(const int timeoutMs)
{
    bool res =
        (connectedFuture.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready && connectedFuture.get() == true);
    subscriber->setConnectedCb(nullptr);
    return res;
}

void MqttStreamingDeviceImpl::receiveSignalTopics(const int timeoutMs)
{
    if (timeoutMs > 0)
    {
        subscriber->setMessageArrivedCb(
            std::bind(&MqttStreamingDeviceImpl::onSignalsMessage, this, std::placeholders::_1, std::placeholders::_2));
        subscriber->subscribe(TOPIC_ALL_SIGNALS, 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs)); // TODO : remove it!

        subscriber->unsubscribe(TOPIC_ALL_SIGNALS);
        subscriber->setMessageArrivedCb(nullptr);
    }
    else
    {
        LOG_W("Signal discovering step was skipped");
    }
}

void MqttStreamingDeviceImpl::onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg)
{
    const std::string signalList(msg.getData().begin(), msg.getData().end());
    const std::string topic = msg.getTopic();
    std::string deviceName = mqtt::MqttDataWrapper::extractDeviceName(topic);
    if (!deviceName.empty()) {
        deviceMap.insert({std::move(deviceName), std::move(signalList)});
    }
}
void MqttStreamingDeviceImpl::buildFunctionBlockTypes()
{
    fbTypes = Dict<IString, IFunctionBlockType>();
    // Add base function block types
    fbTypes = baseFbTypes;
    // Add function block types from deviceMap (devices that sent signal lists)
    for (const auto& [deviceName, config] : deviceMap)
    {
        auto defaultConfig = PropertyObject();
        defaultConfig.addProperty(StringProperty(PROPERTY_NAME_SIGNAL_LIST, config));

        const auto fbType = FunctionBlockType(deviceName,
                                              deviceName,
                                              "",
                                              defaultConfig);

        fbTypes.set(fbType.getId(), fbType);
    }
    if (fbTypes.getCount() != 0)
    {
        LOG_I("Function block types available:");
    }
    else
    {
        LOG_I("No function block types available");
    }

    for (const auto& [fbName, _] : fbTypes)
    {
        LOG_I("\t{}", fbName.toStdString());
    }
}

DictPtr<IString, IFunctionBlockType> MqttStreamingDeviceImpl::onGetAvailableFunctionBlockTypes()
{
    return fbTypes;
}

FunctionBlockPtr MqttStreamingDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        if (fbTypes.hasKey(typeId))
        {
            auto fbTypePtr = fbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == RAW_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttRawReceiverFbImpl>(context, functionBlocks, fbTypePtr, typeId, subscriber, config);
            }
            else if (fbTypePtr.getName() == JSON_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttJsonReceiverFbImpl>(context, functionBlocks, fbTypePtr, typeId, subscriber, config);
            }
            else if (fbTypePtr.getName() == PUB_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttPublisherFbImpl>(context, functionBlocks, fbTypePtr, subscriber, config);
            }
            else
            {
                setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
                return nestedFunctionBlock;
            }
        }
        if (nestedFunctionBlock.assigned())
        {
            addNestedFunctionBlock(nestedFunctionBlock);
            setComponentStatus(ComponentStatus::Ok);
        }
        else
        {
            setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

std::string MqttStreamingDeviceImpl::getLocalId()
{
    return std::string(MQTT_LOCAL_DEVICE_ID_PREFIX + std::to_string(localIndex++));
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
