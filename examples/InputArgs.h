#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>

class InputArgs
{
public:
    void addArg(const std::string& name, const std::string& description, bool hasValue = false)
    {
        argDescriptions[name] = {description, hasValue};
    }

    void parse(int argc, char* argv[])
    {
        parsedArgs.clear();
        argValues.clear();
        positionalArgs.clear();

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg.rfind("--", 0) == 0)
            {
                auto eqPos = arg.find('=');
                if (eqPos != std::string::npos)
                {
                    std::string key = arg.substr(0, eqPos);
                    std::string value = arg.substr(eqPos + 1);
                    argValues[key] = value;
                    parsedArgs.push_back(key);
                }
                else if (i + 1 < argc && argDescriptions[arg].hasValue)
                {
                    argValues[arg] = argv[i + 1];
                    parsedArgs.push_back(arg);
                    ++i;
                }
                else
                {
                    parsedArgs.push_back(arg);
                }
            }
            else
            {
                positionalArgs.push_back(arg);
            }
        }
    }

    bool hasArg(const std::string& name) const
    {
        return std::find(parsedArgs.begin(), parsedArgs.end(), name) != parsedArgs.end();
    }

    std::string getArgValue(const std::string& name, const std::string& defaultValue = "") const
    {
        auto it = argValues.find(name);
        if (it != argValues.end())
            return it->second;
        return defaultValue;
    }

    const std::vector<std::string>& getPositionalArgs() const
    {
        return positionalArgs;
    }

    bool hasUnknownArgs() const
    {
        for (const auto& arg : parsedArgs)
        {
            if (argDescriptions.find(arg) == argDescriptions.end())
                return true;
        }
        return false;
    }

    void setUsageHelp(const std::string& str)
    {
        usageString = str;
    }

    void printHelp() const
    {
        if (!usageString.empty())
            std::cout << "Usage: " << usageString << std::endl;
        std::cout << "Available arguments:" << std::endl;
        for (const auto& [name, descStruct] : argDescriptions)
        {
            std::cout << "  " << name;
            if (descStruct.hasValue)
                std::cout << " <value>";
            std::cout << " : " << descStruct.description << std::endl;
        }
    }

private:
    struct ArgDesc
    {
        std::string description;
        bool hasValue;
    };
    std::unordered_map<std::string, ArgDesc> argDescriptions;
    std::vector<std::string> parsedArgs;
    std::unordered_map<std::string, std::string> argValues;
    std::vector<std::string> positionalArgs;
    std::string usageString;
};
