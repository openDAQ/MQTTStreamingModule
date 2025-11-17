#include <mqtt_streaming_client_module/multiple_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/reader_utils.h>
#include <opendaq/sample_type_traits.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MultipleHandler::MultipleHandler(bool useSignalNames, std::string topic)
    : useSignalNames(useSignalNames),
      topic(topic)
{
}

MqttData MultipleHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
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
                    std::string valueFieldName = (useSignalNames ? signal.getName() : signal.getGlobalId()).toStdString();
                    array.emplace_back(toString(valueFieldName, packet));
                }
            }
        }
        if (array.empty())
            continue;
        std::string topic = buildTopicName();
        std::string msg = messageFromArray(array);
        messages.emplace_back(std::move(topic), std::move(msg));
    }
    return messages;
}

ProcedureStatus MultipleHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
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
                                                         SampleType::Int64};
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

ProcedureStatus MultipleHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    return ProcedureStatus{true, {}};
}

std::string MultipleHandler::toString(const std::string valueFieldName, daq::DataPacketPtr packet)
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

std::string MultipleHandler::toString(const DataPacketPtr& dataPacket)
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

std::string MultipleHandler::buildTopicName()
{
    return topic;
}

std::string MultipleHandler::messageFromArray(const std::vector<std::string>& array)
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

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
