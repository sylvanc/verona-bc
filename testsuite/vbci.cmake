set(TESTSUITE_REGEX ".*/run/exit_code\\.txt$")
set(TESTSUITE_DEFINE vbci_test_define)

function(vbci_test_define test)
  get_filename_component(run_dir "${test}" DIRECTORY)
  get_filename_component(test_root "${run_dir}" DIRECTORY)
  set(compile_golden
    "${CMAKE_CURRENT_SOURCE_DIR}/${test_root}/compile/exit_code.txt")

  if(EXISTS "${compile_golden}")
    file(READ "${compile_golden}" compile_exit_code)
    if(NOT compile_exit_code MATCHES "^0$")
      return()
    endif()
  endif()

  get_filename_component(test_name "${test_root}" NAME)
  set(compile_node "${test_root}/compile")
  set(run_node "${test_root}/run")

  testsuite_output_path(
    bytecode NODE "${compile_node}" FILE "${test_name}.vbc")

  testsuite_add_test(
    NAME "${run_node}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${test_root}"
    DEPENDS "${compile_node}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    COMMAND "${CMAKE_INSTALL_PREFIX}/vbci/vbci" "${bytecode}")
endfunction()
