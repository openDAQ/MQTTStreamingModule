#include <opendaq/opendaq.h>
#include "../../InputArgs.h"
#include <mqtt_streaming_client_module/constants.h>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

std::string readFileToString(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath);

    std::ostringstream buffer;
    buffer << file.rdbuf();  // Read the entire file buffer
    return buffer.str();
}

int main(int argc, char* argv[])
{
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true); // If you want to support --address for sub
    args.setUsageHelp(APP_NAME " [options] <config file>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs()) {
        args.printHelp();
        return 0;
    }

    std::string brokerAddress = args.getArgValue("--address", "127.0.0.1");
    auto configFilePath = args.getPositionalArgs();
    if (configFilePath.size() != 1) {
        std::cout << "Configuration file path is required." << std::endl;
        return -1;
    }

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    auto brokerDevice = instance.addDevice("daq.mqtt://" + brokerAddress);
    auto availableDeviceNodes = brokerDevice.getAvailableFunctionBlockTypes();

    if (availableDeviceNodes.getCount() == 0) {
        std::cout << "No function block available from the device." << std::endl;
        return -1;
    }

    for (const auto& [key, value] : availableDeviceNodes) {
        std::cout << "Available function block: " << key << std::endl;
    }
    const std::string fbName = JSON_FB_NAME;
    std::cout << "Try to add the " << fbName << std::endl;

    auto config = availableDeviceNodes.get(fbName).createDefaultConfig();
    std::string jsonConfig = readFileToString(configFilePath[0]);

    config.setPropertyValue(PROPERTY_NAME_SIGNAL_LIST, jsonConfig);
    daq::FunctionBlockPtr jsonFb = brokerDevice.addFunctionBlock(fbName, config);


    auto signals = jsonFb.getSignals();

    using ReaderContainerEntryType = std::pair<daq::GenericSignalPtr<>, StreamReaderPtr>;
    using ReaderContainerType = std::vector<ReaderContainerEntryType>;
    ReaderContainerType readers;
    for (const auto& s : signals)
    {
        readers.emplace_back(ReaderContainerEntryType(
            std::pair(s, daq::StreamReader<double, uint64_t>(s, ReadTimeoutType::Any))));
    }

    std::thread readerThread([readers]() {
        constexpr int size = 1000;

        std::vector<double> samplesVec(size);
        std::vector<uint64_t> timestampsVec(size);
        auto samples = samplesVec.data();
        auto timestamps = timestampsVec.data();

        auto readWithDomain = [samples, timestamps](const daq::GenericSignalPtr<> signal, const daq::StreamReaderPtr reader)
        {
            daq::SizeT count = size;
            reader.readWithDomain(samples, timestamps, &count);
            const std::string sampleUnit = (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned()) ?
                                               " " + signal.getDescriptor().getUnit().getSymbol().toStdString() : "";
            for (daq::SizeT i = 0; i < count; ++i)
                std::cout << signal.getName() << " - Sample: " << samples[i] << sampleUnit << " Timestamp: " << timestamps[i] << std::endl;
        };

        auto read = [samples](const daq::GenericSignalPtr<> signal, const daq::StreamReaderPtr reader)
        {
            daq::SizeT count = size;
            reader.read(samples, &count);
            const std::string sampleUnit = (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned()) ?
                                               " " + signal.getDescriptor().getUnit().getSymbol().toStdString() : "";
            for (daq::SizeT i = 0; i < count; ++i)
                std::cout << signal.getName() << " - Sample: " << samples[i] << sampleUnit << std::endl;
        };

        while (true) {
            for (const auto& [signal, reader] : readers) {
                while (!reader.getEmpty()) {
                    if (signal.getDomainSignal().assigned())
                        readWithDomain(signal, reader);
                    else
                        read(signal, reader);

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
