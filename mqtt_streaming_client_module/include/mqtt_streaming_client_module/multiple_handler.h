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

#include <mqtt_streaming_client_module/handler_base.h>
#include <opendaq/multi_reader_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

class MultipleHandler : public HandlerBase
{
public:
    explicit MultipleHandler(bool useSignalNames, std::string topic);

    MqttData processSignalContexts(std::vector<SignalContext>& signalContexts) override;
    ProcedureStatus validateSignalContexts(const std::vector<SignalContext>& signalContexts) const override;
    ProcedureStatus signalListChanged(std::vector<SignalContext>& signalContexts) override;
protected:
    bool useSignalNames;
    const size_t buffersSize;
    const std::string topic;
    std::vector<void*> domainBuffers;
    std::vector<void*> dataBuffers;
    daq::MultiReaderPtr reader;

    template<typename T>
    std::string toString(const std::string& valueFieldName, void* data, SizeT offset);
    std::string toString(const SampleType sampleType, const std::string& valueFieldName, void* data, SizeT offset);
    std::string tsToString(void* data, SizeT offset);
    std::string buildTopicName();
    void createReader(const std::vector<SignalContext>& signalContexts);
    void allocateBuffers(const std::vector<SignalContext>& signalContexts);
    static std::string messageFromFields(const std::vector<std::string>& fields);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
