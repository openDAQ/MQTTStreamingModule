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
#include <mqtt_streaming_module/atomic_signal_atomic_sample_handler.h>
#include <list>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class AtomicSignalSampleArrayHandler : public AtomicSignalAtomicSampleHandler
{
public:

    struct SignalBuffer
    {
        std::list<DataPacketPtr> data;
        size_t dataSize = 0;
        size_t offset = 0;
        void clear()
        {
            data.clear();
            dataSize = 0;
            offset = 0;
        }
    };
    explicit AtomicSignalSampleArrayHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, size_t packSize);
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;

protected:
    size_t packSize;
    std::unordered_map<std::string, SignalBuffer> signalBuffers;

    MqttData processSignalContext(SignalContext& signalContext) override;
    MqttDataSample processDataPackets(SignalContext& signalContext);
    std::string toString(const std::string valueFieldName, SignalContext& signalContext);
    std::pair<DataPacketPtr, size_t> getSample(SignalContext& signalContext);
    void
    processSignalDescriptorChanged(SignalContext& signalCtx, const DataDescriptorPtr& valueSigDesc, const DataDescriptorPtr& domainSigDesc);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
