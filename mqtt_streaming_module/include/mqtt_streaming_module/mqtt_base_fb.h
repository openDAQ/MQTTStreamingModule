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
#include <opendaq/function_block_impl.h>

#include "MqttAsyncClient.h"
#include "mqtt_streaming_module/constants.h"
#include "mqtt_streaming_module/status_helper.h"

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
    
class MqttBaseFb : public FunctionBlock
{
public:
    enum class SubscriptionStatus : EnumType
    {
        InvalidTopicName = 0,
        SubscribingError,
        WaitingForData,
        HasData
    };

    struct CmdResult
    {
        bool success = false;
        std::string msg;
        int token = 0;

        CmdResult(bool success = false, const std::string& msg = "", int token = 0)
            : success(success),
              msg(msg),
              token(token)
        {
        }
    };


    explicit DAQ_MQTT_STREAM_MODULE_API MqttBaseFb(const ContextPtr& ctx,
                                const ComponentPtr& parent,
                                const FunctionBlockTypePtr& type,
                                const StringPtr& localId,
                                std::shared_ptr<mqtt::MqttAsyncClient> subscriber,
                                const PropertyObjectPtr& config = nullptr);
    virtual ~MqttBaseFb() = default;

    virtual std::string getSubscribedTopic() const = 0;

protected:
    static std::vector<std::pair<SubscriptionStatus, std::string>> subscriptionStatusMap;

    std::shared_ptr<mqtt::MqttAsyncClient> subscriber;
    StatusHelper<SubscriptionStatus> subscriptionStatus;
    int qos = DEFAULT_SUB_QOS;

    virtual void processMessage(const mqtt::MqttMessage& msg) = 0;

    void initProperties(const PropertyObjectPtr& config);
    virtual void readProperties() = 0;

    DAQ_MQTT_STREAM_MODULE_API void onSignalsMessage(const mqtt::MqttAsyncClient& subscriber, const mqtt::MqttMessage& msg);

    virtual void clearSubscribedTopic() = 0;
    CmdResult subscribeToTopic();
    CmdResult unsubscribeFromTopic();

    virtual void propertyChanged() = 0;

    void removed() override;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
