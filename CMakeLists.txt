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

find_package(OpenSSL REQUIRED)
add_library(deps::libffi INTERFACE IMPORTED)
add_library(deps::libgit2 INTERFACE IMPORTED)
add_library(deps::zstd INTERFACE IMPORTED)

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
  pkg_check_modules(LIBFFI REQUIRED IMPORTED_TARGET libffi)
  target_link_libraries(deps::libffi INTERFACE PkgConfig::LIBFFI)

  pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)
  target_link_libraries(deps::libgit2 INTERFACE PkgConfig::LIBGIT2)

  pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
  target_link_libraries(deps::zstd INTERFACE PkgConfig::ZSTD)
else()
  find_package(libffi REQUIRED CONFIG)
  target_link_libraries(deps::libffi INTERFACE libffi::libffi)

  find_package(libgit2 REQUIRED CONFIG)
  target_link_libraries(deps::libgit2 INTERFACE libgit2::libgit2)

  find_package(zstd REQUIRED CONFIG)
  target_link_libraries(deps::zstd INTERFACE zstd::libzstd)
endif()

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
  GIT_TAG 2818ab9ccc4e10b95782162a3da26f122851de33
)

FetchContent_MakeAvailable_ExcludeFromAll(trieste)

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
