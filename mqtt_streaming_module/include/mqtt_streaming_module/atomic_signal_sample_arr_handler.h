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

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class AtomicSignalSampleArrayHandler : public AtomicSignalAtomicSampleHandler
{
public:
    explicit AtomicSignalSampleArrayHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, size_t packSize);
    std::string getSchema() override;

protected:
    size_t packSize;

    MqttData processSignalContext(SignalContext& signalContext) override;
    MqttDataSample processDataPackets(SignalContext& signalContext);
    std::string toString(const std::string valueFieldName, SignalContext& signalContext);
    std::pair<DataPacketPtr, size_t> getSample(SignalContext& signalContext);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
