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
#include <set>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class RawHandler : public HandlerBase
{
public:
    explicit RawHandler(WeakRefPtr<IFunctionBlock> parentFb);

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>&) override
    {
        return ProcedureStatus{true, {}};
    };
    ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;
protected:
    virtual MqttData processSignalContext(SignalContext& signalContext);
    void
    processSignalDescriptorChanged(SignalContext& signalCtx, const DataDescriptorPtr& valueSigDesc, const DataDescriptorPtr& domainSigDesc);
    MqttDataSamplePtr processDataPacket(SignalContext& signalContext, const DataPacketPtr& dataPacket, size_t offset);
    std::string buildTopicName(const SignalContext& signalContext);
    std::vector<uint8_t> toDataBuffer(daq::DataPacketPtr packet, size_t offset);

    inline static const std::set<SampleType> allowedSampleTypes{SampleType::Float64,
                                                         SampleType::Float32,
                                                         SampleType::UInt8,
                                                         SampleType::Int8,
                                                         SampleType::UInt16,
                                                         SampleType::Int16,
                                                         SampleType::UInt32,
                                                         SampleType::Int32,
                                                         SampleType::UInt64,
                                                         SampleType::Int64,
                                                         SampleType::Binary,
                                                         SampleType::String};
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
