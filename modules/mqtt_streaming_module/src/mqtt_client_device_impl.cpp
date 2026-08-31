#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/mqtt_subscriber_fb_impl.h"
#include "mqtt_streaming_module/mqtt_publisher_fb_impl.h"
#include <mqtt_streaming_module/mqtt_client_device_impl.h>
#include <coretypes/enumeration_factory.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <boost/algorithm/string.hpp>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_CLIENT_SYNC_DISCONNECT_TOUT = 3000;

MqttClientDeviceImpl::MqttClientDeviceImpl(const ContextPtr& ctx,
                                           const ComponentPtr& parent,
                                           const StringPtr& localId,
                                           const StringPtr& connectionString,
                                           const std::string& brokerHost,
                                           const PropertyObjectPtr& config)
    : Device(ctx, parent, localId, nullptr, CLIENT_DEVICE_TYPE_NAME),
      connectionString(connectionString),
      connectTimeout(0),
      subscriber(std::make_shared<mqtt::MqttAsyncClient>())
{
    initComponentStatus();
    connectionStatusContainer.addConfigurationConnectionStatus(
        connectionString, Enumeration("ConnectionStatusType", "Reconnecting", context.getTypeManager()));
    initConnectionStatus();
    initProperties(config);

    connectionSettings.mqttUrl = brokerHost;

    initNestedFbTypes();
    initMqttSubscriber();

    if (!waitForConnection(connectTimeout))
    {
        LOG_E("MQTT: could not connect to MQTT broker within {} ms", connectTimeout);
        DAQ_THROW_EXCEPTION(CreateFailedException, "could not connect to MQTT broker within {} ms", connectTimeout);
    }

    LOG_I("MQTT: Connection established");
}

void MqttClientDeviceImpl::removed()
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

DeviceInfoPtr MqttClientDeviceImpl::onGetInfo()
{
    auto info = DeviceInfo(connectionString, CLIENT_DEVICE_TYPE_NAME);
    info.setDeviceType(CreateType());
    return info;
}

void MqttClientDeviceImpl::initNestedFbTypes()
{
    nestedFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a MQTT subscriber function block type
    {
        const auto fbType = MqttSubscriberFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }

    // Add a function block type for MQTT publisher
    {
        const auto fbType = MqttPublisherFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
}

void MqttClientDeviceImpl::initMqttSubscriber()
{
    const auto serverUrl = connectionSettings.mqttUrl + ((connectionSettings.port > 0) ? ":" + std::to_string(connectionSettings.port) : "");
    subscriber->setServerURL(serverUrl);
    subscriber->setClientId(connectionSettings.clientId);
    subscriber->setUsernamePasswrod(connectionSettings.username, connectionSettings.password);

    connectedDone = false;
    auto connectedPromise = std::make_shared<std::promise<bool>>();
    connectedFuture = connectedPromise->get_future();

    subscriber->setConnectedCb(
        [this, connectedPromise]
        {
            bool expected = false;
            if (connectedDone.compare_exchange_strong(expected, true))
            {
                setConnectionStatus("Connected");
                connectedPromise->set_value(true);
                std::scoped_lock lock(componentStatusSync);
                setComponentStatus(ComponentStatus::Ok);
            }
        });

    LOG_I("MQTT: Trying to connect to the MQTT broker ({})", serverUrl);
    subscriber->connect();
}

void MqttClientDeviceImpl::initConnectionStatus()
{
    subscriber->setConnectionLostCb(
        [this](std::string msg)
        {
            setConnectionStatus("Reconnecting", msg);
            std::scoped_lock lock(componentStatusSync);
            setComponentStatusWithMessage(ComponentStatus::Error, "Connection lost");
        });
}

void MqttClientDeviceImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (propName == PROPERTY_NAME_CLIENT_PASSWORD)
            continue;
        if (!objPtr.hasProperty(propName))
        {
            auto propClone = PropertyBuilder(prop.getName())
                            .setValueType(prop.getValueType())
                            .setDescription(prop.getDescription())
                            .setUnit(prop.getUnit())
                            .setMinValue(prop.getMinValue())
                            .setMaxValue(prop.getMaxValue())
                            .setDefaultValue(prop.getValue())
                            .setVisible(prop.getVisible())
                            .setReadOnly(true)
                            .setSelectionValues(prop.getSelectionValues())
                            .setSuggestedValues(prop.getSuggestedValues())
                            .setReferencedProperty(prop.getReferencedProperty())
                            .setCoercer(prop.getCoercer())
                            .setValidator(prop.getValidator())
                            .setCallableInfo(prop.getCallableInfo())
                            .setOnPropertyValueRead(prop.getOnPropertyValueRead())
                            .setOnPropertyValueWrite(prop.getOnPropertyValueWrite())
                            .setOnSelectionValuesRead(prop.getOnSelectionValuesRead())
                            .setOnSuggestedValuesRead(prop.getOnSuggestedValuesRead())
                            .build();
            objPtr.addProperty(propClone);
        }
        else if (objPtr.getProperty(propName).getReadOnly() == false)
        {
            objPtr.setPropertyValue(propName, prop.getValue());
        }
    }
    readProperties(config);
}

void MqttClientDeviceImpl::readProperties(const PropertyObjectPtr& config)
{
    connectionSettings.port = config.getPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT).asPtr<IInteger>();
    connectionSettings.username = config.getPropertyValue(PROPERTY_NAME_CLIENT_USERNAME).asPtr<IString>().toStdString();
    connectionSettings.password = config.getPropertyValue(PROPERTY_NAME_CLIENT_PASSWORD).asPtr<IString>().toStdString();
    connectionSettings.clientId = globalId.toStdString();

    connectTimeout = config.getPropertyValue(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT);
}

bool MqttClientDeviceImpl::waitForConnection(const int timeoutMs)
{
    bool res =
        (connectedFuture.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready && connectedFuture.get() == true);
    subscriber->setConnectedCb(
        [this]
        {
            setConnectionStatus("Connected");
            std::scoped_lock lock(componentStatusSync);
            setComponentStatus(ComponentStatus::Ok);
        });
    return res;
}

void MqttClientDeviceImpl::setConnectionStatus(const std::string& value, const std::string& message)
{
    connectionStatusContainer.updateConnectionStatusWithMessage(
        connectionString, Enumeration("ConnectionStatusType", value, context.getTypeManager()), nullptr, String(message));
}

DictPtr<IString, IFunctionBlockType> MqttClientDeviceImpl::onGetAvailableFunctionBlockTypes()
{
    return nestedFbTypes;
}

FunctionBlockPtr MqttClientDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        if (nestedFbTypes.hasKey(typeId))
        {
            auto fbTypePtr = nestedFbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == SUB_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttSubscriberFbImpl>(context, functionBlocks, fbTypePtr, subscriber, config);
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
            DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

void MqttClientDeviceImpl::onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock)
{
    auto lock = getRecursiveConfigLock2();

    if (!functionBlocks.hasItem(functionBlock.getLocalId()))
        DAQ_THROW_EXCEPTION(NotFoundException, "Function block not found: " + functionBlock.getLocalId().toStdString());

    functionBlocks.removeItem(functionBlock);
    setComponentStatus(ComponentStatus::Ok);
}

PropertyObjectPtr MqttClientDeviceImpl::CreateDefaultConfig()
{
    auto config = PropertyObject();
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_CLIENT_USERNAME, DEFAULT_USERNAME)
                .setDescription(fmt::format("Username for MQTT broker authentication. By default it is set to \"{}\".", DEFAULT_USERNAME));
        config.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_CLIENT_PASSWORD, DEFAULT_PASSWORD)
                .setDescription(fmt::format("Password for MQTT broker authentication. By default it is set to \"{}\".", DEFAULT_PASSWORD));
        config.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_CLIENT_BROKER_PORT, DEFAULT_PORT)
                .setMinValue(1)
                .setMaxValue(65535)
                .setDescription(fmt::format("Port number for MQTT broker connection. Used only when the connection string "
                                            "carries no port. By default it is set to {}.",
                                            DEFAULT_PORT));
        config.addProperty(builder.build());
    }
    {
        auto builder = IntPropertyBuilder(PROPERTY_NAME_CLIENT_CONNECT_TIMEOUT, DEFAULT_INIT_TIMEOUT)
        .setMinValue(0)
            .setUnit(Unit("ms"))
            .setDescription(fmt::format("Timeout in milliseconds for the initial connection to the MQTT broker. If the "
                                        "connection fails, an exception is thrown. By default it is set to {} ms.",
                                        DEFAULT_INIT_TIMEOUT));
        config.addProperty(builder.build());
    }
    return config;
}

DeviceTypePtr MqttClientDeviceImpl::CreateType()
{
    return DeviceType(CLIENT_DEVICE_TYPE_ID,
                      CLIENT_DEVICE_TYPE_NAME,
                      "The MQTT device connects to an MQTT broker. It may contain nested publisher/subscriber FBs.",
                      CLIENT_DEVICE_CONN_PREFIX,
                      CreateDefaultConfig());
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
