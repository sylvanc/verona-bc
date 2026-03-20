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

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFFI REQUIRED IMPORTED_TARGET libffi)
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
  GIT_TAG 380e91f5e3378400d28bf7de8a480fd08cb758c0
)

FetchContent_MakeAvailable_ExcludeFromAll(verona-rt)

set(TRIESTE_BUILD_SAMPLES Off)
set(TRIESTE_BUILD_PARSERS Off)

FetchContent_Declare(
  trieste
  GIT_REPOSITORY https://github.com/microsoft/trieste
  GIT_TAG 6c751a5e10ffc85f3650aecc23d23a454be05437
)

FetchContent_MakeAvailable_ExcludeFromAll(trieste)

# This isn't detecting correctly on WSL2.
set(GIT_USE_STAT_MTIM 1)
set(BUILD_TESTS OFF CACHE INTERNAL "Disable libgit2 tests")

FetchContent_Declare(
  libgit2
  GIT_REPOSITORY https://github.com/libgit2/libgit2
  GIT_TAG        0060d9cf5666f015b1067129bd874c6cc4c9c7ac
)

FetchContent_MakeAvailable_ExcludeFromAll(libgit2)

FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd
  GIT_TAG        ae9f20ca2716f2605822ca375995b7d876389b64
)

FetchContent_MakeAvailable_ExcludeFromAll(zstd)

# Use snmalloc clangformat function (verona-rt has a stale copy).
# Override the version to find whatever is installed, and set the include
# patterns to match our source layout.
clangformat_targets(
  VERSION 20
  INCLUDE_PATTERNS vc/*.cc vc/*.h vbcc/*.cc vbcc/*.h vbci/*.cc vbci/*.h include/*.h
)

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
