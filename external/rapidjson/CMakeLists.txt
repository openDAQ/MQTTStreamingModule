FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG        v1.1.0
	GIT_PROGRESS   ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "Disable documentation" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)

FetchContent_MakeAvailable(rapidjson)

message(STATUS "RapidJSON dir: ${rapidjson_SOURCE_DIR}")