llvm_map_components_to_libnames(VERONA_LLVM_LIBS Core Support native)

add_library(virc_llvm STATIC
  classes.cc
  codegen.cc
  # Shared call lowering.
  calls/callable.cc
  # Call statements.
  calls/call.cc
  calls/call_dyn.cc
  calls/lookup.cc
  # Call terminators.
  calls/tailcall.cc
  calls/tailcall_dyn.cc
  functions/declaration.cc
  functions/descriptors.cc
  functions/emission.cc
  functions/params.cc
  libs/declaration.cc
  libs/initializer.cc
  literals.cc
  literals/integer.cc
  literals/literal.cc
  model/blocks.cc
  model/locals.cc
  module.cc
  ownership.cc
  program.cc
  vrt/declaration.cc
  vrt/frame.cc
  vrt/raise.cc
  vrt/tailcall.cc
  statements.cc
  statements/args.cc
  # Allocation statements.
  statements/allocation/array.cc
  statements/allocation/heap.cc
  statements/allocation/heap_array.cc
  statements/allocation/new.cc
  statements/allocation/new_array.cc
  statements/allocation/new_array_const.cc
  statements/allocation/object.cc
  statements/allocation/region.cc
  statements/allocation/region_array.cc
  statements/allocation/singleton.cc
  statements/allocation/stack.cc
  statements/allocation/stack_array.cc
  # Callback statements.
  statements/callbacks/addexternal.cc
  statements/callbacks/removeexternal.cc
  # Constant statements.
  statements/constants/constant.cc
  statements/constants/e.cc
  statements/constants/inf.cc
  statements/constants/nan.cc
  statements/constants/pi.cc
  statements/arraycompare.cc
  statements/arraycopy.cc
  statements/arrayfill.cc
  statements/array_ref.cc
  statements/array_ref_const.cc
  statements/binop.cc
  statements/convert.cc
  statements/copy.cc
  statements/drop.cc
  statements/ffi.cc
  statements/freeze.cc
  statements/field_ref.cc
  statements/get_raise.cc
  statements/load.cc
  statements/move.cc
  statements/merge.cc
  statements/pin.cc
  statements/register_ref.cc
  statements/set_raise.cc
  statements/store.cc
  statements/unop.cc
  terminators.cc
  terminators/cond.cc
  terminators/jump.cc
  terminators/raise.cc
  terminators/return.cc
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

target_include_directories(virc_llvm SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
)

separate_arguments(
  VERONA_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(virc_llvm PRIVATE
  ${VERONA_LLVM_DEFINITIONS}
)

target_link_libraries(virc_llvm PUBLIC
  virc_core
  ${VERONA_LLVM_LIBS}
)

add_library(libvbcllvm ALIAS virc_llvm)
add_library(vbc::vbcllvm ALIAS virc_llvm)

install(TARGETS virc_llvm ARCHIVE DESTINATION lib/virc)
