opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

if (NOT TARGET rapidjson)
    set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "Disable documentation" FORCE)
    set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
    set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)

    opendaq_dependency(
        NAME                rapidjson
        REQUIRED_VERSION    1.1.0
        GIT_REPOSITORY      https://github.com/Tencent/rapidjson.git
        GIT_REF             v1.1.0
    )

    message(STATUS "RapidJSON source directory: ${rapidjson_SOURCE_DIR}")

    add_library(rapidjson INTERFACE)
    target_include_directories(rapidjson SYSTEM INTERFACE ${rapidjson_SOURCE_DIR}/include)
endif()
