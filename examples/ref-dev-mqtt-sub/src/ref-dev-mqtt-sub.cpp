#include "../../InputArgs.h"
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

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

std::string to_string(daq::DataPacketPtr packet)
{
    std::string result;
    std::string data;
    switch (packet.getDataDescriptor().getSampleType())
    {
    case SampleType::Float64:
        data = std::to_string(*(static_cast<double*>(packet.getData())));
        break;
    case SampleType::UInt64:
        data = std::to_string(*(static_cast<uint64_t*>(packet.getData())));
        break;
    case SampleType::Int64:
        data = std::to_string(*(static_cast<int64_t*>(packet.getData())));
        break;
    case SampleType::Binary:
        data = '\"' + std::string(static_cast<char*>(packet.getData()), packet.getDataSize()) + '\"';
        break;
    default:
        break;
    }
    result = fmt::format("Data: {};", data);
    if (packet.getDomainPacket().assigned())
    {
        uint64_t ts = *(static_cast<uint64_t*>(packet.getDomainPacket().getData()));
        result += fmt::format(" Timestamp : {};", ts);
    }
    return result;
}

int main(int argc, char* argv[])
{
    // Parse input arguments
    auto appConfig = StartUp(argc, argv);
    if (appConfig.exit)
    {
        return appConfig.error;
    }

    // Create OpenDAQ instance and add MQTT broker device
    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    auto brokerDevice = instance.addDevice("daq.mqtt://" + appConfig.brokerAddress);
    auto availableDeviceNodes = brokerDevice.getAvailableFunctionBlockTypes();

    // Check available function blocks, skip RAW and JSON FBs
    if (availableDeviceNodes.getCount() <= 2)
    {
        std::cout << "No function block available from the device." << std::endl;
        return -1;
    }

    std::vector<daq::FunctionBlockPtr> fbList;
    std::cout << "Available function blocks: " << std::endl;
    for (const auto& [key, value] : availableDeviceNodes)
    {
        std::cout << " - " << key << std::endl;
        if (key == RAW_FB_NAME || key == JSON_FB_NAME)
            continue;
        fbList.push_back(brokerDevice.addFunctionBlock(key));
    }

    std::cout << "Try to connect the " << fbList[0].getLocalId() << std::endl;

    // Create packet readers for all signals
    auto signals = fbList[0].getSignals();
    std::vector<PacketReaderPtr> readers;
    for (const auto& s : signals)
    {
        readers.emplace_back(daq::PacketReader(s));
    }

    // Start a thread to read packets from the readers
    std::atomic<bool> running = true;
    std::thread readerThread(
        [&readers, &running]()
        {
            while (running)
            {
                for (int iRdr = 0; iRdr < readers.size(); ++iRdr)
                {
                    const auto& reader = readers[iRdr];
                    while (!reader.getEmpty() && running)
                    {
                        auto packet = reader.read();
                        if (packet.getType() == PacketType::Event)
                        {
                            std::cout << "Event packet is skipped!" << std::endl;
                        }
                        else if (packet.getType() == PacketType::Data)
                        {
                            const auto dataPacket = packet.asPtrOrNull<IDataPacket>();
                            std::cout << "READER #" << iRdr << " - " << to_string(dataPacket) << std::endl;
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
