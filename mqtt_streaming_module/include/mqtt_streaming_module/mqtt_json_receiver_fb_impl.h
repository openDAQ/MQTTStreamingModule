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
    friend class MqttJsonFbTest;
    friend class MqttJsonDecoderFbHelper;

public:
    explicit MqttJsonReceiverFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    ~MqttJsonReceiverFbImpl() override;

    static FunctionBlockTypePtr CreateType();

    std::string getSubscribedTopic() const override;

protected:
    mqtt::MqttDataWrapper jsonDataWorker;
    std::unordered_map<mqtt::SignalId, SignalConfigPtr> outputSignals;
    std::string topicForSubscribing;
    static std::atomic<int> localIndex;

    DictObjectPtr<IDict, IString, IFunctionBlockType> baseFbTypes;
    std::vector<FunctionBlockPtr> nestedFunctionBlocks;

    static std::string getLocalId();

    void initBaseFunctionalBlocks();

    void createSignals() override;
    void clearSubscribedTopic() override;

    void processMessage(const mqtt::MqttMessage& msg) override;
    void initProperties(const PropertyObjectPtr& config) override;
    void readProperties() override;
    void readJsonConfig();
    std::pair<bool, std::string> readFileToString(const std::string& filePath);
    void setJsonConfig(const std::string config);
    void propertyChanged() override;

    bool setTopic(std::string topic);

    // void createDataPacket(const std::string& topic, const std::string& json);

    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;
    void onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock) override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
