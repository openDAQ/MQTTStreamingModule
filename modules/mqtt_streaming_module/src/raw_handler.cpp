#include <mqtt_streaming_module/raw_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

RawHandler::RawHandler(WeakRefPtr<IFunctionBlock> parentFb)
    : HandlerBase(parentFb)
{
}

MqttData RawHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    for (auto& sigCtx : signalContexts)
    {
        auto msgs = processSignalContext(sigCtx);
        messages.merge(std::move(msgs));
    }
    return messages;
}

ProcedureStatus RawHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
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
    }
    status.merge(HandlerBase::validateSignalContexts(signalContexts));
    return status;
}

MqttData RawHandler::processSignalContext(SignalContext& signalContext)
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

void RawHandler::processSignalDescriptorChanged(SignalContext&,
                                                   const DataDescriptorPtr&,
                                                   const DataDescriptorPtr&)
{
}

std::string RawHandler::buildTopicName(const SignalContext& signalContext)
{
    return signalContext.inputPort.getSignal().getGlobalId().toStdString();
}

MqttDataSamplePtr RawHandler::processDataPacket(SignalContext& signalContext, const DataPacketPtr& dataPacket, size_t offset)
{
    (void)signalContext;
    auto msg = toDataBuffer(dataPacket, offset);
    std::string topic = buildTopicName(signalContext);
    return std::make_shared<MqttDataSample<std::vector<uint8_t>>>(signalContext.previewSignal, std::move(topic), std::move(msg));
}

template<typename T>
std::vector<uint8_t> copyData(daq::DataPacketPtr packet, size_t offset)
{
    std::vector<uint8_t> data;
    auto size = sizeof(T);
    data.resize(size);
    memcpy(data.data(), (static_cast<T*>(packet.getData()) + offset), size);
    return data;
}

std::vector<uint8_t> RawHandler::toDataBuffer(daq::DataPacketPtr packet, size_t offset)
{
    std::vector<uint8_t> data;

    switch (packet.getDataDescriptor().getSampleType())
    {
        case SampleType::Float64:
            data = copyData<SampleTypeToType<SampleType::Float64>::Type>(packet, offset);
            break;
        case SampleType::Float32:
            data = copyData<SampleTypeToType<SampleType::Float32>::Type>(packet, offset);
            break;
        case SampleType::UInt64:
            data = copyData<SampleTypeToType<SampleType::UInt64>::Type>(packet, offset);
            break;
        case SampleType::Int64:
            data = copyData<SampleTypeToType<SampleType::Int64>::Type>(packet, offset);
            break;
        case SampleType::UInt32:
            data = copyData<SampleTypeToType<SampleType::UInt32>::Type>(packet, offset);
            break;
        case SampleType::Int32:
            data = copyData<SampleTypeToType<SampleType::Int32>::Type>(packet, offset);
            break;
        case SampleType::UInt16:
            data = copyData<SampleTypeToType<SampleType::UInt16>::Type>(packet, offset);
            break;
        case SampleType::Int16:
            data = copyData<SampleTypeToType<SampleType::Int16>::Type>(packet, offset);
            break;
        case SampleType::UInt8:
            data = copyData<SampleTypeToType<SampleType::UInt8>::Type>(packet, offset);
            break;
        case SampleType::Int8:
            data = copyData<SampleTypeToType<SampleType::Int8>::Type>(packet, offset);
            break;
        case SampleType::String:
        case SampleType::Binary:
        {
            data.resize(packet.getDataSize());
            memcpy(data.data(), packet.getData(), packet.getDataSize());
        }
        break;
        default:
            break;
    }
    return data;
}

ListPtr<IString> RawHandler::getTopics(const std::vector<SignalContext>& signalContexts)
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

std::string RawHandler::getSchema()
{
    return std::string("Raw data");
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
