#include <mqtt_streaming_module/atomic_signal_sample_arr_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/sample_type_traits.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

AtomicSignalSampleArrayHandler::AtomicSignalSampleArrayHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, size_t packSize)
    : AtomicSignalAtomicSampleHandler(parentFb, signalNamesMode),
      packSize(packSize > 0 ? packSize : 1)
{
}

ProcedureStatus AtomicSignalSampleArrayHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    std::set<std::string> set;
    for (const auto& buf : signalBuffers)
        set.insert(buf.first);

    for (auto& sigCtx : signalContexts)
    {
        const auto signal = sigCtx.inputPort.getSignal();
        if (!signal.assigned())
            continue;
        auto& buffer = signalBuffers[signal.getGlobalId().toStdString()];
        buffer.clear();
        set.erase(signal.getGlobalId().toStdString());
    }
    for (const auto& el : set)
        signalBuffers.erase(el);

    return ProcedureStatus{true, {}};
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
                break;
            }
        }
        else if (packet.getType() == PacketType::Data)
        {
            auto dataPacket = packet.asPtr<IDataPacket>();
            const auto sigGlobalId = signalContext.inputPort.getSignal().getGlobalId().toStdString();
            signalBuffers[sigGlobalId].data.push_back(dataPacket);
            signalBuffers[sigGlobalId].dataSize += dataPacket.getSampleCount();
            while (signalBuffers[sigGlobalId].dataSize >= packSize)
                messages.emplace_back(processDataPackets(signalContext));

        }

        packet = conn.dequeue();
    }
    return messages;
}

std::pair<DataPacketPtr, size_t> AtomicSignalSampleArrayHandler::getSample(SignalContext& signalContext)
{
    const auto sigGlobalId = signalContext.inputPort.getSignal().getGlobalId().toStdString();
    if (signalBuffers[sigGlobalId].data.empty())
        return {nullptr, 0};
    auto dataPacket = signalBuffers[sigGlobalId].data.front();
    size_t offset = signalBuffers[sigGlobalId].offset++;
    signalBuffers[sigGlobalId].dataSize--;
    if (signalBuffers[sigGlobalId].offset == dataPacket.getSampleCount())
    {
        signalBuffers[sigGlobalId].data.pop_front();
        signalBuffers[sigGlobalId].offset = 0;
    }
    return {dataPacket, offset};
}

void AtomicSignalSampleArrayHandler::processSignalDescriptorChanged(SignalContext& signalCtx,
                                                                    const DataDescriptorPtr& valueSigDesc,
                                                                    const DataDescriptorPtr& domainSigDesc)
{
    signalBuffers[signalCtx.inputPort.getSignal().getGlobalId().toStdString()].clear();
}

std::string AtomicSignalSampleArrayHandler::toString(const std::string valueFieldName, SignalContext& signalContext)
{
    std::ostringstream dataOss;
    std::ostringstream tsOss;
    bool hasDomain = true;
    dataOss << "[";
    tsOss << "[";
    size_t commonCnt = 0;
    while (commonCnt < packSize)
    {
        auto [dataPacket, offset] = getSample(signalContext);
        if (commonCnt > 0)
        {
            dataOss << ", ";
            tsOss << ", ";
        }

        dataOss << HandlerBase::toString(dataPacket, offset);

        if (auto domainPacket = dataPacket.getDomainPacket(); domainPacket.assigned())
        {
            uint64_t ts = convertToEpoch(domainPacket, offset);
            tsOss << std::to_string(ts);
        }
        else
        {
            hasDomain = false;
        }
        commonCnt++;
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

MqttDataSample AtomicSignalSampleArrayHandler::processDataPackets(SignalContext& signalContext)
{
    if (signalBuffers[signalContext.inputPort.getSignal().getGlobalId().toStdString()].data.empty())
        return MqttDataSample{nullptr, "", ""};
    const auto signal = signalContext.inputPort.getSignal();
    std::string valueFieldName = buildValueFieldName(signalNamesMode, signal);
    auto msg = toString(valueFieldName, signalContext);
    std::string topic = buildTopicName(signalContext);
    return MqttDataSample{signalContext.previewSignal, topic, msg};
}
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
