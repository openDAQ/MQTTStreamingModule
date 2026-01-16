cmake_minimum_required(VERSION 3.10)
set_cmake_folder_context(TARGET_FOLDER_NAME)

set(MQTT_STREAMING_PROTOCOL_VERSION ${OPENDAQ_PACKAGE_VERSION})
set(MQTT_STREAMING_PROTOCOL_PRJ_NAME "OpenDaqMqttStreamingProtocol")

message(STATUS "${MQTT_STREAMING_PROTOCOL_PRJ_NAME} version: ${MQTT_STREAMING_PROTOCOL_VERSION}")
project(${MQTT_STREAMING_PROTOCOL_PRJ_NAME} VERSION ${MQTT_STREAMING_PROTOCOL_VERSION} LANGUAGES C CXX)

if(MSVC)
    add_compile_options(
        $<$<CONFIG:Release>:/wd4201>
    )
endif()

add_subdirectory(src)
if (OPENDAQ_MQTT_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
