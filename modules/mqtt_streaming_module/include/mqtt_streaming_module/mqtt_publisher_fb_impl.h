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
#include "mqtt_streaming_module/status_helper.h"
#include <mqtt_streaming_module/common.h>
#include <mqtt_streaming_module/types.h>
#include <opendaq/function_block_impl.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttPublisherFbImpl final : public FunctionBlock
{
public:
    enum class SignalStatus : EnumType
    {
        NotConnected = 0,
        Invalid,
        Valid
    };

    enum class PublishingStatus : EnumType
    {
        Ok = 0,
        SampleSkipped
    };

    enum class SettingStatus : EnumType
    {
        Valid = 0,
        Invalid
    };

    explicit MqttPublisherFbImpl(const ContextPtr& ctx,
                                 const ComponentPtr& parent,
                                 const FunctionBlockTypePtr& type,
                                 std::shared_ptr<mqtt::MqttAsyncClient> mqttClient,
                                 const PropertyObjectPtr& config = nullptr);
    ~MqttPublisherFbImpl();

    DAQ_MQTT_STREAM_MODULE_API static FunctionBlockTypePtr CreateType();
    DAQ_MQTT_STREAM_MODULE_API PublisherFbConfig getFbConfig() const;

    void onConnected(const InputPortPtr& port) override;
    void onDisconnected(const InputPortPtr& port) override;

    inline static const std::vector<std::pair<SignalStatus, std::string>> signalStatusMap =
		{{SignalStatus::NotConnected, "NotConnected"},
		 {SignalStatus::Invalid, "Invalid"},
		 {SignalStatus::Valid, "Valid"}};
    inline static const std::vector<std::pair<PublishingStatus, std::string>> publishingStatusMap =
		{{PublishingStatus::Ok, "Ok"},
		 {PublishingStatus::SampleSkipped, "SampleSkipped"}};
    inline static const std::vector<std::pair<SettingStatus, std::string>> settingStatusMap =
		{{SettingStatus::Valid, "Valid"},
		 {SettingStatus::Invalid, "Invalid"}};

private:
    static std::atomic<int> localIndex;
    std::shared_ptr<mqtt::MqttAsyncClient> mqttClient;
    mqtt::MqttDataWrapper jsonDataWorker;
    PublisherFbConfig config;
    std::vector<SignalContext> signalContexts;
    std::unordered_map<std::string, WeakRefPtr<ISignal>> signalMap;
    std::thread readerThread;
    std::atomic<bool> running;
    std::atomic<bool> hasSignalError;
    std::atomic<bool> signalDescriptorChanged;
    std::atomic<bool> signalAttributeChanged;
    std::atomic<bool> hasSettingError;
    std::atomic<bool> hasEmptyTopic;
    std::vector<std::string> signalErrors;
    std::vector<std::string> settingErrors;
    std::unique_ptr<HandlerBase> handler;
    std::mutex processingMutex;
    StatusHelper<SignalStatus> signalStatus;
    StatusHelper<PublishingStatus> publishingStatus;
    StatusHelper<SettingStatus> settingStatus;
    std::atomic<bool> hasSkippedMsg;
    std::string lastSkippedReason;
    SignalConfigPtr commonPreviewSignal;

    static std::string generateLocalId();
    void coreEventCallback(ComponentPtr& sender, CoreEventArgsPtr& eventArgs);
    void updateComponentStatus();
    void updatePublishingStatus();
    std::string buildPublishingStatusMessage();
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();
    void updatePortsAndSignals(bool reassignPorts);
    void clearPorts();
    void updateCoreEventCallbacks();
    void clearCoreEventCallbacks(const std::unordered_map<std::string, WeakRefPtr<ISignal>>& signalMap);
    void updateStatuses();
    void validateInputPorts();
    void updateTopics();
    void updateSchema();
    template <typename retT, typename intfT>
    retT readProperty(const std::string& propertyName, const retT defaultValue);
    void runReaderThread();
    void readerLoop();
    void sendMessages(const MqttData& data);
    void removed() override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
