add_library(virc_core STATIC
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
)

target_include_directories(virc_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(virc_core PUBLIC
  Threads::Threads
  snmalloc-new-override
  trieste::trieste
  verona::include
)

add_subdirectory(vbc)

add_library(virc_reader STATIC
  reader/labels.cc
  reader/parser.cc
  reader/statements.cc
)
target_link_libraries(virc_reader PUBLIC virc_core)

add_library(libvbcc ALIAS virc_core)
add_library(vbc::vbcc ALIAS virc_core)

add_executable(vbcc
  main.cc
)

if(MSVC)
  target_compile_options(virc_core PUBLIC /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(virc_core PUBLIC
    -Wall -Wextra -Werror -Wno-zero-length-array)
endif()

target_link_libraries(vbcc
  CLI11::CLI11
  virc_reader
  virc_vbc
)

install(TARGETS virc_core virc_reader ARCHIVE DESTINATION lib/virc)
install(TARGETS vbcc RUNTIME DESTINATION vbcc)
