function(add_vbc_tests)
  # Use Trieste to test the Verona bytecode compiler and interpreter.
  testsuite(
    vbc
    vc.cmake
    vbcc.cmake
    vbci.cmake)
endfunction()
