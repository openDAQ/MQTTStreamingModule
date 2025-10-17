set(LIB_NAME mqtt_stream_cli_module)
set(MODULE_HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

set(SRC_Include common.h
                module_dll.h
                mqtt_streaming_client_module_impl.h
                mqtt_streaming_device_impl.h
                mqtt_receiver_fb_impl.h
                constants.h
)

set(SRC_Srcs module_dll.cpp
             mqtt_streaming_client_module_impl.cpp
             mqtt_streaming_device_impl.cpp
             mqtt_receiver_fb_impl.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Include)

source_group("common" FILES ${MODULE_HEADERS_DIR}/common.h
                            ${MODULE_HEADERS_DIR}/constants.h
)

source_group("module" FILES ${MODULE_HEADERS_DIR}/mqtt_streaming_client_module_impl.h
                            ${MODULE_HEADERS_DIR}/module_dll.h
                            module_dll.cpp
                            mqtt_streaming_client_module_impl.cpp
)

source_group("device" FILES ${MODULE_HEADERS_DIR}/mqtt_streaming_device_impl.h
                            mqtt_streaming_device_impl.cpp
)

source_group("functionalBlock" FILES ${MODULE_HEADERS_DIR}/mqtt_receiver_fb_impl.h
                            dispatch.h
                            mqtt_receiver_fb_impl.cpp
)

find_package(Boost REQUIRED COMPONENTS algorithm)

add_library(${LIB_NAME} SHARED ${SRC_Include}
                               ${SRC_Srcs}
)
add_library(${SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /bigobj)
endif()

target_link_libraries(${LIB_NAME} PUBLIC  daq::opendaq
                                  PRIVATE mqtt_streaming_protocol
                                          $<BUILD_INTERFACE:Boost::algorithm>
)

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})

