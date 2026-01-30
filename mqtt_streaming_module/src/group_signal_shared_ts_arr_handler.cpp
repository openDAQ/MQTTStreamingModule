#include <mqtt_streaming_module/group_signal_shared_ts_arr_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/reader_utils.h>
#include <opendaq/sample_type_traits.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

GroupSignalSharedTsArrHandler::GroupSignalSharedTsArrHandler(WeakRefPtr<IFunctionBlock> parentFb,
                                                             SignalValueJSONKey signalNamesMode,
                                                             std::string topic,
                                                             size_t packSize)
    : GroupSignalSharedTsHandler(parentFb, signalNamesMode, topic),
      packSize(packSize),
      tsBuilder(packSize)
{
}

MqttData GroupSignalSharedTsArrHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    std::scoped_lock lock(sync);
    if (!reader.assigned())
        return messages;

    const auto dataAvailable = reader.getAvailableCount();
    SizeT count = std::min(SizeT{buffersSize}, dataAvailable);
    auto status = reader.read(dataBuffers.data(), &count);
    if (count > 0)
    {
        const auto tsStruct = domainToTs(status);
        for (size_t signalCnt = 0; signalCnt < signalContexts.size() - 1; ++signalCnt)
        {
            const auto signal = signalContexts[signalCnt].inputPort.getSignal();
            dataBuilders[signalCnt].append(signal.getDescriptor().getSampleType(), dataBuffers[signalCnt], count);
        }
        tsBuilder.append(tsStruct, count);
    }

    messages.needRevalidation = processEvents(status);

    while (!tsBuilder.empty())
    {
        std::vector<std::string> fields;
        for (size_t signalCnt = 0; signalCnt < signalContexts.size() - 1; ++signalCnt)
        {
            std::string valueFieldName = buildValueFieldName(signalNamesMode, signalContexts[signalCnt].inputPort.getSignal());
            fields.emplace_back(dataBuilders[signalCnt].getPack(valueFieldName));
        }
        fields.emplace_back(tsBuilder.getPack());
        messages.data.emplace_back(MqttDataSample{signalContexts[0].previewSignal, buildTopicName(), messageFromFields(fields)});
    }
    return messages;
}

ProcedureStatus GroupSignalSharedTsArrHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    auto status = GroupSignalSharedTsHandler::signalListChanged(signalContexts);
    initBuilders(signalContexts.size() - 1);
    return status;
}

std::string GroupSignalSharedTsArrHandler::getSchema()
{
    if (packSize == 1)
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>], ..., \"{}\" : [<sample_value_0>], \"timestamp\": [<timestamp_ns_0>]}}",
                           buildValueFieldNameForSchema(signalNamesMode, "_0"),
                           buildValueFieldNameForSchema(signalNamesMode, "_N"));
    }
    else if (packSize == 2)
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>, <sample_value_1>], ..., \"{}\" : [<sample_value_0>, <sample_value_1>], "
                           "\"timestamp\": [<timestamp_ns_0>, <timestamp_ns_1>]}}",
                           buildValueFieldNameForSchema(signalNamesMode, "_0"),
                           buildValueFieldNameForSchema(signalNamesMode, "_N"));
    }
    else
    {
        return fmt::format("{{\"{}\" : [<sample_value_0>, ..., <sample_value_{}>], ..., \"timestamp\": [<timestamp_ns_0>, ..., "
                           "<timestamp_ns_{}>]}}",
                           buildValueFieldNameForSchema(signalNamesMode),
                           packSize - 1,
                           packSize - 1);
    }
}

void GroupSignalSharedTsArrHandler::initBuilders(const size_t size)
{
    if (dataBuilders.empty() || dataBuilders.size() != size)
    {
        tsBuilder.clear();
        dataBuilders.clear();
        dataBuilders.reserve(size);
        for (size_t i = 0; i < size; ++i)
            dataBuilders.emplace_back(packSize);
    }
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
