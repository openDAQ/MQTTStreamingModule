#include "../../InputArgs.h"
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/opendaq.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <iostream>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

int main(int argc, char* argv[])
{
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.setUsageHelp(APP_NAME " [options] <topic1> <topic2> ... <topicN>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        return 0;
    }

    std::string brokerAddress = args.getArgValue("--address", "127.0.0.1");
    auto topics = args.getPositionalArgs();

    if (topics.empty())
    {
        std::cout << "MQTT topics are required." << std::endl;
        return -1;
    }
    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    auto brokerDevice = instance.addDevice("daq.mqtt://" + brokerAddress);
    auto availableDeviceNodes = brokerDevice.getAvailableFunctionBlockTypes();

    if (availableDeviceNodes.getCount() == 0)
    {
        std::cout << "No function block available from the device." << std::endl;
        return -1;
    }

    for (const auto& [key, value] : availableDeviceNodes)
    {
        std::cout << "Available function block: " << key << std::endl;
    }
    const std::string fbName = RAW_FB_NAME;
    std::cout << "Try to add the " << fbName << std::endl;

    auto config = availableDeviceNodes.get(fbName).createDefaultConfig();
    auto topicList = List<IString>();
    for (auto& topic : topics)
    {
        addToList(topicList, std::move(topic));
    }
    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, topicList);
    daq::FunctionBlockPtr rawFb = brokerDevice.addFunctionBlock(fbName, config);

    auto signals = rawFb.getSignals();
    std::map<std::string, PacketReaderPtr> readers;
    for (const auto& s : signals)
    {
        readers.emplace(std::pair<std::string, PacketReaderPtr>(s.getName().toStdString(), daq::PacketReader(s)));
    }

    std::atomic<bool> running = true;
    std::thread readerThread(
        [&readers, &running]()
        {
            while (running)
            {
                for (const auto& pair : readers)
                {
                    const auto& reader = pair.second;
                    while (!reader.getEmpty() && running)
                    {
                        auto packet = reader.read();
                        const auto eventPacket = packet.asPtrOrNull<IEventPacket>();
                        if (eventPacket.assigned())
                        {
                            continue;
                        }
                        const auto dataPacket = packet.asPtrOrNull<IDataPacket>();
                        if (dataPacket.assigned())
                        {
                            std::string tmp(static_cast<char*>(dataPacket.getData()), dataPacket.getDataSize());
                            std::cout << pair.first << " - " << tmp << std::endl;
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
