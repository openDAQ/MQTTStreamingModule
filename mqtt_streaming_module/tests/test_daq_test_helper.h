#pragma once
#include <mqtt_streaming_module/module_dll.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/instance_factory.h>

class DaqTestHelper
{
public:
    daq::InstancePtr daqInstance;
    daq::DevicePtr device;

    void StartUp(std::string connectionStr = "daq.mqtt://127.0.0.1", int discoveryTimeoutMs = 0)
    {
        DaqInstanceInit();
        DaqMqttDeviceInit(connectionStr, discoveryTimeoutMs);
    }

    daq::InstancePtr DaqInstanceInit()
    {
        if (!daqInstance.assigned())
            daqInstance = daq::Instance();
        return daqInstance;
    }

    daq::GenericDevicePtr<daq::IDevice> DaqMqttDeviceInit(std::string connectionStr, int discoveryTimeoutMs = -1)
    {
        if (!device.assigned())
        {
            if (discoveryTimeoutMs >= 0)
            {
                daq::ModulePtr module;
                createModule(&module, daq::NullContext());

                auto config = module.getAvailableDeviceTypes().get(daq::modules::mqtt_streaming_module::DaqMqttDeviceTypeId).createDefaultConfig();
                config.setPropertyValue(daq::modules::mqtt_streaming_module::PROPERTY_NAME_DISCOVERY_TIMEOUT, 0);
                device = daqInstance.addDevice(connectionStr, config);
            }
            else
            {
                device = daqInstance.addDevice(connectionStr);
            }
        }
        return device;
    }

    daq::PropertyObjectPtr DaqMqttDeviceConfig(int discoveryTimeoutMs = -1)
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());

        auto config = module.getAvailableDeviceTypes().get(daq::modules::mqtt_streaming_module::DaqMqttDeviceTypeId).createDefaultConfig();
        if (discoveryTimeoutMs >= 0)
        {
            config.setPropertyValue(daq::modules::mqtt_streaming_module::PROPERTY_NAME_DISCOVERY_TIMEOUT, 0);
        }

        return config;
    }

    static daq::ModulePtr CreateModule()
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());
        return module;
    }
};
