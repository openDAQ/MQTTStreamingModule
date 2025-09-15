cmake_minimum_required(VERSION 3.10)
set_cmake_folder_context(TARGET_FOLDER_NAME ${SDK_TARGET_NAMESPACE}_mqtt_streaming_protocol)
project(OpenDaqMqttStreamingProtocol
    VERSION 1.0.0
    LANGUAGES CXX
)

add_subdirectory(src)
