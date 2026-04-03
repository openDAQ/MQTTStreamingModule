#include "../../InputArgs.h"
#include <opendaq/opendaq.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <thread>

#include <iostream>

using namespace daq;

struct ConfigStruct {
    std::string brokerAddress;
    std::string topic;
    bool exit = true;
    int error = 0;
};

ConfigStruct StartUp(int argc, char* argv[])
{
    ConfigStruct config;
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.setUsageHelp(APP_NAME " [options] <topic>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        config.error = 0;
        return config;
    }

    config.brokerAddress = args.getArgValue("--address", "127.0.0.1");
    const auto positionalArgs = args.getPositionalArgs();
    if (positionalArgs.empty())
    {
        std::cout << "An MQTT topic is required." << std::endl;
        config.error = -1;
        return config;
    }
    config.topic = args.getPositionalArgs()[0];;

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

    // Create OpenDAQ instance and add MQTT broker FB
    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    const std::string clientFbName = "MQTTClientFB";
    auto clientFbConfig = instance.getAvailableFunctionBlockTypes().get(clientFbName).createDefaultConfig();
    clientFbConfig.setPropertyValue("BrokerAddress", appConfig.brokerAddress);
    auto brokerFB = instance.addFunctionBlock(clientFbName, clientFbConfig);
    auto availableFbs = brokerFB.getAvailableFunctionBlockTypes();

    const std::string fbName = "MQTTSubscriberFB";
    std::cout << "Try to add the " << fbName << std::endl;

    // Create subscriber function block configuration
    auto config = availableFbs.get(fbName).createDefaultConfig();
    config.setPropertyValue("Topic", appConfig.topic);
    config.setPropertyValue("EnablePreviewSignal", True);
    config.setPropertyValue("MessageIsString", True);

    // Add the subscriber function block to the broker FB
    daq::FunctionBlockPtr subFb = brokerFB.addFunctionBlock(fbName, config);

    // Create packet readers for a signal
    const auto signal = subFb.getSignals()[0];
    PacketReaderPtr reader = daq::PacketReader(signal);

    // Start a thread to read packets from the reader
    std::atomic<bool> running = true;
    std::thread readerThread(
        [&reader, &signal, &running]()
        {
            while (running)
            {
                while (!reader.getEmpty() && running)
                {
                    auto packet = reader.read();
                    if (packet.getType() == PacketType::Event)
                    {
                        continue;
                    }
                    else if (packet.getType() == PacketType::Data)
                    {
                        const auto dataPacket = packet.asPtr<IDataPacket>();
                        std::string dataStr(static_cast<char*>(dataPacket.getData()), dataPacket.getDataSize());
                        std::cout << signal.getName() << " - " << dataStr << std::endl;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();

    running = false;
    readerThread.join();
    std::cout << "Reader thread finished. Exiting.\n";
    return 0;
}
