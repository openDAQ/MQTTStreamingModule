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

#include "mqtt_streaming_module/types.h"
#include <mqtt_streaming_module/atomic_signal_atomic_sample_handler.h>
#include <mqtt_streaming_module/atomic_signal_sample_arr_handler.h>
#include <mqtt_streaming_module/group_signal_shared_ts_handler.h>
#include <mqtt_streaming_module/signal_arr_atomic_sample_handler.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class HandlerFactory
{
public:
    static std::unique_ptr<HandlerBase>
    create(WeakRefPtr<IFunctionBlock> parentFb, const PublisherFbConfig config, const std::string& publisherFbGlobalId)
    {
        if (config.topicMode == TopicMode::Single)
        {
            return std::make_unique<GroupSignalSharedTsHandler>(parentFb,
                                                                config.valueFieldName,
                                                                config.topicName.empty() ? publisherFbGlobalId : config.topicName);
        }
        else if (config.topicMode == TopicMode::PerSignal)
        {
            if (config.groupValues)
                return std::make_unique<AtomicSignalSampleArrayHandler>(parentFb, config.valueFieldName, config.groupValuesPackSize);
            else
                return std::make_unique<AtomicSignalAtomicSampleHandler>(parentFb, config.valueFieldName);
        }

        return std::make_unique<AtomicSignalAtomicSampleHandler>(parentFb, config.valueFieldName);
    }
};
END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
