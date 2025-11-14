#include <mqtt_streaming_client_module/multiple_shared_handler.h>
#include <opendaq/custom_log.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_factory.h>
#include <opendaq/reader_utils.h>
#include <opendaq/sample_type_traits.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

MultipleSharedHandler::MultipleSharedHandler(bool useSignalNames, std::string topic)
    : useSignalNames(useSignalNames),
      buffersSize(1000),
      topic(topic)
{
}

MqttData MultipleSharedHandler::processSignalContexts(std::vector<SignalContext>& signalContexts)
{
    MqttData messages;
    if (!reader.assigned())
        return messages;
    auto dataAvailable = reader.getAvailableCount();
    auto count = std::min(SizeT{buffersSize}, dataAvailable);
    auto status = reader.readWithDomain(dataBuffers.data(), domainBuffers.data(), &count);
    if (status.getReadStatus() == ReadStatus::Ok && count > 0)
    {
        for (SizeT sampleCnt = 0; sampleCnt < count; ++sampleCnt)
        {
            std::vector<std::string> fields;
            for (size_t signalCnt = 0; signalCnt < signalContexts.size() - 1; ++signalCnt)
            {
                const auto signal = signalContexts[signalCnt].inputPort.getSignal();
                std::string valueFieldName = (useSignalNames ? signal.getName() : signal.getGlobalId()).toStdString();
                fields.emplace_back(toString(signal.getDescriptor().getSampleType(), valueFieldName, dataBuffers[signalCnt], sampleCnt));
            }
            fields.emplace_back(tsToString(domainBuffers[0], sampleCnt));
            std::string topic = buildTopicName();
            std::string msg = messageFromFields(fields);
            messages.emplace_back(std::move(topic), std::move(msg));
        }
    }

    return messages;
}

ProcedureStatus MultipleSharedHandler::validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
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
        if (!signal.getDomainSignal().assigned())
        {
            status.messages.emplace_back(fmt::format("Connected signal \"{}\" doesn't contain a domain signal. This is not allowed.",
                                                     sigCtx.inputPort.getSignal().getGlobalId()));
            status.success = false;
        }
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

ProcedureStatus MultipleSharedHandler::signalListChanged(std::vector<SignalContext>& signalContexts)
{
    ProcedureStatus status{true, {}};
    createReader(signalContexts);
    return status;
}

std::string MultipleSharedHandler::toString(const SampleType sampleType, const std::string& valueFieldName, void* data, SizeT offset)
{
    switch (sampleType)
    {
        case SampleType::Float64:
            return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<SampleTypeToType<SampleType::Float64>::Type*>(data) + offset)));
        case SampleType::UInt64:
            return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt64>::Type*>(data) + offset)));
        case SampleType::Int64:
            return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<SampleTypeToType<SampleType::Int64>::Type*>(data) + offset)));
        default:
            break;
    }
    return "";
}

template <typename T>
std::string MultipleSharedHandler::toString(const std::string& valueFieldName, void* data, SizeT offset)
{
    return fmt::format("\"{}\" : {}", valueFieldName, std::to_string(*(static_cast<T*>(data) + offset)));
}

std::string MultipleSharedHandler::tsToString(void* data, SizeT offset)
{
    return fmt::format("\"timestamp\" : {}", std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt64>::Type*>(data) + offset)));
}

std::string MultipleSharedHandler::buildTopicName()
{
    return topic;
}

void MultipleSharedHandler::createReader(const std::vector<SignalContext>& signalContexts)
{
    // signalContexts always contain an unconnected input port
    if (signalContexts.size() <= 1)
        return;

    if (reader.assigned())
        reader.release();

    auto multiReaderBuilder = MultiReaderBuilder().setValueReadType(SampleType::Undefined).setDomainReadType(SampleType::UInt64);
    for (const auto& sContext : signalContexts)
    {
        if (sContext.inputPort.getSignal().assigned())
            multiReaderBuilder.addInputPort(sContext.inputPort);
    }

    reader = multiReaderBuilder.build();
    allocateBuffers(signalContexts);
}

void MultipleSharedHandler::allocateBuffers(const std::vector<SignalContext>& signalContexts)
{
    // Allocate buffers for each signal
    auto signalsCount = signalContexts.size() - 1;
    for (size_t i = 0; i < domainBuffers.size(); ++i)
    {
        std::free(domainBuffers[i]);
        std::free(dataBuffers[i]);
    }

    domainBuffers = std::vector<void*>(signalsCount, nullptr);
    dataBuffers = std::vector<void*>(signalsCount, nullptr);

    for (size_t i = 0; i < signalsCount; ++i)
    {
        dataBuffers[i] = std::malloc(buffersSize * getSampleSize(signalContexts[i].inputPort.getSignal().getDescriptor().getSampleType()));
        domainBuffers[i] = std::malloc(buffersSize * getSampleSize(SampleType::UInt64));
    }
}

std::string MultipleSharedHandler::messageFromFields(const std::vector<std::string>& fields)
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

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
