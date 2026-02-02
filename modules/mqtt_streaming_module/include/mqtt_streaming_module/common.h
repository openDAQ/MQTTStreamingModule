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
#include <coretypes/common.h>

#define BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE BEGIN_NAMESPACE_OPENDAQ_MODULE(mqtt_streaming_module)
#define END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE END_NAMESPACE_OPENDAQ_MODULE

#if !defined(OPENDAQ_MQTT_ENABLE_TESTS)
    #define DAQ_MQTT_STREAM_MODULE_API
#else
    #if defined(_WIN32)
        #if defined(OPENDAQ_MODULE_DLL_IMPORT)
            #define DAQ_MQTT_STREAM_MODULE_API __declspec(dllimport)
        #else
            #define DAQ_MQTT_STREAM_MODULE_API __declspec(dllexport)
        #endif
    #else
        #define DAQ_MQTT_STREAM_MODULE_API __attribute__((visibility("default")))
    #endif
#endif
