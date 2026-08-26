#pragma once
#include <mqtt_streaming_module/module_dll.h>
#include <mqtt_streaming_module/constants.h>
#include <opendaq/instance_factory.h>
#include <string>

using namespace daq::modules::mqtt_streaming_module;

class DaqTestHelper
{
public:
    daq::InstancePtr daqInstance;
    daq::DevicePtr mqttDevice;
    daq::FunctionBlockPtr subMqttFb;

    void StartUp(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        DaqInstanceInit();
        DaqMqttDeviceInit(url, port);
    }

    daq::InstancePtr DaqInstanceInit()
    {
        if (!daqInstance.assigned())
            daqInstance = daq::Instance();
        return daqInstance;
    }

    static std::string MqttConnectionString(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        return std::string(CLIENT_DEVICE_CONN_PREFIX) + "://" + url + ":" + std::to_string(port);
    }

    daq::DevicePtr DaqAddMqttDevice(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        mqttDevice = daqInstance.addDevice(MqttConnectionString(url, port), DaqMqttDeviceConfig());
        return mqttDevice;
    }

    daq::DevicePtr DaqMqttDeviceInit(std::string url = DEFAULT_BROKER_ADDRESS, uint16_t port = DEFAULT_PORT)
    {
        if (!mqttDevice.assigned())
            DaqAddMqttDevice(url, port);
        return mqttDevice;
    }

    static daq::PropertyObjectPtr DaqMqttDeviceConfig()
    {
        return CreateModule().getAvailableDeviceTypes().get(CLIENT_DEVICE_TYPE_ID).createDefaultConfig();
    }

    static daq::ModulePtr CreateModule()
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());
        return module;
    }

    daq::FunctionBlockPtr AddSubFb(std::string topic = "")
    {
        auto config = mqttDevice.getAvailableFunctionBlockTypes().get(SUB_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_SUB_TOPIC, daq::String(topic));
        subMqttFb = mqttDevice.addFunctionBlock(SUB_FB_NAME, config);
        return subMqttFb;
    }
};
