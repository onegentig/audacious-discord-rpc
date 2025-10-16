cmake_minimum_required(VERSION 2.8.12)
project(audacious-discord-rpc)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FindPkgConfig)
PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=3.9)
add_definitions(${AUDACIOUS_CFLAGS})

include(FetchContent)
FetchContent_Declare(
  discord-rpc
  GIT_REPOSITORY https://github.com/EclipseMenu/discord-presence.git
  GIT_TAG main
  UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(discord-rpc)
if(IS_DIRECTORY "${discord-rpc_SOURCE_DIR}")
  set_property(DIRECTORY ${discord-rpc_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

set(SOURCES src/audacious-discord-rpc.cpp)
add_library(audacious-discord-rpc SHARED ${SOURCES})
target_compile_features(audacious-discord-rpc PUBLIC cxx_std_23)
target_link_libraries(audacious-discord-rpc PRIVATE discord-rpc)
target_include_directories(audacious-discord-rpc PRIVATE "${discord-rpc_SOURCE_DIR}/include")
set_target_properties(audacious-discord-rpc
  PROPERTIES
  PREFIX ""
  OUTPUT_NAME "discord-rpc"
)

PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")
install(TARGETS audacious-discord-rpc LIBRARY DESTINATION ${INSTALL_DIR})
