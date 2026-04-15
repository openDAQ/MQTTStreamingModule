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
#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/status_container.h"
#include "mqtt_streaming_protocol/MqttAsyncClient.h"
#include "mqtt_streaming_protocol/common.h"
#include <mqtt_streaming_module/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/function_block_impl.h>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttSubscriberFbImpl final : public FunctionBlock
{
    friend class MqttSubscriberFbHelper;
    friend class MqttJsonDecoderFbHelper;

public:
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

    std::string topicForSubscribing;
    int qos;
    bool enablePreview;
    DomainSignalMode previewDomainMode;
    bool previewIsString;
    std::atomic<uint32_t> dataIntervalMs;

    uint64_t lastTsValue;

    DictObjectPtr<IDict, IString, IFunctionBlockType> nestedFbTypes;
    std::vector<FunctionBlockPtr> nestedFunctionBlocks;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;

    std::shared_ptr<utils::StatusContainer> statuses;
    utils::Error configErr;
    utils::Error jsonConfigErr;
    utils::Error domainValueErr;
    utils::Error subscriptionErr;
    utils::Error dataAcqStatus;

    std::thread processingThread;
    std::atomic<bool> processingRunning{false};
    std::atomic<bool> forceProcessing{false};
    std::queue<std::pair<mqtt::MqttMessage, uint64_t>> messageQueue;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    mutable std::recursive_mutex processingMutex;
    std::mutex componentStatusMutex;

    DAQ_MQTT_STREAM_MODULE_API void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, const mqtt::MqttMessage& msg);

    static std::string generateLocalId();

    void initStatusContainer();
    DAQ_MQTT_STREAM_MODULE_API void updateStatuses();

    void initNestedFbTypes();

    void createSignals();
    SignalConfigPtr createDomainSignal();
    void removePreviewSignal();
    void removeDomainSignal();
    void reconfigureSignal();
    void clearSubscribedTopic();

    DAQ_MQTT_STREAM_MODULE_API DataPacketPtr createDomainDataPacket(const uint64_t epochTime);

    void processMessage(const mqtt::MqttMessage& msg);
    void processMessageImpl(const mqtt::MqttMessage& msg, const uint64_t epochTime);
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void readJsonConfig();
    std::pair<bool, std::string> readFileToString(const std::string& filePath);
    void setJsonConfig(const std::string config);
    void propertyChanged();

    mqtt::CmdResult setTopic(std::string topic);
    mqtt::CmdResult subscribeToTopic();
    mqtt::CmdResult unsubscribeFromTopic();

    void startProcessingThread();
    void stopProcessingThread();
    void processingLoop();

    void removed() override;
    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;
    void onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock) override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
