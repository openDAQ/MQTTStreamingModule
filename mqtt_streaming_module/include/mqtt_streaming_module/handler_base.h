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

#include "mqtt_streaming_module/common.h"
#include <mqtt_streaming_module/types.h>
#include <opendaq/sample_type_traits.h>
#include <vector>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class HandlerBase
{
public:
    virtual ~HandlerBase() = default;
    virtual MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) = 0;
    virtual ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const = 0;
    virtual ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) = 0;
    virtual ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) = 0;

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
        constexpr uint64_t US_IN_S = 1'000'000; // amount microseconds in a second
        const auto tickResolution = domainPacket.getDataDescriptor().getTickResolution().simplify();
        uint64_t num = tickResolution.getNumerator() * US_IN_S;
        uint64_t den = tickResolution.getDenominator();
        const uint64_t g = std::gcd(num, den);
        num /= g;
        den /= g;

        uint64_t ts = 0;
        if (domainPacket.getDataDescriptor().getSampleType() == SampleType::UInt64)
            ts = *(static_cast<uint64_t*>(domainPacket.getData()));
        else if (domainPacket.getDataDescriptor().getSampleType() == SampleType::Int64)
            ts = *(static_cast<int64_t*>(domainPacket.getData()));
        ts = ts * num / den; // us
        return ts;
    }

    static std::string toString(const DataPacketPtr& dataPacket)
    {
        auto sampleType = dataPacket.getDataDescriptor().getSampleType();
        std::string data("unsupported");

        switch (sampleType)
        {
            case SampleType::Float64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Float64>::Type*>(dataPacket.getData())));
                break;
            case SampleType::Float32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Float32>::Type*>(dataPacket.getData())));
                break;
            case SampleType::UInt64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt64>::Type*>(dataPacket.getData())));
                break;
            case SampleType::Int64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int64>::Type*>(dataPacket.getData())));
                break;
            case SampleType::UInt32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt32>::Type*>(dataPacket.getData())));
                break;
            case SampleType::Int32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int32>::Type*>(dataPacket.getData())));
                break;
            case SampleType::UInt16:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt16>::Type*>(dataPacket.getData())));
                break;
            case SampleType::Int16:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int16>::Type*>(dataPacket.getData())));
                break;
            case SampleType::UInt8:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt8>::Type*>(dataPacket.getData())));
                break;
            case SampleType::Int8:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int8>::Type*>(dataPacket.getData())));
                break;
            case SampleType::String:
            case SampleType::Binary:
                data = '\"' + std::string(static_cast<char*>(dataPacket.getData()), dataPacket.getDataSize()) + '\"';
                break;
            default:
                break;
        }
        return data;
    }

    static std::string buildValueFieldName(SignalValueJSONKey signalNamesMode, daq::SignalPtr signal)
    {
        std::string valueFieldName;
        if (signalNamesMode == SignalValueJSONKey::LocalID)
        {
            valueFieldName = signal.getLocalId().toStdString();
        }
        else if (signalNamesMode == SignalValueJSONKey::Name)
        {
            valueFieldName = signal.getName().toStdString();
        }
        else
        {
            valueFieldName = signal.getGlobalId().toStdString();
        }
        return valueFieldName;
    }
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
