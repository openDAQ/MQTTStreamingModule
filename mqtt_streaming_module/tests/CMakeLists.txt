set(MODULE_NAME mqtt_stream_module)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_mqtt_streaming_module.cpp
                 test_mqtt_root_fb.cpp
                 test_mqtt_raw_fb.cpp
                 test_mqtt_json_fb.cpp
                 test_mqtt_json_decoder_fb.cpp
                 test_mqtt_publisher_fb.cpp
                 test_daq_test_helper.h
                 test_app.cpp
                 test_data.h
)

add_executable(${TEST_APP} ${TEST_SOURCES}
)

target_link_libraries(${TEST_APP} PRIVATE daq::test_utils
                                          ${MODULE_NAME}
                                          mqtt_streaming_protocol_test_helper
                                          mqtt_streaming_helper
)

if (MSVC)
    target_compile_options(${TEST_APP} PRIVATE /bigobj)
endif()

add_custom_command(
    TARGET ${TEST_APP} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:${TEST_APP}>/data
)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)
