cmake_minimum_required(VERSION 3.14.0)
project(verona-bc VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SNMALLOC_HEADER_ONLY_LIBRARY OFF CACHE INTERNAL "Snmalloc header library")
set(SNMALLOC_BUILD_TESTING OFF CACHE INTERNAL "Turn off snmalloc tests")
set(VERONA_RT_ONLY_HEADER_LIBRARY OFF CACHE INTERNAL "Verona RT header library")

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
  # GIT_REPOSITORY https://github.com/microsoft/verona-rt
  GIT_REPOSITORY https://github.com/mjp41/verona-rt
  GIT_TAG 8914bf9a6dec841053b82b6afe077fd124f56f0a
  # GIT_TAG 6549f0f7b7b36e9106a0457f3235f74799d9cce4
)

FetchContent_MakeAvailable_ExcludeFromAll(verona-rt)

FetchContent_Declare(
  trieste
  GIT_REPOSITORY https://github.com/microsoft/trieste
  GIT_TAG 1de098a53e82a79c6b6e7143bb92caa83cc128e2
)

FetchContent_MakeAvailable_ExcludeFromAll(trieste)

# Use snmalloc clangformat target
clangformat_targets()

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Installation instructions
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/dist)

add_library(CommonIncludes INTERFACE)
add_library(verona-bc::include ALIAS CommonIncludes)
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
