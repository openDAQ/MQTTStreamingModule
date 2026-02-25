opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source build is not supported!")
endif()

add_subdirectory(paho_mqtt_c)
add_subdirectory(rapidjson)
