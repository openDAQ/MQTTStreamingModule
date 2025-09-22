set(OPENDAQ_ENABLE_TESTS false)

FetchContent_Declare(
     opendaq
     GIT_REPOSITORY git@github.com:openDAQ/openDAQ.git
     GIT_TAG v3.20.4
     GIT_PROGRESS   ON
     EXCLUDE_FROM_ALL
     SYSTEM
)

FetchContent_MakeAvailable(opendaq)
