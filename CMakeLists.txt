cmake_minimum_required(VERSION 2.8.12)
project(audacious-discord-rpc)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
include(FindPkgConfig)
PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=3.9)
add_definitions(${AUDACIOUS_CFLAGS})
find_package(CURL REQUIRED)

FetchContent_Declare(
  discord-rpc
  GIT_REPOSITORY https://github.com/EclipseMenu/discord-presence.git
  GIT_TAG main
  GIT_PROGRESS TRUE
  UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(discord-rpc)
if(IS_DIRECTORY "${discord-rpc_SOURCE_DIR}")
  set_property(DIRECTORY ${discord-rpc_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(SOURCES src/audacious-discord-rpc.cpp)
add_library(audacious-discord-rpc SHARED ${SOURCES})
target_compile_features(audacious-discord-rpc PUBLIC cxx_std_23)
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

PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")
install(TARGETS audacious-discord-rpc LIBRARY DESTINATION ${INSTALL_DIR})
