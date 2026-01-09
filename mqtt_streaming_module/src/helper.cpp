#include "mqtt_streaming_module/helper.h"
#include <boost/algorithm/string/replace.hpp>
#include <coreobjects/property_internal_ptr.h>
#include <coreobjects/property_object_factory.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

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

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
