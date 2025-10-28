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
#include <opendaq/function_block_impl.h>

#include <MqttAsyncClient.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
    
class MqttRawReceiverFbImpl final : public FunctionBlock
{
    friend class MqttStreamingClientModuleTest;
public:
    explicit MqttRawReceiverFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const StringPtr& localId,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    ~MqttRawReceiverFbImpl() override;

    static std::string buildSignalNameFromTopic(std::string topic);

private:
    std::unordered_map<std::string, SignalConfigPtr> outputSignals;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    ListObjectPtr<IList, IString> topicsForSubscribing;

    std::mutex sync;

    void createSignals();

    void createAndSendDataPacket(mqtt::MqttMessage& msg);

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();

    void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg);

    void subscribeToTopics();
    void unsubscribeFromTopics();
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
