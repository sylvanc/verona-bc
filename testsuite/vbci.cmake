# Arguments for testing vbci samples
macro(toolinvoke ARGS testfile outputdir)
  get_filename_component(test_root ${WORKING_DIR} DIRECTORY)
  get_filename_component(test_name ${test_root} NAME)
  get_filename_component(build_root ${outputdir} DIRECTORY)
  set(${ARGS} ${build_root}/compile/${test_name}.vbc)
endmacro()

# Run tests are keyed from their golden run directory, not from a committed .vbc.
set(TESTSUITE_REGEX ".*/run/exit_code\\.txt$")

set(TESTSUITE_EXE "${CMAKE_INSTALL_PREFIX}/vbci/vbci")
set(TESTSUITE_RESULT_FILES exit_code.txt stderr.txt stdout.txt)
set(TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REGEX "/run$")
set(TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REPLACE "/compile")
set(TESTSUITE_SKIP_CLEAN_GOLDEN_DIR TRUE)
set(TESTSUITE_REQUIRE_DEPENDENCY_EXIT_CODE_REGEX "^0$")

function (test_output_dir out test)
  get_filename_component(test_dir ${test} DIRECTORY)
  get_filename_component(parent_dir ${test_dir} DIRECTORY)
  set(${out} "${parent_dir}/run" PARENT_SCOPE)
endfunction()
