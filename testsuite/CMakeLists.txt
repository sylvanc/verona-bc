include("${trieste_SOURCE_DIR}/cmake/testsuite.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/fixture_metadata.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/compiler_fixtures.cmake")

verona_validate_compiler_fixtures()

# The VBC architecture branch owns only currently available compiler
# collections. Later backend collections can join this suite explicitly.
testsuite(
	compiler
	COLLECTIONS
		vc-vbc.cmake
		virc-vbc.cmake)
