include(${CMAKE_CURRENT_SOURCE_DIR}/verona_testsuite.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/vbc_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/vbcrt_tests.cmake)

add_vbc_tests()
add_vbcrt_tests()

if(VERONA_ENABLE_LLVM_BACKEND)
  include(${CMAKE_CURRENT_SOURCE_DIR}/llvm_tests.cmake)
  add_llvm_tests()
endif()
