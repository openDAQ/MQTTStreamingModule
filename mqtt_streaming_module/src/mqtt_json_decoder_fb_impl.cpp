#include "mqtt_streaming_module/constants.h"
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/mqtt_json_decoder_fb_impl.h>
#include <chrono>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

std::atomic<int> MqttJsonDecoderFbImpl::localIndex = 0;

std::vector<std::pair<MqttJsonDecoderFbImpl::ParsingStatus, std::string>> MqttJsonDecoderFbImpl::parsingStatusMap =
    {{ParsingStatus::InvalidParamaters, "InvalidParamaters"},
     {ParsingStatus::ParsingFailed, "ParsingFailed"},
     {ParsingStatus::WaitingForData, "WaitingForData"},
     {ParsingStatus::ParsingSuccedeed, "ParsingSuccedeed"}};

MqttJsonDecoderFbImpl::MqttJsonDecoderFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, generateLocalId()),
      jsonDataWorker(loggerComponent),
      parsingStatus(MQTT_FB_PARSING_STATUS_TYPE,
                    MQTT_FB_PARSING_STATUS_NAME,
                    statusContainer,
                    parsingStatusMap,
                    ParsingStatus::WaitingForData,
                    context.getTypeManager())
{
    initComponentStatus();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    createSignal();
}

FunctionBlockTypePtr MqttJsonDecoderFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_VALUE_NAME, String("")).setDescription("");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_TS_NAME, String("")).setDescription("");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_UNIT, String("")).setDescription("");
        defaultConfig.addProperty(builder.build());
    }

    const auto fbType = FunctionBlockType(JSON_DECODER_FB_NAME,
                                          JSON_DECODER_FB_NAME,
                                          "",
                                          defaultConfig);
    return fbType;
}

std::string MqttJsonDecoderFbImpl::generateLocalId()
{
    return std::string(MQTT_LOCAL_JSON_DECODER_FB_ID_PREFIX + std::to_string(localIndex++));
}

void MqttJsonDecoderFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
                objPtr.setPropertyValue(propName, prop.getValue());
                objPtr.getOnPropertyValueWrite(prop.getName()) +=
                    [this](PropertyObjectPtr& obj, PropertyValueEventArgsPtr& args) { propertyChanged(); };
            }
        }
        else
        {
            objPtr.setPropertyValue(propName, prop.getValue());
        }
    }
    readProperties();
}

void MqttJsonDecoderFbImpl::readProperties()
{
    auto lock = this->getRecursiveConfigLock();
    configStatus.configValid = true;
    configStatus.configMsg.clear();
    config.valueFieldName = readProperty<std::string, IString>(PROPERTY_NAME_VALUE_NAME, "");
    if (config.valueFieldName.empty())
    {
        configStatus.configMsg = fmt::format("\"{}\" property is empty!", PROPERTY_NAME_VALUE_NAME);
        configStatus.configValid = false;
    }
    config.tsFieldName = readProperty<std::string, IString>(PROPERTY_NAME_TS_NAME, "");
    config.unitSymbol = readProperty<std::string, IString>(PROPERTY_NAME_UNIT, "");

    jsonDataWorker.setValueFieldName(config.valueFieldName);
    jsonDataWorker.setTimestampFieldName(config.tsFieldName);
    configStatus.waitingData = true;
    updateStatuses();
}

template <typename retT, typename intfT>
retT MqttJsonDecoderFbImpl::readProperty(const std::string& propertyName, const retT defaultValue)
{
    retT returnValue{};
    bool isPresent = false;
    if (objPtr.hasProperty(propertyName))
    {
        auto property = objPtr.getPropertyValue(propertyName).asPtrOrNull<intfT>();
        if (property.assigned())
        {
            isPresent = true;
            returnValue = property.getValue(defaultValue);
        }
    }
    if (!isPresent)
    {
        LOG_W("{} property is missing! Default value is set (\"{}\")", propertyName, defaultValue);
    }
    return returnValue;
}

void MqttJsonDecoderFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto prevConfig = config;
    readProperties();
    reconfigureSignal(prevConfig);
}

void MqttJsonDecoderFbImpl::updateStatuses()
{
    if (configStatus.configValid == false)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Configuration is invalid!");
        parsingStatus.setStatus(ParsingStatus::InvalidParamaters, configStatus.configMsg);
    }
    else if (configStatus.waitingData)
    {
        setComponentStatus(ComponentStatus::Ok);
        parsingStatus.setStatus(ParsingStatus::WaitingForData);
    }
    else if (configStatus.parsingSucceeded)
    {
        setComponentStatus(ComponentStatus::Ok);
        parsingStatus.setStatus(ParsingStatus::ParsingSuccedeed);
    }
    else
    {
        if (statusContainer.getStatus("ComponentStatus") == ComponentStatus::Ok)
        {
            setComponentStatus(ComponentStatus::Warning);
        }
        parsingStatus.setStatus(ParsingStatus::ParsingFailed, configStatus.parsingMsg);
    }
}

void MqttJsonDecoderFbImpl::processMessage(const std::string& json)
{
    auto lock = this->getRecursiveConfigLock();
    configStatus.waitingData = false;
    auto status = jsonDataWorker.createAndSendDataPacket(json);
    configStatus.parsingSucceeded = status.success;
    if (status.success)
    {
        configStatus.parsingMsg.clear();
    }
    else
    {
        configStatus.parsingMsg = status.msg;
    }
    updateStatuses();
}

void MqttJsonDecoderFbImpl::createSignal()
{
    auto lock = this->getRecursiveConfigLock();
    LOG_I("Creating a signal...");

    auto dataDescBdr =
        daq::DataDescriptorBuilder().setSampleType(daq::SampleType::Undefined);
    if (config.unitSymbol != "")
        dataDescBdr.setUnit(Unit(config.unitSymbol));

    outputSignal = createAndAddSignal(DEFAULT_VALUE_SIGNAL_LOCAL_ID, dataDescBdr.build());
    outputSignal.setName(config.valueFieldName);
    if (config.tsFieldName != "")
    {
        outputSignal.setDomainSignal(createDomainSignal());
    }

    jsonDataWorker.setOutputSignal(outputSignal);
}

void MqttJsonDecoderFbImpl::reconfigureSignal(const FbConfig& prevConfig)
{
    auto lock = this->getRecursiveConfigLock();
    if (prevConfig.valueFieldName != config.valueFieldName)
    {
        outputSignal.setName(config.valueFieldName);
    }

    if (prevConfig.valueFieldName != config.valueFieldName || prevConfig.unitSymbol != config.unitSymbol)
    {
        auto descBilder = DataDescriptorBuilderCopy(outputSignal.getDescriptor()).setSampleType(daq::SampleType::Undefined);
        if (config.unitSymbol != "")
            descBilder.setUnit(Unit(config.unitSymbol));
        outputSignal.setDescriptor(descBilder.build());
    }

    if (prevConfig.tsFieldName != config.tsFieldName)
    {
        if (prevConfig.tsFieldName.empty() && !config.tsFieldName.empty())
        {
            outputSignal.setDomainSignal(createDomainSignal());
        }
        else if (!prevConfig.tsFieldName.empty() && config.tsFieldName.empty())
        {
            outputSignal.setDomainSignal(nullptr);
            removeSignal(outputDomainSignal);
            outputDomainSignal = nullptr;
        }
        else if (!prevConfig.tsFieldName.empty() && !config.tsFieldName.empty())
        {
            // do nothing
        }
    }
}

SignalConfigPtr MqttJsonDecoderFbImpl::createDomainSignal()
{
    auto getEpoch = []() -> std::string
    {
        const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});
        char buf[48];
        strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));
        return {buf};
    };

    const auto domainSignalDsc = DataDescriptorBuilder()
                                     .setSampleType(SampleType::UInt64)
                                     .setUnit(Unit("s", -1, "seconds", "time"))
                                     .setTickResolution(Ratio(1, 1'000'000))
                                     .setOrigin(getEpoch())
                                     .setName("Time")
                                     .build();
    outputDomainSignal = createAndAddSignal(DEFAULT_TS_SIGNAL_LOCAL_ID, domainSignalDsc, false);
    return outputDomainSignal;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
