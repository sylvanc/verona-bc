function(add_llvm_native_test name vir expected_exit)
  add_test(
    NAME llvm/${name}/native
    COMMAND
      ${CMAKE_COMMAND}
      -DVBCC=${CMAKE_INSTALL_PREFIX}/vbcc/$<TARGET_FILE_NAME:vbcc>
      -DVIR=${CMAKE_CURRENT_SOURCE_DIR}/${vir}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/llvm/${name}
      -DLLVM_AS=${VERONA_LLVM_AS}
      -DLLC=${VERONA_LLC}
      -DCXX=${CMAKE_CXX_COMPILER}
      -DMSVC=${MSVC}
      -DNATIVE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
      -DOBJECT_SUFFIX=${CMAKE_CXX_OUTPUT_EXTENSION}
      -DRUNTIME=${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:libvbcrt>
      -DEXPECTED_EXIT=${expected_exit}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/llvm_native_test.cmake)
  set_tests_properties(
    llvm/${name}/native
    PROPERTIES
      LABELS llvm
      TIMEOUT 60)
endfunction()

function(add_llvm_tests)
  find_program(VERONA_LLVM_AS
    NAMES llvm-as
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH)
  find_program(VERONA_LLC
    NAMES llc
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH)

  if(NOT VERONA_LLVM_AS OR NOT VERONA_LLC)
    message(FATAL_ERROR "LLVM backend tests require llvm-as and llc")
  endif()

  # Add all LLVM parsable tests to the test suite. These are tests that can be compiled to LLVM IR and then to native code.
  add_llvm_native_test(simp1 vir/simp1/simp1.vir 0)
endfunction()
