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
#include <opendaq/module_impl.h>
#include <opendaq/device_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

class MqttStreamingClientModule final : public Module
{
public:
    MqttStreamingClientModule(ContextPtr context);

    ListPtr<IDeviceInfo> onGetAvailableDevices() override;
    DictPtr<IString, IDeviceType> onGetAvailableDeviceTypes() override;
    DevicePtr onCreateDevice(const StringPtr& connectionString,
                             const ComponentPtr& parent,
                             const PropertyObjectPtr& config) override;
    bool acceptsConnectionParameters(const StringPtr& connectionString, const PropertyObjectPtr& config);
    Bool onCompleteServerCapability(const ServerCapabilityPtr& source, const ServerCapabilityConfigPtr& target) override;  // ???

private:
    void extractConnectionParams(const StringPtr& connectionString, const PropertyObjectPtr& config, std::string& hostType);
    static DeviceTypePtr createDeviceType();
    static PropertyObjectPtr createDefaultConfig();
    static PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& config);

    std::mutex sync;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
