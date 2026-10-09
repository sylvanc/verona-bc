set(TESTSUITE_REGEX ".*\\.vir$")
set(TESTSUITE_DEFINE vir_test_define)

function(vir_test_define test)
  get_filename_component(test_dir "${test}" DIRECTORY)
  get_filename_component(test_file "${test}" NAME)
  get_filename_component(test_name "${test}" NAME_WE)
  set(test_root "${test_dir}/${test_name}")
  set(compile_node "${test_root}/compile")
  set(run_node "${test_root}/run")

  verona_fixture_metadata("${test}" vbc_stage)
  if(vbc_stage STREQUAL "none")
    return()
  endif()
  set(pipeline_labels frontend:virc backend:vbc)

  testsuite_output_path(
    bytecode NODE "${compile_node}" FILE "${test_name}.vbc")
  testsuite_output_path(
    final_ast NODE "${compile_node}" FILE "${test_name}_final.trieste")

  set(artifact_metadata)
  if(vbc_stage STREQUAL "run")
    set(artifact_metadata ARTIFACTS "${test_name}.vbc")
  endif()

  testsuite_add_test(
    NAME "${compile_node}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ${artifact_metadata}
    LABELS ${pipeline_labels}
    COMMAND
      "${CMAKE_INSTALL_PREFIX}/virc/$<TARGET_FILE_NAME:virc>"
      build "${test_file}"
      -b "${bytecode}"
      -o "${final_ast}")

  if(vbc_stage STREQUAL "run")
    testsuite_add_test(
      NAME "${run_node}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${test_root}"
      DEPENDS "${compile_node}"
      GOLDENS exit_code.txt stderr.txt stdout.txt
      LABELS ${pipeline_labels}
      COMMAND "${CMAKE_INSTALL_PREFIX}/vbci/vbci" "${bytecode}")
  endif()
endfunction()
