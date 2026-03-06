#include <mqtt_streaming_module/atomic_signal_atomic_sample_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

AtomicSignalAtomicSampleHandler::AtomicSignalAtomicSampleHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode)
    : HandlerBase(parentFb, signalNamesMode)
{
}

MqttData AtomicSignalAtomicSampleHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    for (auto& sigCtx : signalContexts)
    {
        auto msgs = processSignalContext(sigCtx);
        messages.merge(std::move(msgs));
    }
    return messages;
}

ProcedureStatus AtomicSignalAtomicSampleHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
{
    ProcedureStatus status{true, {}};
    for (const auto& sigCtx : signalContexts)
    {
        auto signal = sigCtx.inputPort.getSignal();
        if (!signal.assigned())
            continue;
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
        if (auto dSignal = signal.getDomainSignal(); dSignal.assigned())
        {
            auto descriptor = dSignal.getDescriptor();
            if (!descriptor.assigned())
            {
                status.addError(fmt::format("Connected signal \"{}\" has a domain signal without descriptor. This is not allowed.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
            else if (descriptor.getSampleType() != SampleType::UInt64 && descriptor.getSampleType() != SampleType::Int64)
            {
                status.addError(fmt::format("Connected signal \"{}\" has an incompatible sample type for its domain signal. "
                                            "Only SampleType::UInt64 and SampleType::Int64 are allowed.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
            else if (auto unit = descriptor.getUnit(); !unit.assigned() || unit.getSymbol() != "s")
            {
                status.addError(fmt::format("Connected signal \"{}\" has an incompatible unit for its domain signal. "
                                            "Only 's' (seconds) is allowed.",
                                            sigCtx.inputPort.getSignal().getGlobalId()));
            }
        }
    }
    status.merge(HandlerBase::validateSignalContexts(signalContexts));
    return status;
}

MqttData AtomicSignalAtomicSampleHandler::processSignalContext(SignalContext& signalContext)
{
    MqttData messages;
    const auto conn = signalContext.inputPort.getConnection();
    if (!conn.assigned())
        return messages;

    PacketPtr packet = conn.dequeue();
    while (packet.assigned())
    {
        if (packet.getType() == PacketType::Event)
        {
            auto eventPacket = packet.asPtr<IEventPacket>(true);
            LOG_T("Processing {} event", eventPacket.getEventId())
            if (eventPacket.getEventId() == event_packet_id::DATA_DESCRIPTOR_CHANGED)
            {
                DataDescriptorPtr valueSignalDescriptor = eventPacket.getParameters().get(event_packet_param::DATA_DESCRIPTOR);
                DataDescriptorPtr domainSignalDescriptor = eventPacket.getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR);
                processSignalDescriptorChanged(signalContext, valueSignalDescriptor, domainSignalDescriptor);
                messages.needRevalidation = true;
                break;
            }
        }
        else if (packet.getType() == PacketType::Data)
        {
            auto dataPacket = packet.asPtr<IDataPacket>();
            for (size_t i = 0; i < dataPacket.getSampleCount(); ++i)
                messages.data.emplace_back(processDataPacket(signalContext, dataPacket, i));
        }

        packet = conn.dequeue();
    }
    return messages;
}

void AtomicSignalAtomicSampleHandler::processSignalDescriptorChanged(SignalContext&,
                                                   const DataDescriptorPtr&,
                                                   const DataDescriptorPtr&)
{
}

std::string AtomicSignalAtomicSampleHandler::toString(const std::string valueFieldName, daq::DataPacketPtr packet, size_t offset)
{
    std::string result;
    std::string data = HandlerBase::toString(packet, offset);
    if (auto domainPacket = packet.getDomainPacket(); domainPacket.assigned())
    {
        uint64_t ts = convertToEpoch(domainPacket, offset);
        result = fmt::format("{{\"{}\" : {}, \"timestamp\": {}}}", valueFieldName, data, ts);
    }
    else
    {
        result = fmt::format("{{\"{}\" : {}}}", valueFieldName, data);
    }

    return result;
}

std::string AtomicSignalAtomicSampleHandler::buildTopicName(const SignalContext& signalContext)
{
    return signalContext.inputPort.getSignal().getGlobalId().toStdString();
}

MqttDataSamplePtr AtomicSignalAtomicSampleHandler::processDataPacket(SignalContext& signalContext, const DataPacketPtr& dataPacket, size_t offset)
{
    const auto signal = signalContext.inputPort.getSignal();
    std::string valueFieldName = buildValueFieldName(signalNamesMode, signal);
    auto msg = toString(valueFieldName, dataPacket, offset);
    std::string topic = buildTopicName(signalContext);
    return std::make_shared<MqttDataSample<std::string>>(signalContext.previewSignal, std::move(topic), std::move(msg));
}

ListPtr<IString> AtomicSignalAtomicSampleHandler::getTopics(const std::vector<SignalContext>& signalContexts)
{
    auto res = List<IString>();
    for (const auto& sigCtx : signalContexts)
    {
        if (!sigCtx.inputPort.getConnection().assigned())
            continue;
        auto t = buildTopicName(sigCtx);
        res.pushBack(String(t));
    }
    return res;
}

std::string AtomicSignalAtomicSampleHandler::getSchema()
{
    return fmt::format("{{\"{}\" : <sample_value>, \"timestamp\": <timestamp_ns>}}", buildValueFieldNameForSchema(signalNamesMode));
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
