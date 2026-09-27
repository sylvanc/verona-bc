# Compiler fixture feasibility is layered from broad defaults to stable path
# conventions, explicit source groups, and exact exceptions.

verona_fixture_defaults(
	VBC_STAGE run
	LLVM_STAGE none)

verona_fixture_rule(
	MATCH "^(v|vir)/compile_only/"
	VBC_STAGE compile)

verona_fixture_group(
	SOURCES
		v/hello/hello.v
		vir/llvm_array_alloc/llvm_array_alloc.vir
		vir/llvm_array_bulk/llvm_array_bulk.vir
		vir/llvm_class_metadata/llvm_class_metadata.vir
		vir/llvm_constants/llvm_constants.vir
		vir/llvm_control_flow/llvm_control_flow.vir
		vir/llvm_control_flow_join/llvm_control_flow_join.vir
		vir/llvm_convert/llvm_convert.vir
		vir/llvm_copy_move_drop/llvm_copy_move_drop.vir
		vir/llvm_freeze/llvm_freeze.vir
		vir/llvm_library_merge/llvm_library_merge.vir
		vir/llvm_object_alloc/llvm_object_alloc.vir
		vir/llvm_raise/llvm_raise.vir
		vir/llvm_reference/llvm_reference.vir
		vir/llvm_scalar_ops/llvm_scalar_ops.vir
		vir/llvm_singleton_init/llvm_singleton_init.vir
		vir/llvm_tailcall_raise_frame/llvm_tailcall_raise_frame.vir
		vir/llvm_tailcalls/llvm_tailcalls.vir
	LLVM_STAGE run)

verona_fixture(
	SOURCE vir/llvm_dynamic_dispatch/llvm_dynamic_dispatch.vir
	LLVM_STAGE run
	LLVM_VALIDATOR llvm/cmake/validate_dynamic_dispatch_switch.cmake)

verona_fixture(
	SOURCE vir/llvm_dynamic_dispatch_fallback/llvm_dynamic_dispatch_fallback.vir
	LLVM_STAGE run
	LLVM_VALIDATOR llvm/cmake/validate_dynamic_dispatch_fallback.cmake)

verona_fixture(
	SOURCE vir/vrt_finalizer/vrt_finalizer.vir
	VBC_STAGE compile
	LLVM_STAGE run
	LABELS runtime:vrt)

verona_fixture(
	SOURCE vir/vrt_singleton_alloc_error/vrt_singleton_alloc_error.vir
	VBC_STAGE compile
	LLVM_STAGE run
	LABELS runtime:vrt)
