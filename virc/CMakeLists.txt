add_library(virc_core STATIC
  analysis/bitset.cc
  model/compilation.cc
  model/name_table.cc
  model/type_table.cc
  passes/assign_ids.cc
  passes/liveness.cc
  passes/memo.cc
  passes/optimize.cc
  passes/typecheck.cc
  passes/validate_ids.cc
  support/diagnostics.cc
  support/literals.cc
  compile.cc
)

target_include_directories(virc_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(virc_core PUBLIC
  Threads::Threads
  snmalloc-new-override
  trieste::trieste
  vbc::include
)

add_subdirectory(vbc)

add_library(virc_reader STATIC
  reader/labels.cc
  reader/parser.cc
  reader/statements.cc
)
target_link_libraries(virc_reader PUBLIC virc_core)

# Build-tree migration aliases for the former backend target names. These
# expose virc_core only; consumers that emit VBC must also link virc_vbc.
add_library(libvbcc ALIAS virc_core)
add_library(vbc::vbcc ALIAS virc_core)

if(VERONA_ENABLE_LLVM_BACKEND)
  add_subdirectory(llvm)
endif()

# Preserve the former command name during migration. Both executables use the
# same textual-VIR reader and output backends.
add_executable(virc main.cc)
add_executable(vbcc main.cc)

if(MSVC)
  target_compile_options(virc_core PUBLIC /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(virc_core PUBLIC
    -Wall -Wextra -Werror -Wno-zero-length-array)
endif()

foreach(tool IN ITEMS virc vbcc)
  target_link_libraries(${tool}
    CLI11::CLI11
    virc_reader
    virc_vbc
  )

  if(VERONA_ENABLE_LLVM_BACKEND)
    target_compile_definitions(${tool} PRIVATE VERONA_ENABLE_LLVM_BACKEND=1)
    target_link_libraries(${tool} virc_llvm)
  endif()
endforeach()

install(TARGETS virc_core virc_reader
  ARCHIVE DESTINATION lib/virc)
install(TARGETS virc RUNTIME DESTINATION virc)
install(TARGETS vbcc RUNTIME DESTINATION vbcc)
