add_executable(vbci
  dynlib.cc
  frame.cc
  main.cc
  program.cc
  region.cc
  thread.cc
  types.cc
  value.cc
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
    -flto -Wl,--gc-sections
    )
endif()

target_link_libraries(vbci
  Threads::Threads
  snmalloc-new-override
  verona_rt
  ffi::ffi
  CLI11::CLI11
  verona-bc::include
)

install(TARGETS vbci RUNTIME DESTINATION vbci)
