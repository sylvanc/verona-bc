include("${trieste_SOURCE_DIR}/cmake/testsuite.cmake")

# The VBC architecture branch owns only currently available compiler
# collections. Later backend collections can join this suite explicitly.
testsuite(
	compiler
	COLLECTIONS
		vc-vbc.cmake
		virc-vbc.cmake)
