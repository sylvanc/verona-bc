include("${trieste_SOURCE_DIR}/cmake/testsuite.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/fixture_metadata.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/compiler_fixtures.cmake")

verona_validate_compiler_fixtures()

# Compiler collections share the Verona and textual-VIR fixture tree. Explicit
# ownership prevents one suite from registering another suite's pipelines.
testsuite(
	compiler
	COLLECTIONS
		vc-vbc.cmake
		vc-llvm.cmake
		virc-vbc.cmake
		virc-llvm.cmake)

# Runtime collections execute hand-written C/C++ fixtures directly against VRT
# and intentionally bypass compiler-generated code and metadata.
testsuite(runtime COLLECTIONS vrt.cmake)

add_test(
	NAME compiler/virc/vbcc-compat
	COMMAND "${CMAKE_INSTALL_PREFIX}/vbcc/$<TARGET_FILE_NAME:vbcc>" --help)
