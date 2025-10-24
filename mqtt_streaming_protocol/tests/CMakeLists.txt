set(MODULE_NAME mqtt_streaming_protocol)
set(TEST_APP test_${MODULE_NAME})
set(TEST_LIB ${MODULE_NAME}_test_helper)

set(TEST_SOURCES test_mqtt_streaming_protocol.cpp
                 test_app.cpp
)

add_executable(${TEST_APP} ${TEST_SOURCES}
)

add_library(${TEST_LIB} STATIC MqttAsyncClientWrapper.h
                               MqttAsyncClientWrapper.cpp
                               Timer.h
)

target_include_directories(${TEST_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TEST_LIB} PRIVATE ${MODULE_NAME}
)

target_link_libraries(${TEST_APP} PRIVATE daq::test_utils
                                          ${MODULE_NAME}
                                          ${TEST_LIB}
)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)
