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

class SignalArrayAtomicSampleHandler : public HandlerBase
{
public:
    explicit SignalArrayAtomicSampleHandler(WeakRefPtr<IFunctionBlock> parentFb, SignalValueJSONKey signalNamesMode, std::string topic);

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override;
    ListPtr<IString> getTopics(const std::vector<SignalContext>& signalContexts) override;
    std::string getSchema() override;

protected:
    const std::string topic;
    SignalValueJSONKey signalNamesMode;

    std::string toString(const std::string valueFieldName, daq::DataPacketPtr packet);
    std::string buildTopicName();
    static std::string messageFromArray(const std::vector<std::string>& array);


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
