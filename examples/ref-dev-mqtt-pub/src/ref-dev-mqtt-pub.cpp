#include "../../InputArgs.h"
#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;

int main(int argc, char* argv[])
{
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        return 0;
    }

    std::string brokerAddress = args.getArgValue("--address", "127.0.0.1");

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).setRootDevice("daqref://device0").build();
    auto refDevice = instance.getRootDevice();
    refDevice.setPropertyValue("NumberOfChannels", 4);

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

    auto serverConfig = instance.getAvailableServerTypes().get("OpenDAQMQTT").createDefaultConfig();
    serverConfig.setPropertyValue("MqttBrokerAddress", brokerAddress);
    const auto mqttServer = instance.addServer("OpenDAQMQTT", serverConfig);

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
