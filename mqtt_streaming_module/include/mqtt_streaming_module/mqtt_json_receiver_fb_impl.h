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
#include "MqttAsyncClient.h"
#include "MqttDataWrapper.h"
#include <mqtt_streaming_module/common.h>
#include <mqtt_streaming_module/mqtt_base_fb.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttJsonReceiverFbImpl final : public MqttBaseFb
{
    friend class MqttJsonFbHelper;

public:
    explicit MqttJsonReceiverFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const StringPtr& localId,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    ~MqttJsonReceiverFbImpl() override;

    static FunctionBlockTypePtr CreateType();

private:
    mutable std::mutex sync;
    mqtt::MqttDataWrapper jsonDataWorker;
    std::unordered_map<mqtt::SignalId, SignalConfigPtr> outputSignals;
    std::vector<mqtt::SignalId> signalIdList;
    std::unordered_map<mqtt::SignalId, DataDescriptorPtr> subscribedSignals;

    void createSignals() override;
    void clearSubscribedTopics() override;
    std::vector<std::string> getSubscribedTopics() const override;
    void processMessage(const mqtt::MqttMessage& msg) override;
    void readProperties() override;

    void createDataPacket(const std::string& topic, const std::string& json);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
