/*
 * Copyright 2022-2025 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <opendaq/function_block_ptr.h>
#include <mqtt_streaming_module/handler_base.h>
#include <opendaq/multi_reader_ptr.h>
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

struct TimestampTickStruct
{
    uint64_t firstTick;
    uint64_t delta;
    uint64_t ratioNum;
    uint64_t ratioDen;
    uint64_t multiplier;

    uint64_t tsToTicks(size_t offset) const
    {
        // const uint64_t epochTime = (firstTick + delta * offset) * ratioNum * US_IN_S / ratioDen;    // us
        uint64_t res = ((firstTick + delta * offset) * ratioNum * multiplier) / ratioDen;
        return res;
    }
};

class GroupSignalSharedTsHandler : public HandlerBase
{
public:
    explicit GroupSignalSharedTsHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, std::string topic);
    ~GroupSignalSharedTsHandler();

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override;
    ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;

protected:
    const size_t buffersSize;
    const std::string topic;
    SignalValueJSONKey signalNamesMode;
    std::vector<void*> dataBuffers;
    bool firstDescriptorChange;
    daq::MultiReaderPtr reader;
    std::mutex sync;

    template <typename T>
    std::string toString(const std::string& valueFieldName, void* data, SizeT offset);
    std::string toString(const SampleType sampleType, const std::string& valueFieldName, void* data, SizeT offset);
    std::string tsToString(TimestampTickStruct tsStruct, SizeT offset);
    std::string buildTopicName();
    void createReader(const std::vector<SignalContext>& signalContexts);
    void createReaderInternal(const std::vector<SignalContext>& signalContexts);
    void allocateBuffers(const std::vector<SignalContext>& signalContexts);
    void deallocateBuffers();
    static std::string messageFromFields(const std::vector<std::string>& fields);
    static TimestampTickStruct domainToTs(const MultiReaderStatusPtr status);
    bool processEvents(const daq::MultiReaderStatusPtr& status);    // true if descriptor changed or invalid reader


    inline static const std::set<SampleType> allowedSampleTypes{SampleType::Float64,
                                                                SampleType::Float32,
                                                                SampleType::UInt8,
                                                                SampleType::Int8,
                                                                SampleType::UInt16,
                                                                SampleType::Int16,
                                                                SampleType::UInt32,
                                                                SampleType::Int32,
                                                                SampleType::UInt64,
                                                                SampleType::Int64};
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
