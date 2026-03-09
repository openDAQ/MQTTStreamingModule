cmake_minimum_required(VERSION 3.25)
opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

if (MSVC)
    add_compile_options(/wd4100)
endif()

add_subdirectory(mqtt_streaming_module)
