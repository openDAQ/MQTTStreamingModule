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
#include "MqttDataWrapper.h"
#include <mqtt_streaming_module/common.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttJsonDecoderFbImpl final : public FunctionBlock
{
    // friend class MqttJsonFbHelper;

public:
    explicit MqttJsonDecoderFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const PropertyObjectPtr& config = nullptr);

    static FunctionBlockTypePtr CreateType();
    void processMessage(const std::string& json);
protected:

    struct FbConfig {
        std::string valueFieldName;
        std::string tsFieldName;
        std::string unitSymbol;
        std::string signalName;
    };
    static std::atomic<int> localIndex;

    mqtt::MqttDataWrapper jsonDataWorker;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;

    FbConfig config;

    static std::string getLocalId();

    void createSignal();
    void reconfigureSignal(const FbConfig& prevConfig);
    SignalConfigPtr createDomainSignal();

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    template <typename retT, typename intfT>
    retT readProperty(const std::string& propertyName, const retT defaultValue);
    void propertyChanged();
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
