llvm_map_components_to_libnames(VERONA_LLVM_LIBS Core Support native)

add_library(libvbcllvm STATIC
  blocks.cc
  codegen.cc
  functions/declaration.cc
  functions/descriptors.cc
  functions/emission.cc
  functions/params.cc
  libs/declaration.cc
  libs/initializer.cc
  literals.cc
  literals/integer.cc
  literals/literal.cc
  locals.cc
  module.cc
  ownership.cc
  program.cc
  vrt/declaration.cc
  vrt/frame.cc
  vrt/raise.cc
  vrt/tailcall.cc
  statements.cc
  statements/args.cc
  statements/binop.cc
  statements/call.cc
  statements/constant.cc
  statements/convert.cc
  statements/copy.cc
  statements/drop.cc
  statements/ffi.cc
  statements/move.cc
  statements/raise_target.cc
  statements/unop.cc
  terminators.cc
  terminators/cond.cc
  terminators/jump.cc
  terminators/raise.cc
  terminators/return.cc
  terminators/tailcall.cc
  terminators/tailcall_dyn.cc
  types.cc
  types/array.cc
  types/builtin.cc
  types/class_id.cc
  types/cown.cc
  types/dyn.cc
  types/float.cc
  types/integer.cc
  types/primitive.cc
  types/ref.cc
  types/tuple_type.cc
  types/type_id.cc
  types/union.cc
  utilities.cc
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
