include(${CMAKE_CURRENT_SOURCE_DIR}/verona_testsuite.cmake)

# Use Trieste to test Verona bytecode tools (vbcc and vbci)
testsuite(vbc)
