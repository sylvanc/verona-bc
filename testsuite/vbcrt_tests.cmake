function(vbcrt_test_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(
      ${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endfunction()

function(vbcrt_c_test_properties target)
  set_target_properties(
    ${target}
    PROPERTIES
      C_EXTENSIONS OFF
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      LINKER_LANGUAGE CXX)
  vbcrt_test_compile_options(${target})
endfunction()

function(add_vbcrt_exit_code_test target test_name expected_exit)
  add_executable(
    ${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/vbcrt/exit_code_program.c)
  vbcrt_c_test_properties(${target})
  target_link_libraries(${target} PRIVATE vbc::vbcrt)

  add_test(
    NAME vbcrt/behavior/${test_name}
    COMMAND
      ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:${target}>
      -DEXPECTED_EXIT=${expected_exit}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/vbcrt/cmake/check_exit_code.cmake)
  set_tests_properties(
    vbcrt/behavior/${test_name}
    PROPERTIES
      LABELS vbcrt
      TIMEOUT 20)
endfunction()

function(add_vbcrt_tests)
  add_executable(
    vbcrt_abi_c
    ${CMAKE_CURRENT_SOURCE_DIR}/vbcrt/abi_c.c)
  vbcrt_c_test_properties(vbcrt_abi_c)
  target_link_libraries(vbcrt_abi_c PRIVATE vbc::vbcrt)

  add_executable(
    vbcrt_abi_cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vbcrt/abi_cxx.cc)
  vbcrt_test_compile_options(vbcrt_abi_cxx)
  target_link_libraries(vbcrt_abi_cxx PRIVATE vbc::vbcrt)

  add_test(NAME vbcrt/abi/c COMMAND vbcrt_abi_c)
  set_tests_properties(vbcrt/abi/c PROPERTIES LABELS vbcrt)

  add_test(NAME vbcrt/abi/cxx COMMAND vbcrt_abi_cxx)
  set_tests_properties(vbcrt/abi/cxx PROPERTIES LABELS vbcrt)

  add_vbcrt_exit_code_test(vbcrt_default_exit default-exit 0)

  add_vbcrt_exit_code_test(vbcrt_set_exit set-exit 7)
  target_compile_definitions(
    vbcrt_set_exit PRIVATE VBCRT_TEST_FIRST_EXIT_CODE=7)

  add_vbcrt_exit_code_test(vbcrt_last_exit last-write-wins 3)
  target_compile_definitions(
    vbcrt_last_exit
    PRIVATE
      VBCRT_TEST_FIRST_EXIT_CODE=7
      VBCRT_TEST_LAST_EXIT_CODE=3)

  add_executable(
    vbcrt_runtime_state
    ${CMAKE_CURRENT_SOURCE_DIR}/vbcrt/runtime_state.cc)
  vbcrt_test_compile_options(vbcrt_runtime_state)
  target_include_directories(
    vbcrt_runtime_state PRIVATE ${PROJECT_SOURCE_DIR}/runtime)
  target_link_libraries(vbcrt_runtime_state PRIVATE vbc::vbcrt)

  add_test(NAME vbcrt/internal/state COMMAND vbcrt_runtime_state)
  set_tests_properties(vbcrt/internal/state PROPERTIES LABELS vbcrt)
endfunction()
