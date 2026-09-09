set(TESTSUITE_REGEX
  "^vrt/(abi_c\\.c|abi_cxx\\.cc|exit_code_program\\.c|runtime_state\\.cc)$")
set(TESTSUITE_DEFINE vrt_test_define)

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

function(vrt_add_run_node name target)
  testsuite_add_test(
    NAME "${name}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vrt"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    COMMAND "$<TARGET_FILE:${target}>")
endfunction()

function(vrt_add_exit_code_test target test_name)
  add_executable(
    ${target}
    "${CMAKE_CURRENT_SOURCE_DIR}/vrt/exit_code_program.c")
  vrt_c_test_properties(${target})
  target_link_libraries(${target} PRIVATE vbc::vrt)
  vrt_add_run_node("vrt/behavior/${test_name}/run" ${target})
endfunction()

function(vrt_test_define test)
  if(test STREQUAL "vrt/abi_c.c")
    add_executable(vrt_abi_c "${CMAKE_CURRENT_SOURCE_DIR}/${test}")
    vrt_c_test_properties(vrt_abi_c)
    target_link_libraries(vrt_abi_c PRIVATE vbc::vrt)
    vrt_add_run_node("vrt/abi/c/run" vrt_abi_c)
  elseif(test STREQUAL "vrt/abi_cxx.cc")
    add_executable(vrt_abi_cxx "${CMAKE_CURRENT_SOURCE_DIR}/${test}")
    vrt_test_compile_options(vrt_abi_cxx)
    target_link_libraries(vrt_abi_cxx PRIVATE vbc::vrt)
    vrt_add_run_node("vrt/abi/cxx/run" vrt_abi_cxx)
  elseif(test STREQUAL "vrt/exit_code_program.c")
    vrt_add_exit_code_test(vrt_default_exit default-exit)

    vrt_add_exit_code_test(vrt_set_exit set-exit)
    target_compile_definitions(
      vrt_set_exit PRIVATE VRT_TEST_FIRST_EXIT_CODE=7)

    vrt_add_exit_code_test(vrt_last_exit last-write-wins)
    target_compile_definitions(
      vrt_last_exit
      PRIVATE
        VRT_TEST_FIRST_EXIT_CODE=7
        VRT_TEST_LAST_EXIT_CODE=3)
  elseif(test STREQUAL "vrt/runtime_state.cc")
    add_executable(vrt_runtime_state "${CMAKE_CURRENT_SOURCE_DIR}/${test}")
    vrt_test_compile_options(vrt_runtime_state)
    target_include_directories(
      vrt_runtime_state PRIVATE "${PROJECT_SOURCE_DIR}/vrt")
    target_link_libraries(vrt_runtime_state PRIVATE vbc::vrt Threads::Threads)
    vrt_add_run_node("vrt/internal/state/run" vrt_runtime_state)
  else()
    message(FATAL_ERROR "Unexpected VRT test source '${test}'")
  endif()
endfunction()
