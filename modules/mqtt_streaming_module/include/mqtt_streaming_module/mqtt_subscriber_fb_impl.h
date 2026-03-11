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
#include <opendaq/function_block_impl.h>
#include "mqtt_streaming_module/constants.h"
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttSubscriberFbImpl final : public FunctionBlock
{
    friend class MqttSubscriberFbHelper;
    friend class MqttJsonDecoderFbHelper;

public:
    struct CmdResult
    {
        bool success = false;
        std::string msg;
        int token = 0;

        CmdResult(bool success = false, const std::string& msg = "", int token = 0)
            : success(success),
              msg(msg),
              token(token)
        {
        }
    };

    enum class DomainSignalMode : EnumType
    {
        None = 0,
        SystemTime,
        _count
    };

    explicit DAQ_MQTT_STREAM_MODULE_API MqttSubscriberFbImpl(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    DAQ_MQTT_STREAM_MODULE_API ~MqttSubscriberFbImpl() override;

    DAQ_MQTT_STREAM_MODULE_API static FunctionBlockTypePtr CreateType();

    DAQ_MQTT_STREAM_MODULE_API std::string getSubscribedTopic() const;

protected:
    static std::atomic<int> localIndex;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    int qos = DEFAULT_SUB_QOS;
    mqtt::MqttDataWrapper jsonDataWorker;
    std::string topicForSubscribing;
    DictObjectPtr<IDict, IString, IFunctionBlockType> nestedFbTypes;
    std::vector<FunctionBlockPtr> nestedFunctionBlocks;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;
    bool enablePreview;
    DomainSignalMode previewDomainMode;
    bool previewIsString;
    std::atomic<bool> waitingForData;
    uint64_t lastTsValue;

    DAQ_MQTT_STREAM_MODULE_API void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, const mqtt::MqttMessage& msg);

    static std::string generateLocalId();

    void initNestedFbTypes();

    void createSignals();
    SignalConfigPtr createDomainSignal();
    void removePreviewSignal();
    void removeDomainSignal();
    void reconfigureSignal();
    void clearSubscribedTopic();

    DataPacketPtr createDomainDataPacket(const uint64_t epochTime);

    void processMessage(const mqtt::MqttMessage& msg);
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void readJsonConfig();
    std::pair<bool, std::string> readFileToString(const std::string& filePath);
    void setJsonConfig(const std::string config);
    void propertyChanged();

    bool setTopic(std::string topic);
    CmdResult subscribeToTopic();
    CmdResult unsubscribeFromTopic();

    void removed() override;
    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;
    void onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock) override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
