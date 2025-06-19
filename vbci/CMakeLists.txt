add_executable(vbci
  classes.cc
  dynlib.cc
  frame.cc
  freeze.cc
  location.cc
  main.cc
  program.cc
  region.cc
  region_rc.cc
  thread.cc
  types.cc
  value.cc
  ffi/eyeballs.cc
  ffi/ffi.cc
  ffi/ssl.cc
)

target_include_directories(vbci SYSTEM PRIVATE
  ${verona-rt_SOURCE_DIR}
  ${snmalloc_SOURCE_DIR}
)

if(MSVC)
  target_compile_options(vbci PRIVATE /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(vbci PRIVATE
    -Wall -Wextra -Werror -Wshadow
    -flto -ffunction-sections -fdata-sections
    )
  target_link_options(vbci PRIVATE
    -rdynamic -flto -Wl,--gc-sections
    )
endif()

target_link_libraries(vbci
  Threads::Threads
  snmallocshim-static
  verona_rt
  uv_a
  OpenSSL::Crypto
  OpenSSL::SSL
  ffi::ffi
  CLI11::CLI11
  vbc::include
)

install(TARGETS vbci RUNTIME DESTINATION vbci)
