cmake_minimum_required(VERSION 3.16)
project(pc-monitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    webui
    GIT_REPOSITORY https://github.com/webui-dev/webui.git
    GIT_TAG        main
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

FetchContent_MakeAvailable(webui json)

add_executable(pc-monitor
    src/backend/main.cpp
        src/backend/SystemMonitor.cpp
        src/backend/SystemMonitor.hpp
        src/backend/CpuReader.cpp
        src/backend/CpuReader.hpp
        src/backend/MemReader.cpp
        src/backend/MemReader.hpp
        src/backend/ProcessReader.cpp
        src/backend/ProcessReader.hpp
)
target_link_libraries(pc-monitor PRIVATE webui nlohmann_json::nlohmann_json)
find_package(Threads REQUIRED)
target_link_libraries(pc-monitor PRIVATE Threads::Threads)

add_custom_command(TARGET pc-monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/frontend"
        "$<TARGET_FILE_DIR:pc-monitor>/frontend"
)
