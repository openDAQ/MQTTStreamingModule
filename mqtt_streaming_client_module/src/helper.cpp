#include "mqtt_streaming_client_module/helper.h"
#include <boost/algorithm/string/replace.hpp>
#include <coreobjects/property_internal_ptr.h>
#include <coreobjects/property_object_factory.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE

std::string buildSignalNameFromTopic(std::string topic, const std::string& signalName)
{
    boost::replace_all(topic, "/", "_");
    topic += "_Mqtt" + signalName;
    return topic;
}

std::string buildDomainSignalNameFromTopic(std::string topic, const std::string& signalName)
{
    boost::replace_all(topic, "/", "_");
    topic += std::string("_Mqtt") + "_domain" + signalName;
    return topic;
}

PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& defaultConfig, const PropertyObjectPtr& config)
{
    auto newConfig = PropertyObject();
    for (const auto& prop : defaultConfig.getAllProperties())
    {
        newConfig.addProperty(prop.asPtr<IPropertyInternal>(true).clone());
        const auto propName = prop.getName();
        newConfig.setPropertyValue(propName, config.hasProperty(propName) ? config.getPropertyValue(propName) : prop.getValue());
    }
    return newConfig;
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_CLIENT_MODULE
