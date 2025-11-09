# -*- coding: utf-8 -*-
# @brief Build file for Audacious Discord RPC plugin
#
# @author onegen <onegen@onegen.dev>
# @author Derzsi Dániel <daniel@tohka.us>
# @author Sturmlilie
#
# @code{.sh}
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# cmake --build build -j
# @endcode
#

cmake_minimum_required(VERSION 3.5)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  message(FATAL_ERROR
    "Sorry, but this project is currently Linux-only!\r\n\n"
    "There is an ongoing discussion about Windows support at:\r\n"
    "  https://github.com/onegentig/audacious-discord-rpc/issues/4\r\n"
    "You can add a thumbs up reaction to let us know you’d like to see a\r\n"
    "Windows version! And if you're interested, you can help porting the\r\n"
    "plugin to Windows (the issue is mostly CMake-related).\r\n"
  )
endif()

include(FetchContent)
include(FindPkgConfig)

# === COMPILER OPTS === #

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Og -fsanitize=undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 -march=native")

# === DEFINITION === #

project(audacious-discord-rpc
  VERSION 2.2
  HOMEPAGE_URL https://github.com/onegentig/audacious-discord-rpc
  LANGUAGES CXX
)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(audacious-discord-rpc SHARED ${SOURCES})
target_compile_features(audacious-discord-rpc PUBLIC cxx_std_23)

# === DEPENDENCIES === #

# == Audacious (audcore) == #
#
# If this fails, make sure your system has the Audacious
# development package installed, e.g. `audacious-dev` (DEB)
# or `audacious-devel` (RPM).

PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=4.0)
PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
add_definitions(${AUDACIOUS_CFLAGS})

# == O. Nemeš’s Discord RPC == #

FetchContent_Declare(
  discord-rpc
    GIT_REPOSITORY https://github.com/EclipseMenu/discord-presence.git
    GIT_TAG main
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(discord-rpc)
if(IS_DIRECTORY "${discord-rpc_SOURCE_DIR}")
  set_property(
    DIRECTORY ${discord-rpc_SOURCE_DIR}
    PROPERTY EXCLUDE_FROM_ALL YES
)
endif()

# == cURL == #

find_package(CURL REQUIRED)

# == N. Lohmann's C++ JSON library == #

FetchContent_Declare(
  nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# === LINKING === #

target_include_directories(audacious-discord-rpc PRIVATE
  "${discord-rpc_SOURCE_DIR}/include"
  "${nlohmann_json_SOURCE_DIR}/include"
)

target_link_libraries(audacious-discord-rpc
  PRIVATE
    discord-rpc
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

set_target_properties(audacious-discord-rpc
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "discord-rpc"
)

# === INSTALL OPTIONS === #

set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")
install(
  TARGETS audacious-discord-rpc
  LIBRARY DESTINATION ${INSTALL_DIR}
)
