#include "../../InputArgs.h"
#include <iomanip>
#include <mqtt_streaming_client_module/constants.h>
#include <opendaq/opendaq.h>

#include <fstream>
#include <iostream>
#include <sstream>

using namespace daq;
using namespace daq::modules::mqtt_streaming_client_module;

std::string to_string(SampleType sampleType)
{
    switch (sampleType)
    {
    case SampleType::Float32:
        return "Float32";
    case SampleType::Float64:
        return "Float64";
    case SampleType::UInt8:
        return "UInt8";
    case SampleType::Int8:
        return "Int8";
    case SampleType::UInt16:
        return "UInt16";
    case SampleType::Int16:
        return "Int16";
    case SampleType::UInt32:
        return "UInt32";
    case SampleType::Int32:
        return "Int32";
    case SampleType::UInt64:
        return "UInt64";
    case SampleType::Int64:
        return "Int64";
    case SampleType::RangeInt64:
        return "RangeInt64";
    case SampleType::ComplexFloat32:
        return "ComplexFloat32";
    case SampleType::ComplexFloat64:
        return "ComplexFloat64";
    case SampleType::Struct:
        return "Struct";
    case SampleType::Undefined:
        return "Undefined";
    case SampleType::Binary:
        return "Binary";
    case SampleType::String:
        return "String";
    case SampleType::Null:
        return "Null";
    case SampleType::_count:
        return "Count";
    }
    return "Unknown";
}

std::string to_string(uint64_t ts)
{
    using namespace std::chrono;

    system_clock::time_point tp = system_clock::time_point(microseconds(ts));

    auto tt = system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&tt);

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
    result = fmt::format("SampleType : {}; Data: {};", to_string(packet.getDataDescriptor().getSampleType()), data);
    if (packet.getDomainPacket().assigned())
    {
        uint64_t ts = *(static_cast<uint64_t*>(packet.getDomainPacket().getData()));
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

int main(int argc, char* argv[])
{
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.addArg("--address", "MQTT broker address", true); // If you want to support --address for sub
    args.setUsageHelp(APP_NAME " [options] <config file>");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs())
    {
        args.printHelp();
        return 0;
    }

    std::string brokerAddress = args.getArgValue("--address", "127.0.0.1");
    auto configFilePath = args.getPositionalArgs();
    if (configFilePath.size() != 1)
    {
        std::cout << "Configuration file path is required." << std::endl;
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
        readers.emplace_back(ReaderContainerEntryType(std::pair(s, daq::StreamReader<double, uint64_t>(s, ReadTimeoutType::Any))));
    }

    std::map<std::string, PacketReaderPtr> packetReaders;
    for (const auto& s : signals)
    {
        packetReaders.emplace(std::pair<std::string, PacketReaderPtr>(s.getName().toStdString(), daq::PacketReader(s)));
    }

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
                        const auto eventPacket = packet.asPtrOrNull<IEventPacket>();
                        if (eventPacket.assigned())
                        {
                            std::cout << "Event packet is skipped!" << std::endl;
                            continue;
                        }
                        const auto dataPacket = packet.asPtrOrNull<IDataPacket>();
                        if (dataPacket.assigned())
                        {
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
