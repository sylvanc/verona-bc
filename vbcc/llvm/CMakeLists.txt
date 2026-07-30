llvm_map_components_to_libnames(VERONA_LLVM_LIBS Core Support)

add_library(libvbcllvm STATIC
  declarations.cc
  entry.cc
  ffi.cc
  functions.cc
  lifetime.cc
  module.cc
  scalar.cc
  types.cc
)

target_include_directories(libvbcllvm SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
)

target_compile_options(libvbcllvm PRIVATE
  ${LLVM_DEFINITIONS}
)

target_link_libraries(libvbcllvm PUBLIC
  vbc::vbcc
  ${VERONA_LLVM_LIBS}
)

add_library(vbc::vbcllvm ALIAS libvbcllvm)
