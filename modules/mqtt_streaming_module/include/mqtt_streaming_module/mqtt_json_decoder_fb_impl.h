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
#include "mqtt_streaming_protocol/MqttDataWrapper.h"
#include <mqtt_streaming_module/common.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttJsonDecoderFbImpl final : public FunctionBlock
{
public:

    enum class ParsingStatus : EnumType
    {
        InvalidParamaters = 0,
        ParsingFailed,
        WaitingForData,
        ParsingSuccedeed
    };

    explicit MqttJsonDecoderFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const PropertyObjectPtr& config = nullptr);

    DAQ_MQTT_STREAM_MODULE_API static FunctionBlockTypePtr CreateType();
    DAQ_MQTT_STREAM_MODULE_API void processMessage(const std::string& json, const uint64_t externalTs);
protected:

    struct FbConfig {
        std::string valueFieldName;
        mqtt::MqttDataWrapper::DomainSignalMode tsMode;
        std::string tsFieldName;
        std::string unitSymbol;
    };

    static std::atomic<int> localIndex;

    mqtt::MqttDataWrapper jsonDataWorker;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;
    std::atomic<bool> waitingData;
    std::atomic<bool> configValid;
    std::string configMsg;
    std::atomic<bool> parsingSucceeded;
    std::atomic<bool> externalTsDuplicate;
    std::string parsingMsg;
    uint64_t lastExternalTs;

    FbConfig config;

    static std::string generateLocalId();

    void createSignal();
    void reconfigureSignal(const FbConfig& prevConfig);
    SignalConfigPtr createDomainSignal();

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();

    void updateStatuses();
    void checkExternalTs(const uint64_t externalTs);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
