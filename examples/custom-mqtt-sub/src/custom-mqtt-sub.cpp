#include "../../InputArgs.h"
#include <iomanip>
#include <opendaq/opendaq.h>

#include <fstream>
#include <iostream>
#include <sstream>

using namespace daq;

struct ConfigStruct {
    std::string brokerAddress;
    std::string configFilePath;
    bool exit = true;
    int error = 0;
};

std::string to_string(uint64_t ts)
{
    using namespace std::chrono;

    system_clock::time_point tp = system_clock::time_point(microseconds(ts));

    auto tt = system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&tt);

    auto us = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << us.count();
    return oss.str();
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
    std::string unitStr;
    if (auto unit = packet.getDataDescriptor().getUnit(); unit.assigned())
    {
        if (auto s = unit.getSymbol(); s.assigned())
            unitStr = " " + s.toStdString();
    }

    result = fmt::format("SampleType : {}; Data: {}{};", convertSampleTypeToString(packet.getDataDescriptor().getSampleType()), data, unitStr);
    if (auto domainPacket = packet.getDomainPacket(); domainPacket.assigned())
    {
        uint64_t ts = *(static_cast<uint64_t*>(domainPacket.getData()));
        result += fmt::format(" Time : {};", to_string(ts));
    }
    return result;
}

std::string readFileToString(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath);

    std::ostringstream buffer;
    buffer << file.rdbuf(); // Read the entire file buffer
    return buffer.str();
}

ConfigStruct StartUp(int argc, char* argv[])
{
    ConfigStruct config;
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true);
    args.setUsageHelp(APP_NAME " [options] <config file>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        config.error = 0;
        return config;
    }

    config.brokerAddress = args.getArgValue("--address", "127.0.0.1");
    auto configFilePath = args.getPositionalArgs();
    if (configFilePath.size() != 1)
    {
        std::cout << "Configuration file path is required." << std::endl;
        config.error = -1;
        return config;
    }
    if (configFilePath.size() > 1)
    {
        std::cout << "Only one configuration file path is allowed. The first one will be used - " << configFilePath[0] << std::endl;
    }
    config.configFilePath = std::move(configFilePath[0]);
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

    const std::string jsonFbName = "@jsonMqttFb";
    std::cout << "Try to add the " << jsonFbName << std::endl;

    auto config = availableFbs.get(jsonFbName).createDefaultConfig();
    config.setPropertyValue("JsonConfigFile", appConfig.configFilePath);

    // Add the JSON function block to the broker FB
    daq::FunctionBlockPtr jsonFb = brokerFB.addFunctionBlock(jsonFbName, config);

    // Create packet readers for all signals
    auto signals = List<daq::ISignal>();
    const auto fbs = jsonFb.getFunctionBlocks();
    for (const auto& fb : fbs)
    {
        const auto sig = fb.getSignals();
        for (const auto& s : sig)
        {
            signals.pushBack(s);
        }
    }

    std::vector<std::pair<std::string, PacketReaderPtr>> packetReaders;
    for (const auto& s : signals)
    {
        packetReaders.push_back(std::pair<std::string, PacketReaderPtr>(s.getName().toStdString(), daq::PacketReader(s)));
    }

    // Start a thread to read packets from the readers
    std::atomic<bool> running = true;
    std::thread readerThread(
        [&packetReaders, &running]()
        {
            while (running)
            {
                for (const auto& [signal, reader] : packetReaders)
                {
                    while (!reader.getEmpty() && running)
                    {
                        auto packet = reader.read();
                        if (packet.getType() == PacketType::Event)
                        {
                            std::cout << "Event packet is skipped!" << std::endl;
                        }
                        else if (packet.getType() == PacketType::Data)
                        {
                            const auto dataPacket = packet.asPtr<IDataPacket>();
                            std::cout << signal << " - " << to_string(dataPacket) << std::endl;
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
