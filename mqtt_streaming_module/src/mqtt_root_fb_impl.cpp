#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/mqtt_json_receiver_fb_impl.h"
#include "mqtt_streaming_module/mqtt_publisher_fb_impl.h"
#include "mqtt_streaming_module/mqtt_raw_receiver_fb_impl.h"
#include <mqtt_streaming_module/mqtt_root_fb_impl.h>
#include <opendaq/function_block_type_factory.h>
#include <boost/algorithm/string.hpp>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

constexpr int MQTT_CLIENT_SYNC_DISCONNECT_TOUT = 3000;

std::atomic<int> MqttRootFbImpl::localIndex = 0;
std::vector<std::pair<MqttRootFbImpl::ConnectionStatus, std::string>> MqttRootFbImpl::connectionStatusMap =
    {{ConnectionStatus::Connected, "Connected"},
     {ConnectionStatus::Reconnecting, "Reconnecting"},
     {ConnectionStatus::Disconnected, "Disconnected"}};

MqttRootFbImpl::MqttRootFbImpl(const ContextPtr& ctx, const ComponentPtr& parent, const PropertyObjectPtr& config)
    : FunctionBlock(CreateType(), ctx, parent, generateLocalId()),
      subscriber(std::make_shared<mqtt::MqttAsyncClient>()),
      connectTimeout(0),
      connectionStatus(MQTT_ROOT_FB_CON_STATUS_TYPE,
                       MQTT_ROOT_FB_CON_STATUS_NAME,
                       statusContainer,
                       connectionStatusMap,
                       ConnectionStatus::Disconnected,
                       context.getTypeManager())
{
    initComponentStatus();
    initConnectionStatus();
    initProperties(config);
    initNestedFbTypes();
    initMqttSubscriber();

    if (!waitForConnection(connectTimeout))
    {
        LOG_E("MQTT: could not connect to MQTT broker within {} ms", connectTimeout);
        DAQ_THROW_EXCEPTION(CreateFailedException, "could not connect to MQTT broker within {} ms", connectTimeout);
    }

    LOG_I("MQTT: Connection established");
}

void MqttRootFbImpl::removed()
{
    FunctionBlock::removed();
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

void MqttRootFbImpl::initNestedFbTypes()
{
    nestedFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for manual JSON configuration
    {
        const auto fbType = MqttJsonReceiverFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
    // Add a function block type for raw MQTT messages
    {
        const auto fbType = MqttRawReceiverFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }

    // Add a function block type for MQTT publisher
    {
        const auto fbType = MqttPublisherFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
}

void MqttRootFbImpl::initMqttSubscriber()
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
                connectionStatus.setStatus(ConnectionStatus::Connected);
                connectedPromise.set_value(true);
                std::scoped_lock lock(componentStatusSync);
                setComponentStatus(ComponentStatus::Ok);
            }
        });

    LOG_I("MQTT: Trying to connect to the MQTT broker ({})", serverUrl);
    subscriber->connect();
}

void MqttRootFbImpl::initConnectionStatus()
{
    subscriber->setConnectionLostCb(
        [this](std::string msg)
        {
            connectionStatus.setStatus(ConnectionStatus::Reconnecting, msg);
            std::scoped_lock lock(componentStatusSync);
            setComponentStatusWithMessage(ComponentStatus::Error, "Connection lost");
        });
}

void MqttRootFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
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
    readProperties();
}

void MqttRootFbImpl::readProperties()
{
    connectionSettings.mqttUrl = objPtr.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_ADDRESS).asPtr<IString>().toStdString();
    connectionSettings.port = objPtr.getPropertyValue(PROPERTY_NAME_MQTT_BROKER_PORT);
    connectionSettings.username = objPtr.getPropertyValue(PROPERTY_NAME_MQTT_USERNAME).asPtr<IString>().toStdString();
    connectionSettings.password = objPtr.getPropertyValue(PROPERTY_NAME_MQTT_PASSWORD).asPtr<IString>().toStdString();
    connectionSettings.clientId = globalId.toStdString();

    connectTimeout = objPtr.getPropertyValue(PROPERTY_NAME_CONNECT_TIMEOUT);
}

bool MqttRootFbImpl::waitForConnection(const int timeoutMs)
{
    bool res =
        (connectedFuture.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready && connectedFuture.get() == true);
    subscriber->setConnectedCb(
        [this]
        {
            connectionStatus.setStatus(ConnectionStatus::Connected);
            std::scoped_lock lock(componentStatusSync);
            setComponentStatus(ComponentStatus::Ok);
        });
    return res;
}

DictPtr<IString, IFunctionBlockType> MqttRootFbImpl::onGetAvailableFunctionBlockTypes()
{
    return nestedFbTypes;
}

FunctionBlockPtr MqttRootFbImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        if (nestedFbTypes.hasKey(typeId))
        {
            auto fbTypePtr = nestedFbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == RAW_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttRawReceiverFbImpl>(context, functionBlocks, fbTypePtr, subscriber, config);
            }
            else if (fbTypePtr.getName() == JSON_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, MqttJsonReceiverFbImpl>(context, functionBlocks, fbTypePtr, subscriber, config);
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

std::string MqttRootFbImpl::generateLocalId()
{
    return std::string(MQTT_LOCAL_ROOT_FB_ID_PREFIX + std::to_string(localIndex++));
}

FunctionBlockTypePtr MqttRootFbImpl::CreateType()
{
    auto config = PropertyObject();
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_MQTT_BROKER_ADDRESS, DEFAULT_BROKER_ADDRESS)
                .setDescription(fmt::format("MQTT broker address. It can be an IP address or a hostname. By default it is set to \"{}\".",
                                            DEFAULT_BROKER_ADDRESS));
        config.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_MQTT_USERNAME, DEFAULT_USERNAME)
                .setDescription(fmt::format("Username for MQTT broker authentication. By default it is set to \"{}\".", DEFAULT_USERNAME));
        config.addProperty(builder.build());
    }
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_MQTT_PASSWORD, DEFAULT_PASSWORD)
                .setDescription(fmt::format("Password for MQTT broker authentication. By default it is set to \"{}\".", DEFAULT_PASSWORD));
        config.addProperty(builder.build());
    }
    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_MQTT_BROKER_PORT, DEFAULT_PORT)
                .setMinValue(1)
                .setMaxValue(65535)
                .setDescription(fmt::format("Port number for MQTT broker connection. By default it is set to {}.", DEFAULT_PORT));
        config.addProperty(builder.build());
    }
    {
        auto builder = IntPropertyBuilder(PROPERTY_NAME_CONNECT_TIMEOUT, DEFAULT_INIT_TIMEOUT)
        .setMinValue(0)
            .setUnit(Unit("ms"))
            .setDescription(fmt::format("Timeout in milliseconds for the initial connection to the MQTT broker. If the "
                                        "connection fails, an exception is thrown. By default it is set to {} ms.",
                                        DEFAULT_INIT_TIMEOUT));
        config.addProperty(builder.build());
    }
    const auto fbType = FunctionBlockType(ROOT_FB_NAME,
                                          ROOT_FB_NAME,
                                          "The MQTT function block allows connecting to MQTT broker. It may contain nested "
                                          "publisher/subscriber FBs.",
                                          config);
    return fbType;
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
