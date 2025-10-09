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
#include <mqtt_streaming_client_module/common.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/function_block_type_factory.h>
#include <opendaq/function_block_impl.h>
#include <opendaq/signal_config_ptr.h>

#include "MqttAsyncClient.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
    
class MqttReceiverFbImpl final : public FunctionBlock
{
public:
    explicit MqttReceiverFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const StringPtr& localId,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    ~MqttReceiverFbImpl() override;

private:
    std::unordered_map<std::string, SignalConfigPtr> outputSignals;
    std::unordered_map<std::string, SignalConfigPtr> outputDomainSignals;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    DictObjectPtr<IDict, IString, IDataDescriptor> subscribedSignals;

    std::mutex sync;

    void createSignals();

    void parseMessage(mqtt::MqttMessage& msg);
    void createDataPacket(const std::string& topic, double value, UInt timestamp);

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();

    void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg);

    std::string buildSignalNameFromTopic(std::string topic) const;
    std::string buildDomainSignalNameFromTopic(std::string topic) const;

};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
