set(TESTSUITE_REGEX ".*\\.vir$")
set(TESTSUITE_DEFINE vbcc_test_define)

function(vbcc_test_define test)
  get_filename_component(test_dir "${test}" DIRECTORY)
  get_filename_component(test_file "${test}" NAME)
  get_filename_component(test_name "${test}" NAME_WE)
  set(test_root "${test_dir}/${test_name}")
  set(node "${test_root}/compile")

  testsuite_output_path(bytecode NODE "${node}" FILE "${test_name}.vbc")
  testsuite_output_path(
    final_ast NODE "${node}" FILE "${test_name}_final.trieste")

  set(artifacts)
  set(run_golden "${CMAKE_CURRENT_SOURCE_DIR}/${test_root}/run/exit_code.txt")
  set(compile_golden
    "${CMAKE_CURRENT_SOURCE_DIR}/${node}/exit_code.txt")
  if(EXISTS "${run_golden}")
    if(EXISTS "${compile_golden}")
      file(READ "${compile_golden}" compile_exit_code)
    endif()
    if(NOT DEFINED compile_exit_code OR compile_exit_code MATCHES "^0$")
      list(APPEND artifacts "${test_name}.vbc")
    endif()
  endif()
  set(artifact_metadata)
  if(artifacts)
    set(artifact_metadata ARTIFACTS ${artifacts})
  endif()

  testsuite_add_test(
    NAME "${node}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ${artifact_metadata}
    COMMAND
      "${CMAKE_INSTALL_PREFIX}/vbcc/vbcc"
      build "${test_file}"
      -b "${bytecode}"
      -o "${final_ast}")
endfunction()
