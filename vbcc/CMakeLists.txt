add_library(libvbcc STATIC
  passes/assignids.cc
  passes/liveness.cc
  passes/validids.cc
  bitset.cc
  bytecode.cc
  lang.cc
  stringtable.cc
)

target_include_directories(libvbcc INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(libvbcc PUBLIC
  Threads::Threads
  snmalloc-new-override
  trieste::trieste
  vbc::include
  libzstd_static
)

add_library(vbc::vbcc ALIAS libvbcc)

add_executable(vbcc
  passes/labels.cc
  passes/parser.cc
  passes/statements.cc
  main.cc
)

if(MSVC)
  target_compile_options(libvbcc PUBLIC /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(libvbcc PUBLIC
    -Wall -Wextra -Werror -Wno-zero-length-array)
endif()

target_link_libraries(vbcc
  vbc::vbcc
  CLI11::CLI11
)

install(TARGETS libvbcc RUNTIME DESTINATION libvbcc)
install(TARGETS vbcc RUNTIME DESTINATION vbcc)
