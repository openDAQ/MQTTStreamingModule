cmake_minimum_required(VERSION 3.16)

set(CMAKE_FOLDER "${CMAKE_FOLDER}/examples")

if (OPENDAQ_MQTT_ENABLE_EXAMPLE_APPS)
    add_subdirectory(raw-mqtt-sub)
    add_subdirectory(custom-mqtt-sub)
    add_subdirectory(ref-dev-mqtt-pub)
endif()
