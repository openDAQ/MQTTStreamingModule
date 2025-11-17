#include <mqtt_streaming_client_module/single_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

SingleHandler::SingleHandler(bool useSignalNames)
    : useSignalNames(useSignalNames)
{
}

MqttData SingleHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    for (auto& sigCtx: signalContexts)
    {
        auto msgs = processSignalContext(sigCtx);
        messages.reserve(messages.size() + msgs.size());
        messages.insert(messages.end(),
                           std::make_move_iterator(msgs.begin()),
                           std::make_move_iterator(msgs.end()));
    }
    return messages;
}

ProcedureStatus SingleHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
{

    static const std::set<SampleType> allowedSampleTypes{SampleType::Float64,
                                                         SampleType::Float32,
                                                         SampleType::Float32,
                                                         SampleType::Float64,
                                                         SampleType::UInt8,
                                                         SampleType::Int8,
                                                         SampleType::UInt16,
                                                         SampleType::Int16,
                                                         SampleType::UInt32,
                                                         SampleType::Int32,
                                                         SampleType::UInt64,
                                                         SampleType::Int64,
                                                         SampleType::RangeInt64,
                                                         SampleType::String};
    ProcedureStatus status{true, {}};
    for (const auto& sigCtx : signalContexts)
    {
        auto signal = sigCtx.inputPort.getSignal();
        if (!signal.assigned())
            continue;
        if (!signal.getDescriptor().assigned())
        {
            status.messages.emplace_back(fmt::format("Connected signal \"{}\" doesn't contain a descroptor. This is not allowed.",
                                                     sigCtx.inputPort.getSignal().getGlobalId()));
            status.success = false;
        }
        if (auto demensions = signal.getDescriptor().getDimensions(); demensions.assigned() && demensions.getCount() > 0)
        {
            status.messages.emplace_back(fmt::format("Connected signal \"{}\" has more then 1 demention. This is not allowed.",
                                                     sigCtx.inputPort.getSignal().getGlobalId()));
            status.success = false;
        }
        if (auto sampleType = signal.getDescriptor().getSampleType(); allowedSampleTypes.find(sampleType) == allowedSampleTypes.cend())
        {
            status.messages.emplace_back(fmt::format("Connected signal \"{}\" has an incompatible sample type ({}).",
                                                     sigCtx.inputPort.getSignal().getGlobalId(),
                                                     convertSampleTypeToString(sampleType)));
            status.success = false;
        }
    }
    return status;
}

MqttData SingleHandler::processSignalContext(SignalContext& signalContext)
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
            }
        }
        else if (packet.getType() == PacketType::Data)
        {
            messages.emplace_back(processDataPacket(signalContext, packet.asPtr<IDataPacket>()));
        }

        packet = conn.dequeue();
    }
    return messages;
}

void SingleHandler::processSignalDescriptorChanged(SignalContext& signalCtx,
                                                   const DataDescriptorPtr& valueSigDesc,
                                                   const DataDescriptorPtr& domainSigDesc)
{
}

std::string SingleHandler::toString(const DataPacketPtr& dataPacket)
{
    auto sampleType = dataPacket.getDataDescriptor().getSampleType();
    std::string data;

    switch (sampleType)
    {
        case SampleType::Float64:
            data = std::to_string(*(static_cast<double*>(dataPacket.getData())));
            break;
        case SampleType::UInt64:
            data = std::to_string(*(static_cast<uint64_t*>(dataPacket.getData())));
            break;
        case SampleType::Int64:
            data = std::to_string(*(static_cast<int64_t*>(dataPacket.getData())));
            break;
        case SampleType::Binary:
            data = '\"' + std::string(static_cast<char*>(dataPacket.getData()), dataPacket.getDataSize()) + '\"';
            break;
        default:
            break;
    }

    return data;
}

std::string SingleHandler::toString(const std::string valueFieldName, daq::DataPacketPtr packet)
{
    std::string result;
    std::string data = toString(packet);
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

std::string SingleHandler::buildTopicName(const SignalContext& signalContext)
{
    return signalContext.inputPort.getSignal().getGlobalId().toStdString();
}

MqttDataSample SingleHandler::processDataPacket(SignalContext& signalContext, const DataPacketPtr& dataPacket)
{
    const auto signal = signalContext.inputPort.getSignal();
    std::string valueFieldName = useSignalNames ? signal.getName().toStdString() : signal.getGlobalId().toStdString();
    auto msg = toString(valueFieldName, dataPacket);
    std::string topic = buildTopicName(signalContext);
    return MqttDataSample{topic, msg};
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
