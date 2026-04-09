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
#include "mqtt_streaming_module/common.h"
#include <mqtt_streaming_module/types.h>
#include <opendaq/sample_type_traits.h>
#include <vector>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class HandlerBase
{
public:
    HandlerBase(WeakRefPtr<IFunctionBlock> parentFb)
        : parentFb(parentFb)
    {
    }
    virtual ~HandlerBase() = default;
    virtual MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) = 0;
    virtual ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const
    {
        ProcedureStatus status{true, {}};
        std::unordered_set<std::string> globalIdSet;
        for (const auto& sigCtx : signalContexts)
        {
            auto signal = sigCtx.inputPort.getSignal();
            if (!signal.assigned())
                continue;
            std::string globalId = signal.getGlobalId();
            if (globalIdSet.find(globalId) != globalIdSet.end())
            {
                status.addError(fmt::format("Connected signals have non-unique GlobalIDs (\"{}\").", globalId));
            }
            globalIdSet.insert(std::move(globalId));
        }
        return status;
    }
    virtual ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) = 0;
    virtual ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) = 0;
    virtual std::string getSchema() = 0;

protected:
    WeakRefPtr<IFunctionBlock> parentFb;

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

    static uint64_t convertToEpoch(const DataPacketPtr domainPacket, size_t offset)
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
            ts = *(static_cast<uint64_t*>(domainPacket.getData()) + offset);
        else if (domainPacket.getDataDescriptor().getSampleType() == SampleType::Int64)
            ts = *(static_cast<int64_t*>(domainPacket.getData()) + offset);
        ts = ts * num / den; // us
        return ts;
    }

    static std::string toString(const DataPacketPtr& dataPacket, size_t offset)
    {
        auto sampleType = dataPacket.getDataDescriptor().getSampleType();
        std::string data("unsupported");

        switch (sampleType)
        {
            case SampleType::Float64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Float64>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::Float32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Float32>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::UInt64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt64>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::Int64:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int64>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::UInt32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt32>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::Int32:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int32>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::UInt16:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt16>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::Int16:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int16>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::UInt8:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::UInt8>::Type*>(dataPacket.getData()) + offset));
                break;
            case SampleType::Int8:
                data = std::to_string(*(static_cast<SampleTypeToType<SampleType::Int8>::Type*>(dataPacket.getData()) + offset));
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

    static std::string buildValueFieldNameForSchema(SignalValueJSONKey signalNamesMode, std::string postfix = "")
    {
        std::string valueFieldName;
        if (signalNamesMode == SignalValueJSONKey::LocalID)
        {
            valueFieldName = fmt::format("<signal{}_local_id>", postfix);
        }
        else if (signalNamesMode == SignalValueJSONKey::Name)
        {
            valueFieldName = fmt::format("<signal{}_name>", postfix);
        }
        else
        {
            valueFieldName = fmt::format("<signal{}_global_id>", postfix);
        }
        return valueFieldName;
    }
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
