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
#include "mqtt_streaming_module/handler_base.h"
#include <mqtt_streaming_module/common.h>
#include <mqtt_streaming_module/types.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttPublisherFbImpl final : public FunctionBlock
{
public:
    explicit MqttPublisherFbImpl(const ContextPtr& ctx,
                                 const ComponentPtr& parent,
                                 const FunctionBlockTypePtr& type,
                                 std::shared_ptr<mqtt::MqttAsyncClient> mqttClient,
                                 const PropertyObjectPtr& config = nullptr);
    ~MqttPublisherFbImpl();

    static FunctionBlockTypePtr CreateType();
    PublisherFbConfig getFbConfig() const;

    void onConnected(const InputPortPtr& port) override;
    void onDisconnected(const InputPortPtr& port) override;

private:
    static std::atomic<int> localIndex;
    std::shared_ptr<mqtt::MqttAsyncClient> mqttClient;
    mqtt::MqttDataWrapper jsonDataWorker;
    PublisherFbConfig config;
    std::vector<SignalContext> signalContexts;
    std::atomic<int> inputPortCount;
    std::thread readerThread;
    std::atomic<bool> running;
    std::atomic<bool> hasError;
    std::unique_ptr<HandlerBase> handler;

    static std::string getLocalId();
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();
    void updateInputPorts();
    void validateInputPorts();
    template <typename retT, typename intfT>
    retT readProperty(const std::string& propertyName, const retT defaultValue);
    void runReaderThread();
    void readerLoop();
    void sendMessages(const MqttData& data);
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
