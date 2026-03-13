#include "mqtt_streaming_module/constants.h"
#include <mqtt_streaming_module/helper.h>
#include <mqtt_streaming_module/property_helper.h>
#include <mqtt_streaming_module/mqtt_json_decoder_fb_impl.h>
#include <chrono>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

std::atomic<int> MqttJsonDecoderFbImpl::localIndex = 0;

MqttJsonDecoderFbImpl::MqttJsonDecoderFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const FunctionBlockTypePtr& type,
                                       const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, generateLocalId()),
      jsonDataWorker(),
      lastExternalTs(0)
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
    using DSM = mqtt::MqttDataWrapper::DomainSignalMode;
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_DEC_VALUE_NAME, String(""))
                .setDescription("Specifies the JSON field name from which value data will be extracted. This property is required. It "
                                "should be contained in the incoming JSON messages. Otherwise, a parsing error will occur.");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_DEC_TS_MODE,
                                     List<IString>("None", "Extract from message", "System time"),
                                     static_cast<int>(DSM::None))
                .setDescription(
                    "Defines how the timestamp of the decoded signal is generated. By default it is set to None, which means that "
                    "the decoded signal doesn't have a timestamp. If set to Extract from message, the JSON decoder will try "
                    "to extract the timestamp from the incoming JSON messages. If set to System time, the timestamp of the decoded signal "
                    "is set to the system time when the JSON message is received.");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_DEC_TS_NAME, String(""))
                .setVisible(EvalValue(std::string("$") + PROPERTY_NAME_DEC_TS_MODE +
                                      " == " + std::to_string(static_cast<int>(DSM::ExtractFromMessage))))
                .setDescription(
                    "Specifies the JSON field name from which timestamp will be extracted. This property is "
                    "optional. If it is set it should be contained in the incoming JSON messages. Otherwise, a parsing error will occur.");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder = StringPropertyBuilder(PROPERTY_NAME_DEC_UNIT, String(""))
                           .setDescription("Specifies the unit symbol for the decoded value. This property is optional.");
        defaultConfig.addProperty(builder.build());
    }

    const auto fbType =
        FunctionBlockType(JSON_DECODER_FB_NAME,
                          JSON_DECODER_FB_NAME,
                          "The JSON decoder Function Block extracts data from a JSON string and builds a signal based on that data.",
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
                    [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(); };
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
    using namespace property_helper;
    using DSM = mqtt::MqttDataWrapper::DomainSignalMode;
    auto lock = this->getRecursiveConfigLock();
    configValid = true;
    configMsg.clear();
    config.valueFieldName = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_DEC_VALUE_NAME, "");
    if (config.valueFieldName.empty())
    {
        configMsg = fmt::format("\"{}\" property is empty!", PROPERTY_NAME_DEC_VALUE_NAME);
        configValid = false;
    }

    config.tsFieldName = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_DEC_TS_NAME, "");
    config.unitSymbol = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_DEC_UNIT, "");
    auto tmpTsMode = readProperty<int, IInteger>(objPtr, PROPERTY_NAME_DEC_TS_MODE, static_cast<int>(DSM::None));

    if (tmpTsMode < static_cast<int>(DSM::_count) && tmpTsMode >= 0)
    {
        config.tsMode = static_cast<DSM>(tmpTsMode);
    }
    else
    {
        configMsg = fmt::format("Wrong value for the \"{}\" property!", PROPERTY_NAME_DEC_TS_MODE);
        configValid = false;
        config.tsMode = DSM::None;
    }
    if (config.tsMode == DSM::ExtractFromMessage)
    {
        if (config.tsFieldName.empty())
        {
            configMsg =
                fmt::format("Empty \"{}\" property is not allowed for the \'Extract from message\' mode", PROPERTY_NAME_DEC_TS_NAME);
            configValid = false;
        }
    }
    else
    {
        config.tsFieldName = "";
    }
    jsonDataWorker.setValueFieldName(config.valueFieldName);
    jsonDataWorker.setTimestampFieldName(config.tsFieldName);
    jsonDataWorker.setDomainSignalMode(config.tsMode);
    waitingData = configValid.load();
    externalTsDuplicate = false;
    updateStatuses();
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
    if (configValid == false)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Configuration is invalid! " + configMsg);
    }
    else if (waitingData)
    {
        setComponentStatusWithMessage(ComponentStatus::Ok, "Waiting for data");
    }
    else if (parsingSucceeded == false)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Parsing failed: " + parsingMsg);
    }
    else if (externalTsDuplicate)
    {
        setComponentStatusWithMessage(ComponentStatus::Warning,
                                      "Domain signal value for one of the received messages is the same as previous. "
                                      "Data may be lost!");
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Ok, "Parsing succeeded");
    }
}

void MqttJsonDecoderFbImpl::checkExternalTs(const uint64_t externalTs)
{
    if (config.tsMode == mqtt::MqttDataWrapper::DomainSignalMode::ExternalTimestamp)
    {
        if (externalTs == lastExternalTs)
            externalTsDuplicate = true;
        lastExternalTs = externalTs;
    }
}

void MqttJsonDecoderFbImpl::processMessage(const std::string& json, const uint64_t externalTs)
{
    if (configValid)
    {
        auto lock = this->getRecursiveConfigLock();
        waitingData = false;
        checkExternalTs(externalTs);
        auto status = jsonDataWorker.createAndSendDataPacket(json, externalTs);
        parsingSucceeded = status.success;
        if (status.success)
        {
            parsingMsg.clear();
        }
        else
        {
            parsingMsg = status.msg;
        }
        updateStatuses();
    }
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
    if (config.tsMode != mqtt::MqttDataWrapper::DomainSignalMode::None)
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

    if (prevConfig.tsMode != config.tsMode)
    {
        using DSM = mqtt::MqttDataWrapper::DomainSignalMode;
        if (prevConfig.tsMode == DSM::None && config.tsMode != DSM::None)
        {
            outputSignal.setDomainSignal(createDomainSignal());
        }
        else if (prevConfig.tsMode != DSM::None && config.tsMode == DSM::None)
        {
            outputSignal.setDomainSignal(nullptr);
            removeSignal(outputDomainSignal);
            outputDomainSignal = nullptr;
        }
        else
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
