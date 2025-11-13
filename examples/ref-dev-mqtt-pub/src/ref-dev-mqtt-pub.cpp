#include "../../InputArgs.h"
#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;

struct ConfigStruct {
    std::string brokerAddress;
    bool exit = true;
    int error = 0;
};

ConfigStruct StartUp(int argc, char* argv[])
{
    ConfigStruct config;
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        config.error = 0;
        return config;
    }

    config.brokerAddress = args.getArgValue("--address", "127.0.0.1");
    config.exit = false;
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

    // Configure channels
    const auto channels = refDevice.getChannelsRecursive();
    channels[0].setPropertyValue("UseGlobalSampleRate", false);
    channels[0].setPropertyValue("SampleRate", 10);
    channels[0].setPropertyValue("Frequency", 1);
    channels[0].setPropertyValue("Waveform", 1);
    channels[1].setPropertyValue("UseGlobalSampleRate", false);
    channels[1].setPropertyValue("SampleRate", 20);
    channels[1].setPropertyValue("Frequency", 1);
    channels[1].setPropertyValue("Waveform", 3);
    channels[2].setPropertyValue("UseGlobalSampleRate", false);
    channels[2].setPropertyValue("SampleRate", 200);
    channels[2].setPropertyValue("Frequency", 4);

    // Create and configure MQTT server
    auto serverConfig = instance.getAvailableServerTypes().get("OpenDAQMQTT").createDefaultConfig();
    serverConfig.setPropertyValue("MqttBrokerAddress", appConfig.brokerAddress);
    const auto mqttServer = instance.addServer("OpenDAQMQTT", serverConfig);

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
