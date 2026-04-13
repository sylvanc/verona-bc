include(CheckLinkerFlag)

add_executable(vbci
  callback.cc
  classes.cc
  collect.cc
  drag.cc
  dynlib.cc
  frame.cc
  freeze.cc
  main.cc
  merge.cc
  program.cc
  region.cc
  region_rc.cc
  stack.cc
  thread.cc
  value.cc
  ffi/ffi.cc
)

target_include_directories(vbci SYSTEM PRIVATE
  ${verona-rt_SOURCE_DIR}
  ${snmalloc_SOURCE_DIR}
)

include(CheckCCompilerFlag)

# check if -Wno-class-memaccess is available
check_cxx_compiler_flag("-Wno-class-memaccess" HAS_NO_CLASS_MEMACCESS)
if(HAS_NO_CLASS_MEMACCESS)
  target_compile_options(vbci PRIVATE -Wno-class-memaccess)
endif()

if(MSVC)
  target_compile_options(vbci PRIVATE /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(vbci PRIVATE
    -Wall -Wextra -Werror
    -flto -ffunction-sections -fdata-sections
    -g # For better debugging experience
    )

  target_link_options(vbci PRIVATE -rdynamic -flto)
  check_linker_flag(CXX "-Wl,--gc-sections" LINKER_SUPPORTS_GC_SECTIONS)
  if(LINKER_SUPPORTS_GC_SECTIONS)
    target_link_options(vbci PRIVATE -Wl,--gc-sections)
  endif()
endif()

target_link_libraries(vbci
  Threads::Threads
  snmallocshim-static
  verona_rt
  deps::libffi
  deps::zstd
  CLI11::CLI11
  vbc::include
)

install(TARGETS vbci RUNTIME DESTINATION vbci)
