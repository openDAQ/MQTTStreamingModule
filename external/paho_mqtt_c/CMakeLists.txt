opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

set(PAHO_HIGH_PERFORMANCE OFF CACHE BOOL "Enable PAHO High performance" FORCE)

set(PAHO_WITH_SSL ${${REPO_OPTION_PREFIX}_ENABLE_SSL} CACHE BOOL "Enable SSL support" FORCE)

set(PAHO_BUILD_STATIC ON CACHE BOOL "Build static paho library" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "Build dynamic paho library" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ${PAHO_BUILD_STATIC} CACHE BOOL "" FORCE)
set(OPENDAQ_ALWAYS_FETCH_PAHO_MQTT_C ON CACHE BOOL "" FORCE)

opendaq_dependency(
    NAME                paho_mqtt_c
    REQUIRED_VERSION    1.3.12
    GIT_REPOSITORY      https://github.com/eclipse/paho.mqtt.c.git
    GIT_REF             v1.3.12
)

if (paho_mqtt_c_FETCHED)
    

    # Apply WIN32_LEAN_AND_MEAN to the Paho MQTT C library as well
    if(WIN32)
        # Add global compile definition to prevent inclusion of winsock.h and force winsock2.h
        foreach(tgt
            paho-mqtt3a
            paho-mqtt3a-static
            paho-mqtt3c
            paho-mqtt3c-static
            paho-mqtt3as
            paho-mqtt3as-static
            common_obj
            common_ssl_obj
            common_obj_static
            common_ssl_obj_static
        )
            if(TARGET ${tgt})
                target_compile_definitions("${tgt}" PRIVATE _WINSOCKAPI_ NOMINMAX WIN32_LEAN_AND_MEAN)
                if(MSVC)
                    target_compile_options(${tgt} PRIVATE /WX-)
                endif()
            endif()
        endforeach()
        target_compile_options(Base64Test PRIVATE /WX-)
        target_compile_options(Sha1Test PRIVATE /WX-)
    endif()
endif()
