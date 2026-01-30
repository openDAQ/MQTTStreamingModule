#include "../../InputArgs.h"
#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;

enum class Mode {
    TopicPerSignal = 0,
    SingleTopic,
    _COUNT
};

struct ConfigStruct {
    std::string brokerAddress;
    Mode mode;
    bool useArray = false;
    size_t arraySize = 0;
    bool exit = true;
    int error = 0;
};

ConfigStruct StartUp(int argc, char* argv[])
{
    ConfigStruct config;
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.addArg("--mode", "publisher FB mode", true);
    args.addArg("--array", "pablish samples as arrays with specified size", true);
    args.setUsageHelp(APP_NAME " [options]\n"
                              "Available modes:\n"
                              "  0 - Topic per signal\n"
                              "  1 - Single topic\n");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        config.error = 0;
        return config;
    }

    config.brokerAddress = args.getArgValue("--address", "127.0.0.1");
    config.exit = false;
    int mode = std::stoi(args.getArgValue("--mode", "0"));
    if (mode < 0 || mode >= static_cast<int>(Mode::_COUNT))
    {
        std::cout << "Invalid mode value. Allowed values are from 0 to " << (static_cast<int>(Mode::_COUNT) - 1) << "." << std::endl;
        args.printHelp();
        config.error = -1;
        config.exit = true;
        return config;
    }
    config.mode = static_cast<Mode>(mode);
    if (args.hasArg("--array"))
    {
        config.useArray = true;
        config.arraySize = std::stoi(args.getArgValue("--array", "0"));
        if (config.arraySize == 0)
        {
            std::cout << "Invalid array size value. It must be greater than 0." << std::endl;
            args.printHelp();
            config.error = -1;
            config.exit = true;
            return config;
        }
    }
    return config;
}

int main(int argc, char* argv[])
{
    // Parse input arguments
    auto appConfig = StartUp(argc, argv);
    if (appConfig.exit)
    {
        return appConfig.error;
    }

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).setRootDevice("daqref://device0").build();
    auto refDevice = instance.getRootDevice();
    refDevice.setPropertyValue("NumberOfChannels", 4);
    refDevice.setPropertyValue("GlobalSampleRate", 100);

    // Configure channels
    const auto channels = refDevice.getChannelsRecursive();
    channels[0].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::SingleTopic);
    channels[0].setPropertyValue("SampleRate", 10);
    channels[0].setPropertyValue("Frequency", 1);
    channels[0].setPropertyValue("Waveform", 1);
    channels[1].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::SingleTopic);
    channels[1].setPropertyValue("SampleRate", 20);
    channels[1].setPropertyValue("Frequency", 1);
    channels[1].setPropertyValue("Waveform", 3);
    channels[2].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::SingleTopic);
    channels[2].setPropertyValue("SampleRate", 50);
    channels[2].setPropertyValue("Frequency", 4);
    channels[3].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::SingleTopic);
    channels[3].setPropertyValue("SampleRate", 100);
    channels[3].setPropertyValue("Frequency", 20);

    // Create and configure MQTT server
    const std::string clientFbName = "MQTTClientFB";
    auto clientFbConfig = instance.getAvailableFunctionBlockTypes().get(clientFbName).createDefaultConfig();
    clientFbConfig.setPropertyValue("BrokerAddress", appConfig.brokerAddress);
    auto brokerFB = instance.addFunctionBlock(clientFbName, clientFbConfig);
    auto availableFbs = brokerFB.getAvailableFunctionBlockTypes();
    const std::string fbName = "MQTTJSONPublisherFB";
    std::cout << "Try to add the " << fbName << std::endl;

    auto config = availableFbs.get(fbName).createDefaultConfig();
    config.setPropertyValue("QoS", 1);
    config.setPropertyValue("ReaderWaitPeriod", 20);
    config.setPropertyValue("SignalValueJSONKey", 2);
    config.setPropertyValue("TopicMode", (appConfig.mode == Mode::TopicPerSignal) ? 0 : 1);
    config.setPropertyValue("GroupValues", (appConfig.useArray) ? True : False);
    config.setPropertyValue("SamplesPerMessage", appConfig.arraySize);
    config.setPropertyValue("Topic", "opendaq/test/values");


    // Add the publisher function block to the broker device
    daq::FunctionBlockPtr fb = brokerFB.addFunctionBlock(fbName, config);
    const auto signals = refDevice.getSignals(search::Recursive(search::Any()));
    for (const auto& s : signals)
    {
        if (s.getDomainSignal().assigned())
        {
            auto ports = fb.getInputPorts();
            ports[ports.getCount() - 1].connect(s);
        }
    }

    auto status = fb.getStatusContainer().getStatus("ComponentStatus");
    const auto statusStr = status.getValue();
    if (statusStr != "Ok")
        return -1;
    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
