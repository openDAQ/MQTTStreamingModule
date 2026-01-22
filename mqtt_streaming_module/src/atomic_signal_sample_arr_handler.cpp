#include <mqtt_streaming_module/atomic_signal_sample_arr_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

AtomicSignalSampleArrayHandler::AtomicSignalSampleArrayHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, size_t packSize)
    : AtomicSignalAtomicSampleHandler(parentFb, signalNamesMode),
      packSize(packSize > 0 ? packSize : 1)
{
}

std::string AtomicSignalSampleArrayHandler::getSchema()
{
    if (packSize == 1)
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>], \"timestamp\": [<timestamp_ns_0>]}}", buildValueFieldNameForSchema(signalNamesMode));
    }
    else if (packSize == 2)
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>, <sample_value_1>], \"timestamp\": [<timestamp_ns_0>, <timestamp_ns_1>]}}", buildValueFieldNameForSchema(signalNamesMode));
    }
    else
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>, ..., <sample_value_{}>], \"timestamp\": [<timestamp_ns_0>, ..., <timestamp_ns_{}>]}}", buildValueFieldNameForSchema(signalNamesMode), packSize - 1, packSize - 1);
    }
}

MqttData AtomicSignalSampleArrayHandler::processSignalContext(SignalContext& signalContext)
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

std::string AtomicSignalSampleArrayHandler::toString(const std::string valueFieldName, const std::vector<DataPacketPtr>& dataPackets)
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

        dataOss << HandlerBase::toString(dataPackets[i]);

        if (auto domainPacket = dataPackets[i].getDomainPacket(); domainPacket.assigned())
        {
            uint64_t ts = convertToEpoch(domainPacket);
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

MqttDataSample AtomicSignalSampleArrayHandler::processDataPackets(SignalContext& signalContext, const std::vector<DataPacketPtr>& dataPacket)
{
    if (dataPacket.empty())
        return MqttDataSample{nullptr, "", ""};
    const auto signal = signalContext.inputPort.getSignal();
    std::string valueFieldName = buildValueFieldName(signalNamesMode, signal);
    auto msg = toString(valueFieldName, dataPacket);
    std::string topic = buildTopicName(signalContext);
    return MqttDataSample{signalContext.previewSignal, topic, msg};
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
