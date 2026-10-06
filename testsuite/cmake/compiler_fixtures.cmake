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
		vir/array_alloc/array_alloc.vir
		vir/array_bulk/array_bulk.vir
		vir/class_metadata/class_metadata.vir
		vir/constants/constants.vir
		vir/control_flow/control_flow.vir
		vir/control_flow_join/control_flow_join.vir
		vir/convert/convert.vir
		vir/copy_move_drop/copy_move_drop.vir
		vir/freeze_ownership/freeze_ownership.vir
		vir/library_merge/library_merge.vir
		vir/merge/merge.vir
		vir/object_alloc/object_alloc.vir
		vir/pin_unpin/pin_unpin.vir
		vir/raise/raise.vir
		vir/reference/reference.vir
		vir/scalar_ops/scalar_ops.vir
		vir/singleton_init/singleton_init.vir
		vir/stack_alloc/stack_alloc.vir
		vir/stack_tailcall_escape/stack_tailcall_escape.vir
		vir/tailcall_raise_frame/tailcall_raise_frame.vir
		vir/tailcalls/tailcalls.vir
	LLVM_STAGE run)

verona_fixture_group(
	SOURCES
		vir/region_empty/region_empty.vir
	LLVM_STAGE run
	LABELS runtime:vrt)

verona_fixture(
	SOURCE vir/dynamic_dispatch/dynamic_dispatch.vir
	LLVM_STAGE run
	LLVM_VALIDATOR llvm/cmake/validate_dynamic_dispatch_switch.cmake)

verona_fixture(
	SOURCE vir/dynamic_dispatch_fallback/dynamic_dispatch_fallback.vir
	LLVM_STAGE run
	LLVM_VALIDATOR llvm/cmake/validate_dynamic_dispatch_fallback.cmake)

verona_fixture(
	SOURCE vir/finalizer/finalizer.vir
	VBC_STAGE compile
	LLVM_STAGE run
	LABELS runtime:vrt)

verona_fixture(
	SOURCE vir/singleton_alloc_error/singleton_alloc_error.vir
	VBC_STAGE compile
	LLVM_STAGE run
	LABELS runtime:vrt)
