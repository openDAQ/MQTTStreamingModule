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
#include "mqtt_streaming_protocol/MqttAsyncClient.h"
#include "mqtt_streaming_protocol/MqttSettings.h"
#include <future>
#include <mqtt_streaming_module/common.h>
#include <opendaq/device_impl.h>


BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttClientDeviceImpl : public Device
{
public:
    explicit MqttClientDeviceImpl(const ContextPtr& ctx,
                                  const ComponentPtr& parent,
                                  const StringPtr& localId,
                                  const StringPtr& connectionString,
                                  const std::string& brokerHost,
                                  const PropertyObjectPtr& config);

    static DeviceTypePtr CreateType();
    static PropertyObjectPtr CreateDefaultConfig();

protected:
    void removed() override;

    DeviceInfoPtr onGetInfo() override;

    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;

    void initNestedFbTypes();
    void initMqttSubscriber();
    void initConnectionStatus();
    void initProperties(const PropertyObjectPtr& config);
    void readProperties(const PropertyObjectPtr& config);
    bool waitForConnection(const int timeoutMs);

    /// Pushes a ConnectionStatusType value into the device's connection status container.
    /// Surfaces to clients under the "ConfigurationStatus" alias.
    void setConnectionStatus(const std::string& value, const std::string& message = "");

    DictObjectPtr<IDict, IString, IFunctionBlockType> nestedFbTypes;

    StringPtr connectionString;
    int connectTimeout;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    Mqtt::Utils::Settings::MqttConnectionSettings connectionSettings;

    std::future<bool> connectedFuture;
    std::atomic<bool> connectedDone{false};
    std::unordered_map<std::string, std::string> deviceMap;         // device name -> signal list JSON
    std::mutex componentStatusSync;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
