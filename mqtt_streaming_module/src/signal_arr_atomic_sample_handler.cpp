#include <mqtt_streaming_module/signal_arr_atomic_sample_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/reader_utils.h>
#include <opendaq/sample_type_traits.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

SignalArrayAtomicSampleHandler::SignalArrayAtomicSampleHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, std::string topic)
    : HandlerBase(parentFb, signalNamesMode),
      topic(topic)
{
}

MqttData SignalArrayAtomicSampleHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    bool dataAvailable = true;
    while (dataAvailable)
    {
        dataAvailable = false;
        std::vector<std::string> array;
        for (const auto& signalContext : signalContexts)
        {
            const auto conn = signalContext.inputPort.getConnection();
            if (!conn.assigned())
                continue;
            PacketPtr packet = conn.dequeue();
            if (packet.assigned())
            {
                dataAvailable = true;
                if (packet.getType() == PacketType::Event)
                {
                    auto eventPacket = packet.asPtr<IEventPacket>(true);
                    LOG_T("Processing {} event", eventPacket.getEventId());
                }
                else if (packet.getType() == PacketType::Data)
                {
                    const auto signal = signalContext.inputPort.getSignal();
                    std::string valueFieldName = buildValueFieldName(signalNamesMode, signal);
                    array.emplace_back(toString(valueFieldName, packet));
                }
            }
        }
        if (array.empty())
            continue;
        std::string topic = buildTopicName();
        std::string msg = messageFromArray(array);
        messages.emplace_back(MqttDataSample{signalContexts[0].previewSignal, std::move(topic), std::move(msg)});
    }
    return messages;
}

ProcedureStatus SignalArrayAtomicSampleHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
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

ProcedureStatus SignalArrayAtomicSampleHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    return ProcedureStatus{true, {}};
}

ListPtr<IString> SignalArrayAtomicSampleHandler::getTopics(const std::vector<SignalContext>& signalContexts)
{
    auto res = List<IString>(String(buildTopicName()));
    return res;
}

std::string SignalArrayAtomicSampleHandler::getSchema()
{
    return fmt::format("[{{\"{}\" : <sample_value>, \"timestamp\": <timestamp_ns>}}, ..., {{\"{}\" : <sample_value>, \"timestamp\": <timestamp_ns>}}]", buildValueFieldNameForSchema(signalNamesMode, "_0"), buildValueFieldNameForSchema(signalNamesMode, "_N"));
}

std::string SignalArrayAtomicSampleHandler::toString(const std::string valueFieldName, daq::DataPacketPtr packet)
{
    std::string result;
    std::string data = HandlerBase::toString(packet);
    if (auto domainPacket = packet.getDomainPacket(); domainPacket.assigned())
    {
        uint64_t ts = convertToEpoch(domainPacket);
        result = fmt::format("{{\"{}\" : {}, \"timestamp\": {}}}", valueFieldName, data, ts);
    }
    else
    {
        result = fmt::format("{{\"{}\" : {}}}", valueFieldName, data);
    }

    return result;
}

std::string SignalArrayAtomicSampleHandler::buildTopicName()
{
    return topic;
}

std::string SignalArrayAtomicSampleHandler::messageFromArray(const std::vector<std::string>& array)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < array.size(); ++i)
    {
        if (i > 0)
            oss << ", ";
        oss << std::move(array[i]);
    }
    oss << "]";
    return oss.str();
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
