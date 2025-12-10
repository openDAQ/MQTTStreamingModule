#include "../../InputArgs.h"
#include <opendaq/opendaq.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <iostream>

using namespace daq;

struct ConfigStruct {
    std::string brokerAddress;
    std::vector<std::string> topics;
    bool exit = true;
    int error = 0;
};

ConfigStruct StartUp(int argc, char* argv[])
{
    ConfigStruct config;
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.setUsageHelp(APP_NAME " [options] <topic1> <topic2> ... <topicN>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        config.error = 0;
        return config;
    }

    config.brokerAddress = args.getArgValue("--address", "127.0.0.1");
    config.topics = args.getPositionalArgs();

    if (config.topics.empty())
    {
        std::cout << "MQTT topics are required." << std::endl;
        config.error = -1;
        return config;
    }

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
    const std::string rootFbName = "@rootMqttFb";
    auto rootFbConfig = instance.getAvailableFunctionBlockTypes().get(rootFbName).createDefaultConfig();
    rootFbConfig.setPropertyValue("MqttBrokerAddress", appConfig.brokerAddress);
    auto brokerFB = instance.addFunctionBlock(rootFbName, rootFbConfig);
    auto availableFbs = brokerFB.getAvailableFunctionBlockTypes();

    const std::string fbName = "@rawMqttFb";
    std::cout << "Try to add the " << fbName << std::endl;

    // Create RAW function block configuration
    auto config = availableFbs.get(fbName).createDefaultConfig();
    auto topicList = List<IString>();
    for (auto& topic : appConfig.topics)
    {
        addToList(topicList, std::move(topic));
    }
    config.setPropertyValue("SignalList", topicList);

    // Add the RAW function block to the broker FB
    daq::FunctionBlockPtr rawFb = brokerFB.addFunctionBlock(fbName, config);

    // Create packet readers for all signals
    const auto signals = rawFb.getSignals();
    std::map<std::string, PacketReaderPtr> readers;
    for (const auto& s : signals)
    {
        readers.emplace(std::pair<std::string, PacketReaderPtr>(s.getName().toStdString(), daq::PacketReader(s)));
    }

    // Start a thread to read packets from the readers
    std::atomic<bool> running = true;
    std::thread readerThread(
        [&readers, &running]()
        {
            while (running)
            {
                for (const auto& [signalName, reader] : readers)
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
                            std::cout << signalName << " - " << dataStr << std::endl;
                        }
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
