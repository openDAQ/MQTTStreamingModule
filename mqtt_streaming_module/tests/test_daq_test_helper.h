#pragma once
#include <mqtt_streaming_module/module_dll.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/instance_factory.h>

using namespace daq::modules::mqtt_streaming_module;

class DaqTestHelper
{
public:
    daq::InstancePtr daqInstance;
    daq::FunctionBlockPtr rootMqttFb;
    daq::FunctionBlockPtr subMqttFb;

    void StartUp(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        DaqInstanceInit();
        DaqMqttFbInit(url, port);
    }

    daq::InstancePtr DaqInstanceInit()
    {
        if (!daqInstance.assigned())
            daqInstance = daq::Instance();
        return daqInstance;
    }

    daq::FunctionBlockPtr DaqAddRootMqttFb(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        auto config = DaqMqttFbConfig(url, port);
        rootMqttFb = daqInstance.addFunctionBlock(CLIENT_FB_NAME, config);
        return rootMqttFb;
    }

    daq::FunctionBlockPtr DaqMqttFbInit(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        if (!rootMqttFb.assigned())
        {
            auto config = DaqMqttFbConfig(url, port);
            rootMqttFb = daqInstance.addFunctionBlock(CLIENT_FB_NAME, config);
        }
        return rootMqttFb;
    }

    daq::PropertyObjectPtr DaqMqttFbConfig(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());

        auto config = module.getAvailableFunctionBlockTypes().get(daq::modules::mqtt_streaming_module::CLIENT_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_CLIENT_BROKER_ADDRESS, url);
        config.setPropertyValue(PROPERTY_NAME_CLIENT_BROKER_PORT, port);
        return config;
    }

    static daq::ModulePtr CreateModule()
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());
        return module;
    }

    daq::FunctionBlockPtr AddSubFb(std::string topic = "")
    {
        auto config = rootMqttFb.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, daq::String(topic));
        subMqttFb = rootMqttFb.addFunctionBlock(SUB_FB_NAME, config);
        return subMqttFb;
    }
};
