#include "../../InputArgs.h"
#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;

enum class Mode {
    ATOMIC_SIGNAL_ATOMIC_SAMPLE = 0,
    ATOMIC_SIGNAL_SAMPLE_ARRAY,
    SIGNAL_ARRAY_ATOMIC_SAMPLE,
    GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS,
    _COUNT
};

struct ConfigStruct {
    std::string brokerAddress;
    Mode mode;
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
    args.setUsageHelp(APP_NAME " [options]\n"
                              "Available modes:\n"
                              "  0 - ATOMIC_SIGNAL_ATOMIC_SAMPLE\n"
                              "  1 - ATOMIC_SIGNAL_SAMPLE_ARRAY\n"
                              "  2 - SIGNAL_ARRAY_ATOMIC_SAMPLE\n"
                              "  3 - GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS");
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
    channels[0].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS);
    channels[0].setPropertyValue("SampleRate", 10);
    channels[0].setPropertyValue("Frequency", 1);
    channels[0].setPropertyValue("Waveform", 1);
    channels[1].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS);
    channels[1].setPropertyValue("SampleRate", 20);
    channels[1].setPropertyValue("Frequency", 1);
    channels[1].setPropertyValue("Waveform", 3);
    channels[2].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS);
    channels[2].setPropertyValue("SampleRate", 50);
    channels[2].setPropertyValue("Frequency", 4);
    channels[3].setPropertyValue("UseGlobalSampleRate", appConfig.mode == Mode::GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS);
    channels[3].setPropertyValue("SampleRate", 100);
    channels[3].setPropertyValue("Frequency", 20);

    // Create and configure MQTT server
    const std::string rootFbName = "rootMqttFb";
    auto rootFbConfig = instance.getAvailableFunctionBlockTypes().get(rootFbName).createDefaultConfig();
    rootFbConfig.setPropertyValue("MqttBrokerAddress", appConfig.brokerAddress);
    auto brokerFB = instance.addFunctionBlock(rootFbName, rootFbConfig);
    auto availableFbs = brokerFB.getAvailableFunctionBlockTypes();
    const std::string fbName = "publisherMqttFb";
    std::cout << "Try to add the " << fbName << std::endl;

    auto config = availableFbs.get(fbName).createDefaultConfig();
    config.setPropertyValue("MqttQoS", 1);
    config.setPropertyValue("ReaderPeriod", 20);
    config.setPropertyValue("UseSignalNames", True);
    switch (appConfig.mode) {
        case Mode::ATOMIC_SIGNAL_ATOMIC_SAMPLE:
            config.setPropertyValue("SharedTimestamp", False);
            config.setPropertyValue("TopicMode", 0);
            config.setPropertyValue("GroupValues", False);
            break;
        case Mode::ATOMIC_SIGNAL_SAMPLE_ARRAY:
            config.setPropertyValue("SharedTimestamp", False);
            config.setPropertyValue("TopicMode", 0);
            config.setPropertyValue("GroupValues", True);
            config.setPropertyValue("GroupValuesPackSize", 3);
            break;
        case Mode::SIGNAL_ARRAY_ATOMIC_SAMPLE:
            config.setPropertyValue("SharedTimestamp", False);
            config.setPropertyValue("TopicMode", 1);
            config.setPropertyValue("GroupValues", False);
            break;
        case Mode::GROUP_SIGNAL_ATOMIC_SAMPLE_SHARED_TS:
            config.setPropertyValue("SharedTimestamp", True);
            config.setPropertyValue("TopicMode", 1);
            config.setPropertyValue("GroupValues", False);
            break;
        default:
            break;
    }


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
