cmake_minimum_required(VERSION 3.16)
opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

if (${REPO_OPTION_PREFIX}_ENABLE_EXAMPLE_APP)
    add_subdirectory(raw-mqtt-sub)
    add_subdirectory(custom-mqtt-sub)
    add_subdirectory(ref-dev-mqtt-pub)
endif()
