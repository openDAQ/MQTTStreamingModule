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

#include "mqtt_streaming_client_module/types.h"
#include <mqtt_streaming_client_module/single_handler.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

class HandlerFactory
{
public:
    static std::unique_ptr<HandlerBase> create(const PublisherFbConfig config)
    {
        return std::make_unique<SingleHandler>(config.useSignalNames);
    }
};
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
