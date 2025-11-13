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
#include <mqtt_streaming_server_module/common.h>
#include <opendaq/device_ptr.h>
#include <opendaq/packet_reader_ptr.h>
#include <opendaq/stream_reader_ptr.h>
#include <opendaq/server.h>
#include <opendaq/server_impl.h>
#include <coretypes/intfs.h>
#include <opendaq/connection_internal.h>
#include <tsl/ordered_map.h>
#include <MqttAsyncClient.h>
#include <MqttSettings.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE


struct ChannelData {
    std::vector<double> data;
    std::vector<uint64_t> timestamps;
};

class MqttStreamingServerImpl : public daq::Server
{
public:
    explicit MqttStreamingServerImpl(const DevicePtr& rootDevice,
                                       const PropertyObjectPtr& config,
                                       const ContextPtr& context);
    ~MqttStreamingServerImpl() override;
    static PropertyObjectPtr createDefaultConfig(const ContextPtr& context);
    static ServerTypePtr createType(const ContextPtr& context);
    static PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& config, const ContextPtr& context);

protected:
    void onStopServer() override;
    void connectSignalReaders();
    bool isSignalCompatible(const SignalPtr& signal);

    void addReader(SignalPtr signalToRead);

    void stopServerInternal();

    void setupMqttPublisher();
    void sendData(const std::string& topic, const ChannelData& data, SizeT readAmount);
    std::vector<std::string> prepareJsonMessages(const ChannelData& data, SizeT dataAmount);
    std::string prepareJsonTopics();
    void sendTopicList();
    void readMqttSettings();

    void processingThreadFunc();
    void startProcessingThread();
    void stopProcessingThread();

    static void populateDefaultConfigFromProvider(const ContextPtr& context, const PropertyObjectPtr& config);

    daq::ListPtr<ISignal> signals;
    std::vector<StreamReaderPtr> streamReaders;

    std::string rootDeviceGlobalId;

    LoggerPtr logger;
    LoggerComponentPtr loggerComponent;

    std::atomic<bool> serverStopped;
    size_t maxPacketReadCount;
    std::chrono::milliseconds processingThreadSleepTime;
    mqtt::MqttAsyncClient publisher;
    Mqtt::Utils::Settings::MqttConnectionSettings connectionSettings;
    std::mutex readersSync;
    std::atomic<bool> processingThreadRunning;
    std::thread processingThread;
    std::atomic<bool> topicsAreSent = false;

    ChannelData buffer;
};

OPENDAQ_DECLARE_CLASS_FACTORY_WITH_INTERFACE(
    INTERNAL_FACTORY, MqttStreamingServer, daq::IServer,
    DevicePtr, rootDevice,
    PropertyObjectPtr, config,
    const ContextPtr&, context
)

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_SERVER_MODULE
