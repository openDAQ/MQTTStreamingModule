#include <opendaq/opendaq.h>

#include <iostream>

using namespace daq;
namespace
{
class InputArgs
{
public:
    void addArg(const std::string& name, const std::string& description)
    {
        argDescriptions[name] = description;
    }

    void parse(int argc, char* argv[])
    {
        parsedArgs.clear();
        for (int i = 1; i < argc; ++i)
            parsedArgs.push_back(argv[i]);
    }

    bool hasArg(const std::string& name) const
    {
        return std::find(parsedArgs.begin(), parsedArgs.end(), name) != parsedArgs.end();
    }

    bool hasUnknownArgs() const
    {
        for (const auto& arg : parsedArgs) {
            if (argDescriptions.find(arg) == argDescriptions.end())
                return true;
        }
        return false;
    }

    void printHelp() const
    {
        std::cout << "Available arguments:" << std::endl;
        for (const auto& [name, desc] : argDescriptions)
            std::cout << "  " << name << " : " << desc << std::endl;
    }

private:
    std::unordered_map<std::string, std::string> argDescriptions;
    std::vector<std::string> parsedArgs;
};

} // end of namespace

int main(int argc, char* argv[])
{
    InputArgs args;
    args.addArg("--help", "Show help message");
    args.parse(argc, argv);

    if (args.hasArg("--help") || args.hasUnknownArgs()) {
        args.printHelp();
        return 0;
    }

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH).build();
    auto brokerDevice = instance.addDevice("daq.mqtt://127.0.0.1");
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
