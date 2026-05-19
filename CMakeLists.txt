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

# ── Compile TypeScript: src/frontend/app.ts -> app.js ────────────────
set(FRONTEND_DIR "${CMAKE_SOURCE_DIR}/src/frontend")
set(APP_JS       "${FRONTEND_DIR}/app.js")

# find_program may pick up WSL-interop Windows binaries from /mnt/c, which can't
# operate on Linux paths. The helper re-searches over Linux-native locations.
function(_find_linux_program out_var name)
    find_program(${out_var} NAMES ${name})
    if(${out_var} MATCHES "^/mnt/")
        unset(${out_var} CACHE)
        file(GLOB _nvm_bins "$ENV{HOME}/.nvm/versions/node/*/bin")
        find_program(${out_var} NAMES ${name}
            HINTS ${_nvm_bins}
            PATHS /usr/local/bin /usr/bin
            NO_DEFAULT_PATH
        )
    endif()
endfunction()

_find_linux_program(TSC_EXECUTABLE tsc)
if(TSC_EXECUTABLE)
    set(TSC_CMD "${TSC_EXECUTABLE}")
else()
    _find_linux_program(NPX_EXECUTABLE npx)
    if(NOT NPX_EXECUTABLE)
        message(FATAL_ERROR
            "Neither 'tsc' nor 'npx' found in PATH. "
            "Install Node.js (npx) or TypeScript globally (npm i -g typescript).")
    endif()
    set(TSC_CMD "${NPX_EXECUTABLE}" --yes -p typescript@5.5.4 tsc)
endif()

add_custom_command(
    OUTPUT  "${APP_JS}"
    COMMAND ${TSC_CMD} -p "${FRONTEND_DIR}"
    DEPENDS "${FRONTEND_DIR}/app.ts" "${FRONTEND_DIR}/tsconfig.json"
    COMMENT "Compiling TypeScript -> app.js"
    VERBATIM
)
add_custom_target(frontend_ts DEPENDS "${APP_JS}")
add_dependencies(pc-monitor frontend_ts)

add_custom_command(TARGET pc-monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FRONTEND_DIR}"
        "$<TARGET_FILE_DIR:pc-monitor>/frontend"
)
