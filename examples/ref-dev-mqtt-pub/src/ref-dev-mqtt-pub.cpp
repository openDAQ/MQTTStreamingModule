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

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH)
                                     .setRootDevice("daqref://device0")
                                     .build();
    auto refDevice = instance.getRootDevice();
    refDevice.setPropertyValue("NumberOfChannels", 4);


    const auto channels = refDevice.getChannelsRecursive();
    channels[0].setPropertyValue("UseGlobalSampleRate", false);
    channels[0].setPropertyValue("SampleRate", 10);
    channels[0].setPropertyValue("Frequency", 1);
    channels[0].setPropertyValue("Waveform", 1);
    channels[1].setPropertyValue("UseGlobalSampleRate", false);
    channels[1].setPropertyValue("SampleRate", 20);
    channels[1].setPropertyValue("Frequency", 1);
    channels[1].setPropertyValue("Waveform", 3);
    channels[2].setPropertyValue("UseGlobalSampleRate", false);
    channels[2].setPropertyValue("SampleRate", 200);
    channels[2].setPropertyValue("Frequency", 4);

    auto serverConfig = instance.getAvailableServerTypes().get("OpenDAQMQTT").createDefaultConfig();
    serverConfig.setPropertyValue("MqttBrokerAddress", "127.0.0.1");
    const auto mqttServer = instance.addServer("OpenDAQMQTT", serverConfig);

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
