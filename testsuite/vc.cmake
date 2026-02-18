# Arguments for testing vc compile
macro(toolinvoke ARGS testfile outputdir)
  get_filename_component(test_name ${testfile} NAME_WE)
  set(${ARGS} build . -b ${outputdir}/${test_name}.vbc -o ${outputdir}/${test_name}_final.trieste --dump_passes ${outputdir})
endmacro()

# Regular expression to match test files
set(TESTSUITE_REGEX ".*\\.v$")

# Use the installed binary which has _builtin next to it
set(TESTSUITE_EXE "${CMAKE_INSTALL_PREFIX}/vc/vc")

function (test_output_dir out test)
  get_filename_component(test_dir ${test} DIRECTORY)
  get_filename_component(test_name ${test} NAME_WE)
  set(${out} "${test_dir}/${test_name}/compile" PARENT_SCOPE)
endfunction()
