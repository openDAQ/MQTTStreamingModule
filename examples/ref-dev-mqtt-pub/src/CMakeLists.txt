cmake_minimum_required(VERSION 3.16)

opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

if (TARGET daq::ref_device_module)
    add_compile_definitions(MODULE_PATH="${OPENDAQ_MODULES_DIR}")
    add_compile_definitions(APP_NAME="${EXAMPLE_PROJECT_NAME}")
    
    add_executable(${EXAMPLE_PROJECT_NAME} ref-dev-mqtt-pub.cpp)
    add_dependencies(${EXAMPLE_PROJECT_NAME} daq::ref_device_module)
    target_link_libraries(${EXAMPLE_PROJECT_NAME} PRIVATE daq::opendaq)
    target_include_directories(${EXAMPLE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
else()
    message(WARNING "Building ${EXAMPLE_PROJECT_NAME} is canceled because of missing dependencies")
endif()


