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

#include "group_signal_shared_ts_handler.h"

#include <mqtt_streaming_module/handler_base.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/multi_reader_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class StringDataBuilder
{
public:
    StringDataBuilder(size_t packSize)
        : packSize(packSize)
    {
        reset();
    }

    void append(const SampleType sampleType, void* data, size_t size)
    {
        switch (sampleType)
        {
            case SampleType::Float64:
                toString<SampleTypeToType<SampleType::Float64>::Type>(data, size);
                break;
            case SampleType::Float32:
                toString<SampleTypeToType<SampleType::Float32>::Type>(data, size);
                break;
            case SampleType::UInt64:
                toString<SampleTypeToType<SampleType::UInt64>::Type>(data, size);
                break;
            case SampleType::Int64:
                toString<SampleTypeToType<SampleType::Int64>::Type>(data, size);
                break;
            case SampleType::UInt32:
                toString<SampleTypeToType<SampleType::UInt32>::Type>(data, size);
                break;
            case SampleType::Int32:
                toString<SampleTypeToType<SampleType::Int32>::Type>(data, size);
                break;
            case SampleType::UInt16:
                toString<SampleTypeToType<SampleType::UInt16>::Type>(data, size);
                break;
            case SampleType::Int16:
                toString<SampleTypeToType<SampleType::Int16>::Type>(data, size);
                break;
            case SampleType::UInt8:
                toString<SampleTypeToType<SampleType::UInt8>::Type>(data, size);
                break;
            case SampleType::Int8:
                toString<SampleTypeToType<SampleType::Int8>::Type>(data, size);
                break;
            default:
                break;
        }
    }

    std::string getPack(const std::string& valueFieldName)
    {
        std::string result;
        if (!values.empty())
        {
            result = fmt::format("\"{}\" : {}", valueFieldName, values.front());
            values.pop_front();
        }

        return result;
    }

    bool empty() const
    {
        return values.empty();
    }

protected:
    const size_t packSize;
    std::list<std::string> values;
    size_t offset = 0;
    std::ostringstream oss;

    template <typename T>
    void toString(void* data, size_t size)
    {
        for (size_t dataOffset = 0; dataOffset < size; ++dataOffset)
        {
            prefix();
            oss << std::to_string(*(static_cast<T*>(data) + dataOffset));
            ++offset;
            postfix();
        }
    }

    void prefix()
    {
        if (offset > 0)
            oss << ", ";
    }

    void postfix()
    {
        if (offset == packSize)
        {
            offset = 0;
            oss << "]";
            values.push_back(std::move(oss).str());
            reset();
        }
    }

    void reset()
    {
        oss.clear();
        oss.str("");
        oss << "[";
    }
};

class StringTsBuilder : public StringDataBuilder
{
public:
    StringTsBuilder(size_t packSize)
        : StringDataBuilder(packSize)
    {
    }

    void append(const TimestampTickStruct tsStruct, size_t size)
    {
        for (size_t dataOffset = 0; dataOffset < size; ++dataOffset)
        {
            prefix();
            oss << std::to_string(tsStruct.tsToTicks(dataOffset));
            ++offset;
            postfix();
        }
    }

    std::string getPack()
    {
        return StringDataBuilder::getPack("timestamp");
    }
};

class GroupSignalSharedTsArrHandler : public GroupSignalSharedTsHandler
{
public:
    explicit GroupSignalSharedTsArrHandler(WeakRefPtr<IFunctionBlock> parentFb,
                                           SignalValueJSONKey signalNamesMode,
                                           std::string topic,
                                           size_t packSize);
    ~GroupSignalSharedTsArrHandler() = default;

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;

protected:
    const size_t packSize;
    std::vector<StringDataBuilder> dataBuilders;
    StringTsBuilder tsBuilder;

    void initDataBuilders(const size_t size);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
