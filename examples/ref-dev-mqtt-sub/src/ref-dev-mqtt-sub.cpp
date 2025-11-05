#include <opendaq/opendaq.h>
#include "../../InputArgs.h"
#include <mqtt_streaming_client_module/constants.h>

#include <iostream>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

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
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true); // If you want to support --address for sub
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs()) {
        args.printHelp();
        return 0;
    }

    std::string brokerAddress = args.getArgValue("--address", "127.0.0.1");

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    auto brokerDevice = instance.addDevice("daq.mqtt://" + brokerAddress);
    auto availableDeviceNodes = brokerDevice.getAvailableFunctionBlockTypes();

    if (availableDeviceNodes.getCount() == 0) {
        std::cout << "No function block available from the device." << std::endl;
        return -1;
    }

    std::vector<daq::FunctionBlockPtr> fbList;
    for (const auto& [key, value] : availableDeviceNodes) {
        std::cout << "Available function block: " << key << std::endl;
        if (key == RAW_FB_NAME || key == JSON_FB_NAME)
            continue;
        fbList.push_back(brokerDevice.addFunctionBlock(key));
    }
    std::cout << "Try to connect the " << fbList[0].getLocalId() << std::endl;

    auto signals = fbList[0].getSignals();
    std::vector<PacketReaderPtr> readers;
    for (const auto& s : signals) {
        readers.emplace_back(daq::PacketReader(s));
    }

    std::thread readerThread([readers]() {
        while (true) {
            for (int iRdr = 0; iRdr < readers.size(); ++iRdr) {
                const auto& reader = readers[iRdr];
                while (!reader.getEmpty()) {
                    auto packet = reader.read();
                    if (packet.getType() == PacketType::Event)
                    {
                        std::cout << "Event packet is skipped!" << std::endl;
                        continue;
                    }

                    if (packet.getType() == PacketType::Data)
                    {
                        const auto dataPacket = packet.asPtrOrNull<IDataPacket>();
                        std::cout << "READER #" << iRdr << " - " << to_string(dataPacket) << std::endl;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    readerThread.detach();

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
