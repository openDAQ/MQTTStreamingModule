cmake_minimum_required(VERSION 3.16)

if (OPENDAQ_MQTT_ENABLE_EXAMPLE_APPS)
    add_subdirectory(ref-dev-mqtt-pub)
    add_subdirectory(ref-dev-mqtt-sub)
endif()
