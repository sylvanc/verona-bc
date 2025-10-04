cmake_minimum_required(VERSION 3.14.0)
project(vbc VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SNMALLOC_HEADER_ONLY_LIBRARY OFF CACHE INTERNAL "Snmalloc header library")
set(SNMALLOC_BUILD_TESTING OFF CACHE INTERNAL "Turn off snmalloc tests")
set(SNMALLOC_STATIC_LIBRARY_PREFIX OFF CACHE INTERNAL "")
set(VERONA_RT_ONLY_HEADER_LIBRARY OFF CACHE INTERNAL "Verona RT header library")

include(cmake/libffi.cmake)
find_package(OpenSSL REQUIRED)

# Used to provide
#  FetchContent_MakeAvailable_ExcludeFromAll
FetchContent_Declare(
  cmake_utils
  GIT_REPOSITORY https://github.com/mjp41/cmake_utils
  GIT_TAG 28490f85c91053dc501f96a7f8224e8191156ec9
)

FetchContent_MakeAvailable(cmake_utils)

FetchContent_Declare(
  verona-rt
  GIT_REPOSITORY https://github.com/microsoft/verona-rt
  GIT_TAG a4b5a71826b5e3f4e7c1a3be9e26e7219678097d
)

FetchContent_MakeAvailable_ExcludeFromAll(verona-rt)

set(TRIESTE_BUILD_SAMPLES Off)
set(TRIESTE_BUILD_PARSERS Off)

FetchContent_Declare(
  trieste
  GIT_REPOSITORY https://github.com/microsoft/trieste
  GIT_TAG 18022133857a08e8325d3d4d879dddd99ffe537c
)

FetchContent_MakeAvailable_ExcludeFromAll(trieste)

FetchContent_Declare(
  libuv
  GIT_REPOSITORY https://github.com/libuv/libuv
  GIT_TAG        v1.51.0
)

FetchContent_MakeAvailable_ExcludeFromAll(libuv)

# This isn't detecting correctly on WSL2.
set(GIT_USE_STAT_MTIM 1)

FetchContent_Declare(
  libgit2
  GIT_REPOSITORY https://github.com/libgit2/libgit2
  GIT_TAG        0060d9cf5666f015b1067129bd874c6cc4c9c7ac
)

FetchContent_MakeAvailable_ExcludeFromAll(libgit2)

# Use snmalloc clangformat target
clangformat_targets()

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Installation instructions
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/dist)

add_library(CommonIncludes INTERFACE)
add_library(vbc::include ALIAS CommonIncludes)
target_include_directories(CommonIncludes INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Clear all existing files and folders from the install directory
install(CODE [[
  file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/.)
  ]])

add_subdirectory(vbcc)
add_subdirectory(vbci)
add_subdirectory(vc)
enable_testing()
add_subdirectory(testsuite)