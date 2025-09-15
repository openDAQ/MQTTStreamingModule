set(OPENDAQ_ENABLE_TESTS false)

FetchContent_Declare(
     opendaq
     GIT_REPOSITORY git@github.com:openDAQ/openDAQ.git
     GIT_TAG b38e835b591317fdd1f9f84aad2bd79f04c06fd3
     GIT_PROGRESS   ON
     EXCLUDE_FROM_ALL
     SYSTEM
)

FetchContent_MakeAvailable(opendaq)
