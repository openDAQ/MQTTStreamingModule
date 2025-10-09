# CMakeList.txt : CMake project for MQTTStreamingModule, include source and define
# project specific logic here.
#
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.25)

set(REPO_NAME mqtt_module)
set(REPO_OPTION_PREFIX MQTT_MODULE)

project (${REPO_NAME} CXX)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MESSAGE_CONTEXT ${REPO_NAME})
set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show CMake message context")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(OPENDAQ_DEVICE_EXAMPLE_ENABLE_EXAMPLE_APPS "Enable building example applications" OFF)

include(CommonUtils)
setup_repo(${REPO_OPTION_PREFIX})

if(OPENDAQ_DEVICE_EXAMPLE_ENABLE_EXAMPLE_APPS)
	set(DAQMODULES_REF_DEVICE_MODULE ON CACHE BOOL "" FORCE)
endif()


find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
  message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
else()
  message(STATUS "OpenSSL Not Found")
endif()

add_subdirectory(external)
add_subdirectory(mqtt_streaming_protocol)
add_subdirectory(mqtt_streaming_server_module)
add_subdirectory(mqtt_streaming_client_module)

if(OPENDAQ_DEVICE_EXAMPLE_ENABLE_EXAMPLE_APPS)
    message(STATUS "Example applications have been enabled")
    add_subdirectory(examples)
endif()
