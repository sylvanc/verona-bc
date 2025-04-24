include(FetchContent)
include(ExternalProject)

# 1. Try a system / vcpkg / pkg‑config installation first
find_path(LIBFFI_INCLUDE_DIR ffi.h)
find_library(LIBFFI_LIBRARY NAMES ffi libffi)

if(LIBFFI_INCLUDE_DIR AND LIBFFI_LIBRARY)
  add_library(ffi::ffi UNKNOWN IMPORTED)
  set_target_properties(ffi::ffi PROPERTIES
    IMPORTED_LOCATION "${LIBFFI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFFI_INCLUDE_DIR}")
  message(STATUS "Using system libffi")
else()
  # 2. Build from source with ExternalProject
  set(LIBFFI_PREFIX ${CMAKE_BINARY_DIR}/libffi-prefix)
  set(LIBFFI_INSTALL ${LIBFFI_PREFIX}/install)

  ExternalProject_Add(libffi-ext
    URL https://github.com/libffi/libffi/releases/download/v3.4.8/libffi-3.4.8.tar.gz
    URL_HASH SHA256=15fd7b49c1996c7791073aa4e137c55169565b31ca167fca96a1e99e1436eb49
    # ---- Unix build ---------------------------------------------------------
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env
      CC=${CMAKE_C_COMPILER}
      ./configure --disable-shared --enable-static --prefix=${LIBFFI_INSTALL}
    BUILD_COMMAND make -j${CMAKE_JOB_POOLS_NUMBER}
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
    # ---- Windows / MSVC build ----------------------------------------------
    # Autotools doesn't work with MSVC; instead call the bundled MSVC project
    # when MSVC is the generator.
    ${MSVC}
  )

  add_library(ffi::ffi STATIC IMPORTED)
  set_target_properties(ffi::ffi PROPERTIES
    IMPORTED_LOCATION "${LIBFFI_INSTALL}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ffi${CMAKE_STATIC_LIBRARY_SUFFIX}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFFI_INSTALL}/include")

  add_dependencies(ffi::ffi libffi-ext)
  message(STATUS "Building libffi from source")
endif()
