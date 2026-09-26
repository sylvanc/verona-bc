add_library(libvbcc STATIC
  compile.cc
  passes/assign_ids.cc
  passes/liveness.cc
  passes/memo.cc
  passes/optimize.cc
  passes/typecheck.cc
  passes/validate_ids.cc
  analysis/bitset.cc
  model/compilation.cc
  model/name_table.cc
  model/type_table.cc
  support/diagnostics.cc
  support/literals.cc
  vbc/emitter.cc
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
  deps::zstd
)

add_library(vbc::vbcc ALIAS libvbcc)

add_executable(vbcc
  reader/labels.cc
  reader/parser.cc
  reader/statements.cc
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
