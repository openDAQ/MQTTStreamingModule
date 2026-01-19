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
#include <opendaq/function_block_impl.h>
#include <opendaq/streaming_ptr.h>
#include <mqtt_streaming_module/status_helper.h>


BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttClientFbImpl : public FunctionBlock
{
    enum class ConnectionStatus : EnumType
    {
        Connected = 0,
        Reconnecting,
        Disconnected
    };

public:
    explicit MqttClientFbImpl(const ContextPtr& ctx,
                                       const ComponentPtr& parent,
                                       const PropertyObjectPtr& config);

    static FunctionBlockTypePtr CreateType();

protected:
    static std::atomic<int> localIndex;
    static std::string generateLocalId();
    static std::vector<std::pair<MqttClientFbImpl::ConnectionStatus, std::string>> connectionStatusMap;

    void removed() override;

    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;

    void initNestedFbTypes();
    void initMqttSubscriber();
    void initConnectionStatus();
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    bool waitForConnection(const int timeoutMs);

    DictObjectPtr<IDict, IString, IFunctionBlockType> nestedFbTypes;

    StatusHelper<ConnectionStatus> connectionStatus;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    Mqtt::Utils::Settings::MqttConnectionSettings connectionSettings;
    int connectTimeout;

    std::promise<bool> connectedPromise;
    std::future<bool> connectedFuture;
    std::atomic<bool> connectedDone{false};
    std::unordered_map<std::string, std::string> deviceMap;         // device name -> signal list JSON
    std::mutex componentStatusSync;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
