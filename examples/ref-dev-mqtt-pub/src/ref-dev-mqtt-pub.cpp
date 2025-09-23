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

    using namespace std::chrono_literals;
    StringPtr loggerPath = "ref_device_simulator.log";

    auto users = List<IUser>();
    users.pushBack(User("opendaq", "$2b$10$bqZWNEd.g1R1Q1inChdAiuDr5lbal33bBNOehlCwuWcxRH5weF3hu")); // password: opendaq
    users.pushBack(User("root", "$2b$10$k/Tj3yqFV7uQz42UCJK2n.4ECd.ySQ2Sfd81Kx.xfuMOeluvA/Vpy", {"admin"})); // password: root
    const AuthenticationProviderPtr authenticationProvider = StaticAuthenticationProvider(true, users);

    PropertyObjectPtr config = PropertyObject();
    config.addProperty(StringProperty("Name", "Reference device simulator"));
    config.addProperty(StringProperty("LocalId", "RefDevSimulator"));
    config.addProperty(StringProperty("SerialNumber", "sim01"));
    config.addProperty(BoolProperty("EnableLogging", true));
    config.addProperty(StringProperty("LoggingPath", loggerPath));

    const InstancePtr instance = InstanceBuilder().addModulePath(MODULE_PATH)
                                     .addDiscoveryServer("mdns")
                                     .setRootDevice("daqref://device1", config)
                                     .setLogger(Logger(loggerPath))
                                     .setAuthenticationProvider(authenticationProvider)
                                     .build();

    auto refDevice = instance.addDevice("daqref://device0");
    refDevice.setPropertyValue("EnableProtectedChannel", true);

    auto serverConfig = instance.getAvailableServerTypes().get("OpenDAQMQTT").createDefaultConfig();
    serverConfig.setPropertyValue("MqttBrokerAddress", "127.0.0.1");
    const auto mqttServer = instance.addServer("OpenDAQMQTT", serverConfig);

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
