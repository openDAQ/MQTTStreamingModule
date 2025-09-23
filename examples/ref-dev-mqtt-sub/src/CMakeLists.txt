cmake_minimum_required(VERSION 3.16)

add_compile_definitions(MODULE_PATH="${OPENDAQ_MODULES_DIR}")

add_executable(${EXAMPLE_PROJECT_NAME} ref-dev-mqtt-sub.cpp)
target_link_libraries(${EXAMPLE_PROJECT_NAME} PRIVATE daq::opendaq)
