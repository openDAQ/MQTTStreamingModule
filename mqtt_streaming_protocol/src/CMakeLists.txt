set(LIB_NAME mqtt_streaming_protocol)

set(SRC_Cpp MqttAsyncClient.cpp
            MqttDataWrapper.cpp
)

set(SRC_PublicHeaders   MqttAsyncClient.h
			MqttMessage.h
			MqttSettings.h
                        MqttDataWrapper.h
)

set(INCLUDE_DIR ../include)
prepend_include(${INCLUDE_DIR} SRC_PublicHeaders)

source_group("include" FILES ${SRC_PublicHeaders})

find_package(Boost REQUIRED COMPONENTS algorithm)

add_library(${LIB_NAME} STATIC ${SRC_Cpp}
                               ${SRC_PublicHeaders}
)

add_library(${SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if(BUILD_64Bit OR BUILD_ARM)
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

if(PAHO_WITH_SSL)
	set(PAHO_LIB paho-mqtt3as)
else()
	set(PAHO_LIB paho-mqtt3a)
endif()

if(PAHO_BUILD_STATIC)
	set(PAHO_LIB ${PAHO_LIB}-static)
endif()

target_link_libraries(${LIB_NAME} PUBLIC ${PAHO_LIB}
                                         daq::opendaq
                                         rapidjson
                                  PRIVATE $<BUILD_INTERFACE:Boost::algorithm>
)

if (WIN32)
    target_link_libraries(${LIB_NAME} PUBLIC
            ws2_32
            wsock32
    )
endif()

set_target_properties(${LIB_NAME} PROPERTIES PUBLIC_HEADER "${SRC_PublicHeaders}")

opendaq_set_output_lib_name(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
