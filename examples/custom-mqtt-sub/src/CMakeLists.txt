cmake_minimum_required(VERSION 3.16)

add_compile_definitions(MODULE_PATH="${OPENDAQ_MODULES_DIR}")
add_compile_definitions(APP_NAME="${EXAMPLE_PROJECT_NAME}")

add_executable(${EXAMPLE_PROJECT_NAME} custom-mqtt-sub.cpp)
target_link_libraries(${EXAMPLE_PROJECT_NAME} PRIVATE daq::opendaq)
target_include_directories(${EXAMPLE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../)
