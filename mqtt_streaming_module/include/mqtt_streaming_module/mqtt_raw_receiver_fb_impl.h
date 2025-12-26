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
#include <MqttAsyncClient.h>
#include <mqtt_streaming_module/common.h>
#include <mqtt_streaming_module/mqtt_base_fb.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttRawReceiverFbImpl final : public MqttBaseFb
{
    friend class MqttRawFbTest;

public:
    explicit MqttRawReceiverFbImpl(const ContextPtr& ctx,
                                   const ComponentPtr& parent,
                                   const FunctionBlockTypePtr& type,
                                   std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                   const PropertyObjectPtr& config = nullptr);
    ~MqttRawReceiverFbImpl() override;

    static FunctionBlockTypePtr CreateType();

    std::string getSubscribedTopic() const override;
private:
    std::mutex sync;
    SignalConfigPtr outputSignal;
    std::string topicForSubscribing;
    static std::atomic<int> localIndex;

    static std::string generateLocalId();

    void createSignals() override;
    void clearSubscribedTopic() override;

    void processMessage(const mqtt::MqttMessage& msg) override;
    void readProperties() override;
    void propertyChanged() override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
