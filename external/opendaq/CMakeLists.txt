set(OPENDAQ_ENABLE_TESTS false)

FetchContent_Declare(
     opendaq
     GIT_REPOSITORY https://github.com/openDAQ/openDAQ.git
     GIT_TAG 41396d19a1567ab6aecacfe7e381ea616dfdad6c
     GIT_PROGRESS   ON
     EXCLUDE_FROM_ALL
     SYSTEM
)

FetchContent_MakeAvailable(opendaq)

# Ensure we have the source dir variable (set by FetchContent)
if(NOT DEFINED opendaq_SOURCE_DIR)
  FetchContent_GetProperties(opendaq)
  if(NOT opendaq_POPULATED)
    FetchContent_Populate(opendaq)
  endif()
endif()

set(OPENDAQ_VERSION_FILE "${opendaq_SOURCE_DIR}/opendaq_version")

if(EXISTS "${OPENDAQ_VERSION_FILE}")
  file(READ "${OPENDAQ_VERSION_FILE}" _raw_version)
  string(STRIP "${_raw_version}" _raw_version)

  string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" OPENDAQ_PACKAGE_VERSION "${_raw_version}")

  message(STATUS "openDAQ version: ${OPENDAQ_PACKAGE_VERSION}")

  set(OPENDAQ_PACKAGE_VERSION "${OPENDAQ_PACKAGE_VERSION}" CACHE STRING "openDAQ package version" FORCE)
else()
  message(WARNING "opendaq_version file not found at: ${OPENDAQ_VERSION_FILE}")
endif()
