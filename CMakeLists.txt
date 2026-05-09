cmake_minimum_required(VERSION 3.16)
project(Abyss VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Release by default ─────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── SDL2 ───────────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2       REQUIRED sdl2)
pkg_check_modules(SDL2_TTF   REQUIRED SDL2_ttf)

# ── Sources ────────────────────────────────────────────────────────────────
add_executable(abyss src/main.cpp)

target_include_directories(abyss PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(abyss PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

target_compile_options(abyss PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall -Wextra -O2
)

# ── Optional: Discord RPC ──────────────────────────────────────────────────
# To enable Discord Rich Presence:
#   1. Download the Discord Game SDK from https://discord.com/developers/docs/game-sdk/sdk-starter-guide
#   2. Extract discord_game_sdk.zip and place:
#        discord_game_sdk/c/discord_rpc.h      -> lib/discord_rpc.h
#        discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so -> lib/
#   3. Configure with: cmake -DDISCORD_RPC=ON ..
#
option(DISCORD_RPC "Enable Discord Rich Presence" OFF)

if(DISCORD_RPC)
    message(STATUS "Discord RPC: ENABLED")
    target_compile_definitions(abyss PRIVATE DISCORD_RPC)
    target_include_directories(abyss PRIVATE ${CMAKE_SOURCE_DIR}/lib)
    target_link_libraries(abyss PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/libdiscord_game_sdk.so
    )
else()
    message(STATUS "Discord RPC: disabled (use -DDISCORD_RPC=ON to enable)")
endif()

# ── Install ────────────────────────────────────────────────────────────────
install(TARGETS abyss DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "To build:   mkdir build && cd build && cmake .. && make")
