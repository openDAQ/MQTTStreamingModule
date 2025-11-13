#include <mqtt_streaming_client_module/multisingle_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MultisingleHandler::MultisingleHandler(bool useSignalNames, size_t packSize)
    : SingleHandler(useSignalNames),
      packSize(packSize > 0 ? packSize : 1)
{
}

MqttData MultisingleHandler::processSignalContext(SignalContext& signalContext)
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
            signalContext.data.push_back(packet.asPtr<IDataPacket>());
            if (signalContext.data.size() >= packSize)
            {
                messages.emplace_back(processDataPackets(signalContext, signalContext.data));
                signalContext.data.clear();
            }
        }

        packet = conn.dequeue();
    }
    return messages;
}

std::string MultisingleHandler::toString(const std::string valueFieldName, const std::vector<DataPacketPtr>& dataPackets)
{
    std::ostringstream dataOss;
    std::ostringstream tsOss;
    bool hasDomain = true;
    dataOss << "[";
    tsOss << "[";
    for (size_t i = 0; i < dataPackets.size(); ++i)
    {
        if (i > 0)
        {
            dataOss << ", ";
            tsOss << ", ";
        }

        dataOss << SingleHandler::toString(dataPackets[i]);

        if (auto domainPacket = dataPackets[i].getDomainPacket(); domainPacket.assigned())
        {
            uint64_t ts = *(static_cast<uint64_t*>(domainPacket.getData()));
            tsOss << std::to_string(ts);
        }
        else
        {
            hasDomain = false;
        }
    }
    dataOss << "]";
    tsOss << "]";
    std::string result;
    if (hasDomain)
        result = fmt::format("{{\"{}\" : {}, \"timestamp\": {}}}", valueFieldName, dataOss.str(), tsOss.str());
    else
        result = fmt::format("{{\"{}\" : {}}}", valueFieldName, dataOss.str());

    return result;
}

MqttDataSample MultisingleHandler::processDataPackets(SignalContext& signalContext, const std::vector<DataPacketPtr>& dataPacket)
{
    if (dataPacket.empty())
        return MqttDataSample{"", ""};
    const auto signal = signalContext.inputPort.getSignal();
    std::string valueFieldName = useSignalNames ? signal.getName().toStdString() : signal.getGlobalId().toStdString();
    auto msg = toString(valueFieldName, dataPacket);
    std::string topic = buildTopicName(signalContext);
    return MqttDataSample{topic, msg};
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
