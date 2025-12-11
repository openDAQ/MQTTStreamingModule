set(CMAKE_FOLDER "${CMAKE_FOLDER}/external")
list(APPEND CMAKE_MESSAGE_CONTEXT external)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source build is not supported!")
endif()

include(FetchContent)

add_subdirectory(opendaq)
add_subdirectory(mqtt)
add_subdirectory(rapidjson)