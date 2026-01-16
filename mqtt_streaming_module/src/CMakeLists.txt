set(LIB_NAME mqtt_stream_module)
set(MODULE_HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

set(SRC_Include common.h
                module_dll.h
                mqtt_streaming_module_impl.h
                mqtt_root_fb_impl.h
                mqtt_base_fb.h
                mqtt_json_receiver_fb_impl.h
                mqtt_json_decoder_fb_impl.h
                mqtt_raw_receiver_fb_impl.h
                mqtt_publisher_fb_impl.h
                constants.h
                helper.h
                handler_factory.h
                handler_base.h
                atomic_signal_atomic_sample_handler.h
                atomic_signal_sample_arr_handler.h
                group_signal_shared_ts_handler.h
                signal_arr_atomic_sample_handler.h
                types.h
                status_helper.h
)

set(SRC_Srcs module_dll.cpp
             mqtt_streaming_module_impl.cpp
             mqtt_root_fb_impl.cpp
             mqtt_base_fb.cpp
             mqtt_json_receiver_fb_impl.cpp
             mqtt_json_decoder_fb_impl.cpp
             mqtt_raw_receiver_fb_impl.cpp
             mqtt_publisher_fb_impl.cpp
             helper.cpp
             atomic_signal_atomic_sample_handler.cpp
             atomic_signal_sample_arr_handler.cpp
             group_signal_shared_ts_handler.cpp
             signal_arr_atomic_sample_handler.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Include)

source_group("common" FILES ${MODULE_HEADERS_DIR}/common.h
                            ${MODULE_HEADERS_DIR}/constants.h
                            ${MODULE_HEADERS_DIR}/helper.h
                            ${MODULE_HEADERS_DIR}/types.h
                            ${MODULE_HEADERS_DIR}/status_helper.h
                            helper.cpp
)

source_group("module" FILES ${MODULE_HEADERS_DIR}/mqtt_streaming_module_impl.h
                            ${MODULE_HEADERS_DIR}/module_dll.h
                            module_dll.cpp
                            mqtt_streaming_module_impl.cpp
)

source_group("functionalBlock" FILES ${MODULE_HEADERS_DIR}/mqtt_json_receiver_fb_impl.h
                            mqtt_json_receiver_fb_impl.cpp
                            ${MODULE_HEADERS_DIR}/mqtt_json_decoder_fb_impl.h
                            mqtt_json_decoder_fb_impl.cpp
                            ${MODULE_HEADERS_DIR}/mqtt_raw_receiver_fb_impl.h
                            mqtt_raw_receiver_fb_impl.cpp
                            ${MODULE_HEADERS_DIR}/mqtt_publisher_fb_impl.h
                            mqtt_publisher_fb_impl.cpp
                            ${MODULE_HEADERS_DIR}/mqtt_base_fb.h
                            mqtt_base_fb.cpp
                            ${MODULE_HEADERS_DIR}/mqtt_root_fb_impl.h
                            mqtt_root_fb_impl.cpp
)

source_group("handlers" FILES ${MODULE_HEADERS_DIR}/handler_base.h
                            ${MODULE_HEADERS_DIR}/handler_factory.h
                            ${MODULE_HEADERS_DIR}/atomic_signal_atomic_sample_handler.h
                            ${MODULE_HEADERS_DIR}/atomic_signal_sample_arr_handler.h
                            ${MODULE_HEADERS_DIR}/group_signal_shared_ts_handler.h
                            ${MODULE_HEADERS_DIR}/signal_arr_atomic_sample_handler.h
                            atomic_signal_atomic_sample_handler.cpp
                            atomic_signal_sample_arr_handler.cpp
                            group_signal_shared_ts_handler.cpp
                            signal_arr_atomic_sample_handler.cpp
)

find_package(Boost REQUIRED COMPONENTS algorithm)

add_library(${LIB_NAME} SHARED ${SRC_Include}
                               ${SRC_Srcs}
)
add_library(${SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /bigobj)
endif()

if (WIN32)
    target_compile_definitions(${LIB_NAME} PRIVATE OPENDAQ_MODULE_DLL_IMPORT)
endif()

target_link_libraries(${LIB_NAME} PUBLIC  daq::opendaq
                                          mqtt_streaming_protocol
                                          mqtt_streaming_helper
                                  PRIVATE $<BUILD_INTERFACE:Boost::algorithm>
)

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})

