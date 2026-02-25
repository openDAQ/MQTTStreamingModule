cmake_minimum_required(VERSION 3.25)
opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

message(STATUS "${REPO_NAME} version: ${${REPO_OPTION_PREFIX}_VERSION}")
project(${REPO_NAME} VERSION ${${REPO_OPTION_PREFIX}_VERSION} LANGUAGES C CXX)

if(MSVC)
    add_compile_options(
        $<$<CONFIG:Release>:/wd4201>
    )
endif()

add_subdirectory(src)
if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
