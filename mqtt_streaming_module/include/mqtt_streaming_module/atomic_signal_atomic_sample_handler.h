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

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class AtomicSignalAtomicSampleHandler : public HandlerBase
{
public:
    explicit AtomicSignalAtomicSampleHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode);

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override
    {
        return ProcedureStatus{true, {}};
    };
    ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;
protected:
    virtual MqttData processSignalContext(SignalContext& signalContext);
    void
    processSignalDescriptorChanged(SignalContext& signalCtx, const DataDescriptorPtr& valueSigDesc, const DataDescriptorPtr& domainSigDesc);
    MqttDataSample processDataPacket(SignalContext& signalContext, const DataPacketPtr& dataPacket, size_t offset);
    std::string toString(const std::string valueFieldName, daq::DataPacketPtr packet, size_t offset);
    std::string buildTopicName(const SignalContext& signalContext);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
