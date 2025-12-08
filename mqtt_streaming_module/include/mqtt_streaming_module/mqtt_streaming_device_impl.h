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
#include "MqttSettings.h"
#include <future>
#include <mqtt_streaming_module/common.h>
#include <opendaq/device_impl.h>
#include <opendaq/streaming_ptr.h>
#include "MqttDataWrapper.h"


BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttStreamingDeviceImpl : public Device
{
public:
    explicit MqttStreamingDeviceImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const PropertyObjectPtr& config);

protected:
    static std::atomic<int> localIndex;
    static std::string getLocalId();

    void removed() override;
    DeviceInfoPtr onGetInfo() override;

    bool allowAddFunctionBlocksFromModules() override
    {
        return true;
    };
    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;

    void initBaseFunctionalBlocks();
    void initMqttSubscriber();
    void buildFunctionBlockTypes();
    bool waitForConnection(const int timeoutMs);
    void receiveSignalTopics(const int timeoutMs);
    void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, mqtt::MqttMessage& msg);

    DictObjectPtr<IDict, IString, IFunctionBlockType> fbTypes;
    DictObjectPtr<IDict, IString, IFunctionBlockType> baseFbTypes;

    StringPtr connectionString;
    EnumerationPtr connectionStatus;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    Mqtt::Utils::Settings::MqttConnectionSettings connectionSettings;

    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;
    std::atomic<bool> connectedDone{false};
    std::unordered_map<std::string, std::string> deviceMap;         // device name -> signal list JSON
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
