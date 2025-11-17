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

#include "mqtt_streaming_client_module/common.h"
#include <vector>
#include <mqtt_streaming_client_module/types.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

class HandlerBase
{
public:
    virtual ~HandlerBase() = default;
    virtual MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) = 0;
    virtual ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const = 0;
    virtual ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) = 0;
protected:
    static std::pair<uint64_t, uint64_t> calculateRatio(const DataDescriptorPtr descriptor)
    {
        const auto tickResolution = descriptor.getTickResolution().simplify();
        const uint64_t packetDelta = descriptor.getRule().getParameters().get("delta");
        uint64_t num = tickResolution.getNumerator() * packetDelta;
        uint64_t den = tickResolution.getDenominator();
        const uint64_t g = std::gcd(num, den);
        num /= g;
        den /= g;
        return std::pair<uint64_t, uint64_t>{num, den};
    }

    static uint64_t convertToEpoch(const DataPacketPtr domainPacket)
    {
        const auto [ratioNum, ratioDen]  = calculateRatio(domainPacket.getDataDescriptor());
        constexpr uint64_t US_IN_S = 1'000'000; // amount microseconds in a second
        uint64_t ts = *(static_cast<uint64_t*>(domainPacket.getData()));
        ts = ts * ratioNum * US_IN_S / ratioDen;    // us
        return ts;
    }
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
