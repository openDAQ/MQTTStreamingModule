cmake_minimum_required(VERSION 3.10)

set(HELPER_LIB mqtt_streaming_helper)

set(HELPER_PRJ_VERSION "1.0.0")
set(HELPER_PRJ_NAME "HelperUtils")

project(${HELPER_PRJ_NAME} VERSION ${HELPER_PRJ_VERSION} LANGUAGES C CXX)



add_library(${HELPER_LIB} INTERFACE)

target_include_directories(${HELPER_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${HELPER_LIB} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mqtt_streaming_helper/timer.h
)
