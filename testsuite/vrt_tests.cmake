function(vrt_test_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(
      ${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endfunction()

function(vrt_c_test_properties target)
  set_target_properties(
    ${target}
    PROPERTIES
      C_EXTENSIONS OFF
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      LINKER_LANGUAGE CXX)
  vrt_test_compile_options(${target})
endfunction()

function(add_vrt_exit_code_test target test_name expected_exit)
  add_executable(
    ${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/vrt/exit_code_program.c)
  vrt_c_test_properties(${target})
  target_link_libraries(${target} PRIVATE vbc::vrt)

  add_test(
    NAME vrt/behavior/${test_name}
    COMMAND
      ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:${target}>
      -DEXPECTED_EXIT=${expected_exit}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/vrt/cmake/check_exit_code.cmake)
  set_tests_properties(
    vrt/behavior/${test_name}
    PROPERTIES
      LABELS vrt
      TIMEOUT 20)
endfunction()

function(add_vrt_tests)
  add_executable(
    vrt_abi_c
    ${CMAKE_CURRENT_SOURCE_DIR}/vrt/abi_c.c)
  vrt_c_test_properties(vrt_abi_c)
  target_link_libraries(vrt_abi_c PRIVATE vbc::vrt)

  add_executable(
    vrt_abi_cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vrt/abi_cxx.cc)
  vrt_test_compile_options(vrt_abi_cxx)
  target_link_libraries(vrt_abi_cxx PRIVATE vbc::vrt)

  add_test(NAME vrt/abi/c COMMAND vrt_abi_c)
  set_tests_properties(vrt/abi/c PROPERTIES LABELS vrt)

  add_test(NAME vrt/abi/cxx COMMAND vrt_abi_cxx)
  set_tests_properties(vrt/abi/cxx PROPERTIES LABELS vrt)

  add_vrt_exit_code_test(vrt_default_exit default-exit 0)

  add_vrt_exit_code_test(vrt_set_exit set-exit 7)
  target_compile_definitions(
    vrt_set_exit PRIVATE VRT_TEST_FIRST_EXIT_CODE=7)

  add_vrt_exit_code_test(vrt_last_exit last-write-wins 3)
  target_compile_definitions(
    vrt_last_exit
    PRIVATE
      VRT_TEST_FIRST_EXIT_CODE=7
      VRT_TEST_LAST_EXIT_CODE=3)

  add_executable(
    vrt_runtime_state
    ${CMAKE_CURRENT_SOURCE_DIR}/vrt/runtime_state.cc)
  vrt_test_compile_options(vrt_runtime_state)
  target_include_directories(
    vrt_runtime_state PRIVATE ${PROJECT_SOURCE_DIR}/runtime)
  target_link_libraries(vrt_runtime_state PRIVATE vbc::vrt)

  add_test(NAME vrt/internal/state COMMAND vrt_runtime_state)
  set_tests_properties(vrt/internal/state PROPERTIES LABELS vrt)
endfunction()
