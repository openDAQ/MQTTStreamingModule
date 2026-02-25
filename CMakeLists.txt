set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.25)

set(REPO_NAME MqttModule)
set(REPO_OPTION_PREFIX DAQMODULES_MQTT)
set(CMAKE_FOLDER "${CMAKE_FOLDER}/${REPO_NAME}")

list(APPEND CMAKE_MESSAGE_CONTEXT ${REPO_NAME})
add_subdirectory(cmake)

opendaq_read_file_contents("${CMAKE_CURRENT_LIST_DIR}/module_version" module_version)
opendaq_get_version_major_minor_patch("${module_version}" ${REPO_OPTION_PREFIX}_VERSION)

# the following section is required if module-project being built standalone with 32bit arch, it is "temporary" workaround, though - the correct one would be using `-DCMAKE_TOOLCHAIN_FILE`
# 32-bit Linux cross-compilation setup (must be before any project())
if (NOT DEFINED PROJECT_SOURCE_DIR AND OPENDAQ_FORCE_COMPILE_32BIT AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    opendaq_32bit_build_linux_early_setup()
endif()

project(${REPO_NAME} VERSION ${${REPO_OPTION_PREFIX}_VERSION} LANGUAGES CXX)

opendaq_common_early_setup()

if (PROJECT_IS_TOP_LEVEL)
    message(STATUS "Building ${REPO_NAME} version ${${REPO_OPTION_PREFIX}_VERSION} standalone")
else()
    message(STATUS "Building ${REPO_NAME} version ${${REPO_OPTION_PREFIX}_VERSION} as submodule")
endif()

# options
opendaq_setup_common_build_options()
opendaq_setup_project_specific_build_options(${REPO_OPTION_PREFIX})
option(${REPO_OPTION_PREFIX}_ENABLE_EXAMPLE_APP "Enable ${REPO_NAME} example applications" ${PROJECT_IS_TOP_LEVEL})
option(${REPO_OPTION_PREFIX}_ENABLE_TESTS "Enable ${REPO_NAME} testing" ${PROJECT_IS_TOP_LEVEL})
option(${REPO_OPTION_PREFIX}_ENABLE_SSL "Enable building with openSSL" OFF)

opendaq_common_compile_targets_settings()
opendaq_setup_compiler_flags(${REPO_OPTION_PREFIX})

if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    message(STATUS "Unit tests in ${REPO_NAME} are ENABLED")
    enable_testing()
else()
    message(STATUS "Unit tests in ${REPO_NAME} are DISABLED")
endif()

if(${REPO_OPTION_PREFIX}_ENABLE_EXAMPLE_APP)
    set(DAQMODULES_REF_DEVICE_MODULE ON CACHE BOOL "" FORCE)
endif()

if(${REPO_OPTION_PREFIX}_ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    if (OPENSSL_FOUND)
        message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
    else()
        message(FATAL_ERROR "OpenSSL Not Found")
    endif()
    add_compile_definitions(${REPO_OPTION_PREFIX}_ENABLE_SSL)
endif()

if (NOT TARGET "${OPENDAQ_SDK_TARGET_NAMESPACE}::${OPENDAQ_SDK_TARGET_NAME}")
    if (PROJECT_IS_TOP_LEVEL)
        find_package(${OPENDAQ_SDK_NAME} GLOBAL)
    endif()
    if (NOT ${OPENDAQ_SDK_NAME}_FOUND)
        include(FetchContent)
        set(OPENDAQ_ENABLE_TESTS OFF CACHE BOOL "")

        opendaq_read_file_contents("${CMAKE_CURRENT_LIST_DIR}/opendaq_ref" OPENDAQ_REF)
        opendaq_get_custom_fetch_content_params(${OPENDAQ_SDK_NAME} FC_PARAMS)

        FetchContent_Declare(
            ${OPENDAQ_SDK_NAME}
            GIT_REPOSITORY https://github.com/openDAQ/openDAQ.git
            GIT_TAG ${OPENDAQ_REF}
            GIT_PROGRESS   ON
            ${FC_PARAMS}
        )
        FetchContent_MakeAvailable(${OPENDAQ_SDK_NAME})
    else()
        message(STATUS "Found installed ${OPENDAQ_SDK_NAME} version: ${${OPENDAQ_SDK_NAME}_VERSION}")
        set(OPENDAQ_PACKAGE_VERSION "${${OPENDAQ_SDK_NAME}_VERSION}" CACHE INTERNAL "${OPENDAQ_SDK_NAME} SDK version")
    endif()
endif()

add_subdirectory(external)
add_subdirectory(shared)
add_subdirectory(modules)

if(${REPO_OPTION_PREFIX}_ENABLE_EXAMPLE_APP)
    message(STATUS "Example applications in ${REPO_NAME} are ENABLED")
    add_subdirectory(examples)
else()
    message(STATUS "Example applications in ${REPO_NAME} are DISABLED")
endif()
