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
#include <mqtt_streaming_module/common.h>
#include <opendaq/module_impl.h>
#include <opendaq/device_ptr.h>
#include <mutex>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class MqttStreamingModule final : public Module
{
    friend class MqttStreamingClientModuleTest;
public:
    MqttStreamingModule(ContextPtr context);

    DictPtr<IString, IDeviceType> onGetAvailableDeviceTypes() override;
    DevicePtr onCreateDevice(const StringPtr& connectionString,
                             const ComponentPtr& parent,
                             const PropertyObjectPtr& config) override;

private:
    /// Host and port taken from a `daq.mqtt://host[:port]` connection string.
    struct BrokerAddress
    {
        std::string host;
        uint16_t port{0};   // 0 when the connection string carries no port
    };

    /// Throws InvalidParameterException when the string is not a valid `daq.mqtt://` address.
    static BrokerAddress parseConnectionString(const StringPtr& connectionString);
    static StringPtr formatConnectionString(const MqttStreamingModule::BrokerAddress& conParam);

    static DeviceTypePtr createDeviceType();

    static PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& config);

    std::mutex sync;
    size_t deviceIndex{0};
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
