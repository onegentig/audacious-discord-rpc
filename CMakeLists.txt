cmake_minimum_required(VERSION 3.10)
project(audacious-discord-rpc VERSION 2.1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  execute_process(COMMAND uname OUTPUT_VARIABLE uname)
  if (NOT (uname MATCHES "^MSYS" OR uname MATCHES "^MINGW"))
    message(FATAL_ERROR
      "Use MINGW64 for Windows compilation."
    )
  endif()
endif()

include(FetchContent)
include(FindPkgConfig)
include(ExternalProject)
find_package(Git)

set(SOURCES src/audacious-discord-rpc.cpp)
add_library(audacious-discord-rpc SHARED ${SOURCES})
target_compile_features(audacious-discord-rpc PUBLIC cxx_std_23)

if(WIN32)
  set(audacious_VERSION "4.5.1")
  set(audacious_ARCHIVE "_deps/audacious.tar.bz2")
  set(audacious_ROOT_DIR "_deps/audacious-src")
  set(audacious_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/audacious-src/src")

  FILE(DOWNLOAD https://distfiles.audacious-media-player.org/audacious-${audacious_VERSION}.tar.bz2
    "${audacious_ARCHIVE}"
    SHOW_PROGRESS
  )
  FILE(ARCHIVE_EXTRACT
    INPUT "${audacious_ARCHIVE}"
    DESTINATION "_deps/audacious"
  )
  FILE(COPY "${CMAKE_BINARY_DIR}/_deps/audacious/audacious-4.5.1/"
    DESTINATION "${audacious_ROOT_DIR}"
    FOLLOW_SYMLINK_CHAIN
  )
  FILE(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/_deps/audacious")
  FILE(REMOVE "${audacious_ARCHIVE}")
  ExternalProject_Add(audacious
    SOURCE_DIR "${CMAKE_BINARY_DIR}/${audacious_ROOT_DIR}"
    CONFIGURE_COMMAND bash -c "./configure --disable-qt --disable-gtk"
    BUILD_COMMAND make
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE TRUE
  )
else()
  PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=4.0)
  PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
endif()

if(WIN32)
  # NOMINMAX is defined by MinGW libstdc++ headers
  # and Discord RPC library attempts a redef in windows.hpp.
  # This is a patch for that.
  add_definitions("-DNOMINMAX")
endif()

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

if(WIN32)
  # Self-built Audacious on Windows
  add_dependencies(audacious-discord-rpc audacious)
  target_include_directories(audacious-discord-rpc PUBLIC "${audacious_SOURCE_DIR}")
  target_link_libraries(audacious-discord-rpc PRIVATE audcore)
  target_link_directories(audacious-discord-rpc PRIVATE
    "${audacious_SOURCE_DIR}/libaudcore"
  )
endif()

target_link_libraries(audacious-discord-rpc PRIVATE discord-rpc)
target_include_directories(audacious-discord-rpc PRIVATE "${discord-rpc_SOURCE_DIR}/include")
set_target_properties(audacious-discord-rpc
  PROPERTIES
  PREFIX ""
  OUTPUT_NAME "discord-rpc"
)

if(WIN32)
  set(INSTALL_DIR "C:/Program Files (x86)/Audacious/lib/audacious/General")
else()
  PKG_GET_VARIABLE(AUDACIOUS_PLUGIN_DIR audacious plugin_dir)
  set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")
endif()

set(INSTALL_DIR "${AUDACIOUS_PLUGIN_DIR}/General")
install(TARGETS audacious-discord-rpc LIBRARY DESTINATION ${INSTALL_DIR})
