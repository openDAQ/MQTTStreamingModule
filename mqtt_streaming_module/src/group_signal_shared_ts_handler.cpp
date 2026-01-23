#include <mqtt_streaming_module/group_signal_shared_ts_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/reader_utils.h>
#include <opendaq/sample_type_traits.h>
#include <optional>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

GroupSignalSharedTsHandler::GroupSignalSharedTsHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, std::string topic)
    : HandlerBase(parentFb, signalNamesMode),
      buffersSize(1000),
      topic(topic)
{
}

GroupSignalSharedTsHandler::~GroupSignalSharedTsHandler()
{
    std::scoped_lock lock(sync);
    deallocateBuffers();
    if (reader.assigned())
    {
        reader.dispose();
    }
}

MqttData GroupSignalSharedTsHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    std::scoped_lock lock(sync);
    if (!reader.assigned())
        return messages;
    auto dataAvailable = reader.getAvailableCount();
    auto count = std::min(SizeT{buffersSize}, dataAvailable);
    auto status = reader.read(dataBuffers.data(), &count);
    if (status.getReadStatus() == ReadStatus::Ok && count > 0)
    {
        const auto tsStruct = domainToTs(status);
        for (SizeT sampleCnt = 0; sampleCnt < count; ++sampleCnt)
        {
            std::vector<std::string> fields;
            for (size_t signalCnt = 0; signalCnt < signalContexts.size() - 1; ++signalCnt)
            {
                const auto signal = signalContexts[signalCnt].inputPort.getSignal();
                std::string valueFieldName = buildValueFieldName(signalNamesMode, signal);
                fields.emplace_back(toString(signal.getDescriptor().getSampleType(), valueFieldName, dataBuffers[signalCnt], sampleCnt));
            }

            fields.emplace_back(tsToString(tsStruct, sampleCnt));
            std::string topic = buildTopicName();
            std::string msg = messageFromFields(fields);
            messages.emplace_back(MqttDataSample{signalContexts[0].previewSignal, std::move(topic), std::move(msg)});
        }
    }

    return messages;
}

ProcedureStatus GroupSignalSharedTsHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
{

    static const std::set<SampleType> allowedSampleTypes{SampleType::Float64,
                                                         SampleType::Float32,
                                                         SampleType::UInt8,
                                                         SampleType::Int8,
                                                         SampleType::UInt16,
                                                         SampleType::Int16,
                                                         SampleType::UInt32,
                                                         SampleType::Int32,
                                                         SampleType::UInt64,
                                                         SampleType::Int64};
    ProcedureStatus status{true, {}};
    for (const auto& sigCtx : signalContexts)
    {
        auto signal = sigCtx.inputPort.getSignal();
        if (!signal.assigned())
            continue;
        const auto dSignal = signal.getDomainSignal();
        if (!dSignal.assigned())
        {
            status.addError(fmt::format("Connected signal \"{}\" doesn't contain a domain signal. This is not allowed.",
                                        sigCtx.inputPort.getSignal().getGlobalId()));
        }
        else if (const auto dSignalDesc = dSignal.getDescriptor(); !dSignalDesc.assigned())
        {
            status.addError(fmt::format("Connected signal \"{}\" doesn't contain a descroptor for a domain signal. This is not allowed.",
                                        sigCtx.inputPort.getSignal().getGlobalId()));
        }
        else
        {
            if (auto domainDataRule = signal.getDomainSignal().getDescriptor().getRule(); domainDataRule.getType() != DataRuleType::Linear)
            {
                status.addError(fmt::format("Connected signal \"{}\" has an incompatible data rule for its domain signal.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
            if (signal.getDomainSignal().getDescriptor().getSampleType() != SampleType::UInt64 &&
                signal.getDomainSignal().getDescriptor().getSampleType() != SampleType::Int64)
            {
                status.addError(fmt::format("Connected signal \"{}\" has an incompatible sample type for its domain signal. "
                                            "Only SampleType::UInt64 and SampleType::Int64 are allowed.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
            if (auto unit = signal.getDomainSignal().getDescriptor().getUnit(); !unit.assigned() || unit.getSymbol() != "s")
            {
                status.addError(fmt::format("Connected signal \"{}\" has an incompatible unit for its domain signal. "
                                            "Only 's' (seconds) is allowed.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
        }

        if (!signal.getDescriptor().assigned())
        {
            status.addError(fmt::format("Connected signal \"{}\" doesn't contain a descroptor. This is not allowed.",
                                        sigCtx.inputPort.getSignal().getGlobalId()));
        }
        if (auto demensions = signal.getDescriptor().getDimensions(); demensions.assigned() && demensions.getCount() > 0)
        {
            status.addError(fmt::format("Connected signal \"{}\" has more then 1 demention. This is not allowed.",
                                        sigCtx.inputPort.getSignal().getGlobalId()));
        }
        if (auto sampleType = signal.getDescriptor().getSampleType(); allowedSampleTypes.find(sampleType) == allowedSampleTypes.cend())
        {
            status.addError(fmt::format("Connected signal \"{}\" has an incompatible sample type ({}).",
                                        sigCtx.inputPort.getSignal().getGlobalId(),
                                        convertSampleTypeToString(sampleType)));
        }
    }

    std::optional<std::pair<uint64_t, uint64_t>> ratio0;
    bool error = false;
    for (size_t i = 0; i < signalContexts.size() && !error; ++i)
    {
        auto signal = signalContexts[i].inputPort.getSignal();
        if (!signal.assigned())
            continue;

        if (!ratio0.has_value())
        {
            ratio0 = calculateRatio(signal.getDomainSignal().getDescriptor());
        }
        else
        {
            auto ratio = calculateRatio(signal.getDomainSignal().getDescriptor());
            error = (ratio != ratio0);
        }
    }

    if (error)
    {
        status.addError(fmt::format("Connected signals have incompatible sample rates. This is not allowed."));
    }

    std::unordered_set<std::string> namesSet;
    for (const auto& sigCtx : signalContexts)
    {
        auto signal = sigCtx.inputPort.getSignal();
        if (!signal.assigned())
            continue;
        std::string name = buildValueFieldName(signalNamesMode, signal);
        if (namesSet.find(name) != namesSet.end())
        {
            std::string key;
            if (signalNamesMode == SignalValueJSONKey::GlobalID)
                key = "GlobalID";
            else if (signalNamesMode == SignalValueJSONKey::LocalID)
                key = "LocalID";
            else
                key = "name";
            status.addError(
                fmt::format("Connected signals have non-unique {} (\"{}\"). JSON field names cannot be built", key, name));
        }
        namesSet.insert(std::move(name));
    }
    status.merge(HandlerBase::validateSignalContexts(signalContexts));
    return status;
}

TimestampTickStruct GroupSignalSharedTsHandler::domainToTs(const MultiReaderStatusPtr status)
{
    TimestampTickStruct res;
    const auto descriptor =
        status.getMainDescriptor().getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR).asPtr<IDataDescriptor>();
    const uint64_t offset = status.getOffset().getValue<uint64_t>(0);
    const uint64_t start = descriptor.getRule().getParameters().get("start").getValue<uint64_t>(0);
    const uint64_t refOffset = descriptor.getReferenceDomainInfo().getReferenceDomainOffset().getValue<uint64_t>(0);

    res.firstTick = offset + start + refOffset;
    res.ratioNum = descriptor.getTickResolution().simplify().getNumerator();
    res.ratioDen = descriptor.getTickResolution().getDenominator();
    res.multiplier = 1'000'000; // amount of us in a second
    res.delta = descriptor.getRule().getParameters().get("delta").getValue<uint64_t>(0);
    return res;
}

ProcedureStatus GroupSignalSharedTsHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    ProcedureStatus status{true, {}};
    createReader(signalContexts);
    return status;
}

ListPtr<IString> GroupSignalSharedTsHandler::getTopics(const std::vector<SignalContext>& signalContexts)
{
    auto res = List<IString>(String(buildTopicName()));
    return res;
}

std::string GroupSignalSharedTsHandler::getSchema()
{
    return fmt::format("{{\"{}\" : <sample_value>, ..., \"{}\" : <sample_value>, \"timestamp\": <timestamp_ns>}}", buildValueFieldNameForSchema(signalNamesMode, "_0"), buildValueFieldNameForSchema(signalNamesMode, "_N"));
}

std::string GroupSignalSharedTsHandler::toString(const SampleType sampleType, const std::string& valueFieldName, void* data, SizeT offset)
{
    switch (sampleType)
    {
        case SampleType::Float64:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Float64>::Type*>(data) + offset)));
        case SampleType::Float32:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Float32>::Type*>(data) + offset)));
        case SampleType::UInt64:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt64>::Type*>(data) + offset)));
        case SampleType::Int64:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Int64>::Type*>(data) + offset)));
        case SampleType::UInt32:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt32>::Type*>(data) + offset)));
        case SampleType::Int32:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Int32>::Type*>(data) + offset)));
        case SampleType::UInt16:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt16>::Type*>(data) + offset)));
        case SampleType::Int16:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Int16>::Type*>(data) + offset)));
        case SampleType::UInt8:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt8>::Type*>(data) + offset)));
        case SampleType::Int8:
            return fmt::format("\"{}\" : {}",
                               valueFieldName,
                               std::to_string(*(static_cast<SampleTypeToType<SampleType::Int8>::Type*>(data) + offset)));
        default:
            break;
    }
    return "unsupported";
}

template <typename T>
std::string GroupSignalSharedTsHandler::toString(const std::string& valueFieldName, void* data, SizeT offset)
{
    return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<T*>(data) + offset)));
}

std::string GroupSignalSharedTsHandler::tsToString(TimestampTickStruct tsStruct, SizeT offset)
{
    // const uint64_t epochTime = (firstTick + delta * offset) * ratioNum * US_IN_S / ratioDen;    // us
    const uint64_t g = std::gcd(tsStruct.multiplier, tsStruct.ratioDen);
    tsStruct.multiplier /= g;
    tsStruct.ratioDen /= g;
    return fmt::format("\"timestamp\" : {}",
                       std::to_string(((tsStruct.firstTick + tsStruct.delta * offset) * tsStruct.ratioNum * tsStruct.multiplier) /
                                      tsStruct.ratioDen));
}

std::string GroupSignalSharedTsHandler::buildTopicName()
{
    return topic;
}

void GroupSignalSharedTsHandler::createReader(const std::vector<SignalContext>& signalContexts)
{
    std::scoped_lock lock(sync);
    // signalContexts always contain an unconnected input port
    if (signalContexts.size() <= 1)
        return;

    if (reader.assigned())
    {
        reader.dispose();
    }

    auto multiReaderBuilder = MultiReaderBuilder().setValueReadType(SampleType::Undefined).setDomainReadType(SampleType::UInt64);
    for (const auto& sContext : signalContexts)
    {
        if (sContext.inputPort.getSignal().assigned())
            multiReaderBuilder.addInputPort(sContext.inputPort);
    }

    reader = multiReaderBuilder.build();
    const auto parentFb = this->parentFb.getRef();
    if (parentFb.assigned())
        reader.setExternalListener(parentFb.asPtr<IInputPortNotifications>());
    allocateBuffers(signalContexts);
}

void GroupSignalSharedTsHandler::allocateBuffers(const std::vector<SignalContext>& signalContexts)
{
    // Allocate buffers for each signal
    auto signalsCount = signalContexts.size() - 1;
    deallocateBuffers();

    dataBuffers = std::vector<void*>(signalsCount, nullptr);

    for (size_t i = 0; i < signalsCount; ++i)
    {
        dataBuffers[i] = std::malloc(buffersSize * getSampleSize(signalContexts[i].inputPort.getSignal().getDescriptor().getSampleType()));
    }
}

void GroupSignalSharedTsHandler::deallocateBuffers()
{
    for (size_t i = 0; i < dataBuffers.size(); ++i)
    {
        std::free(dataBuffers[i]);
        dataBuffers[i] = nullptr;
    }
    dataBuffers.clear();
}

std::string GroupSignalSharedTsHandler::messageFromFields(const std::vector<std::string>& fields)
{
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < fields.size(); ++i)
    {
        if (i > 0)
            oss << ", ";
        oss << std::move(fields[i]);
    }
    oss << "}";
    return oss.str();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
