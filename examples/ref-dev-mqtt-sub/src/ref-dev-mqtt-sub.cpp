#include <opendaq/opendaq.h>
#include "../../InputArgs.h"

#include <iostream>

using namespace daq;

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
        fbList.push_back(brokerDevice.addFunctionBlock(key));
    }
    std::cout << "Try to connect the first one (" << fbList[0].getLocalId() << ")" << std::endl;

    auto signals = fbList[0].getSignals();
    std::vector<StreamReaderPtr> readers;
    for (const auto& s : signals) {
        readers.emplace_back(daq::StreamReader<double, uint64_t>(s, ReadTimeoutType::Any));
    }

    std::thread t1([readers]() {
        constexpr int size = 1000;
        double samples[size];
        uint64_t timestamps[size];
        while (true) {
            for (int iRdr = 0; iRdr < readers.size(); ++iRdr) {
                const auto& reader = readers[iRdr];
                while (!reader.getEmpty()) {
                    daq::SizeT count = size;
                    reader.readWithDomain(samples, timestamps, &count);
                    for (daq::SizeT i = 0; i < count; ++i)
                        std::cout << "READER #" << iRdr << " - Sample: " << samples[i] << " Timestamp: " << timestamps[i] << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    t1.detach();

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
