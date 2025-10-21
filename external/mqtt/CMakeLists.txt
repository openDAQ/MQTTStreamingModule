FetchContent_Declare(
    paho_mqtt_c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG        v1.3.12
    GIT_PROGRESS   ON
    GIT_SHALLOW    ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Populate(paho_mqtt_c)

# Set build options BEFORE add_subdirectory
set(PAHO_HIGH_PERFORMANCE OFF CACHE BOOL "Enable PAHO High performance" FORCE)
set(PAHO_WITH_SSL ON CACHE BOOL "Enable SSL support" FORCE)
set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static paho library" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build dynamic paho library" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ${PAHO_BUILD_STATIC} CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING ON CACHE BOOL "" FORCE)

# Now add the project
add_subdirectory(${paho_mqtt_c_SOURCE_DIR} ${paho_mqtt_c_BINARY_DIR})

# Apply WIN32_LEAN_AND_MEAN to the Paho MQTT C library as well
if(WIN32)
    # Add global compile definition to prevent inclusion of winsock.h and force winsock2.h
    if(PAHO_WITH_SSL)
		target_compile_definitions(paho-mqtt3as PRIVATE _WINSOCKAPI_)
		target_compile_definitions(paho-mqtt3as PRIVATE NOMINMAX)
		target_compile_definitions(paho-mqtt3as PRIVATE WIN32_LEAN_AND_MEAN)
    	target_compile_definitions(common_ssl_obj PRIVATE _WINSOCKAPI_)
    	target_compile_definitions(common_ssl_obj PRIVATE NOMINMAX)
    	target_compile_definitions(common_ssl_obj PRIVATE WIN32_LEAN_AND_MEAN)
    else()
    	target_compile_definitions(paho-mqtt3a PRIVATE _WINSOCKAPI_)
    	target_compile_definitions(paho-mqtt3a PRIVATE NOMINMAX)
    	target_compile_definitions(paho-mqtt3a PRIVATE WIN32_LEAN_AND_MEAN)
    	target_compile_definitions(common_obj PRIVATE _WINSOCKAPI_)
    	target_compile_definitions(common_obj PRIVATE NOMINMAX)
    	target_compile_definitions(common_obj PRIVATE WIN32_LEAN_AND_MEAN)
    endif()
endif()