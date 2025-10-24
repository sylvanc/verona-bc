# Arguments for testing vbcc samples
macro(toolinvoke ARGS testfile outputdir)
  get_filename_component(test_name ${testfile} NAME_WE)
  set(${ARGS} build ${testfile} -b ${outputdir}/${test_name}.vbc -r -o ${outputdir}/${test_name}_final.trieste --dump_passes ${outputdir}) 
endmacro()

# Regular expression to match test files
# This regex matches files with the .infix extension
set(TESTSUITE_REGEX ".*\\.vir")

set(TESTSUITE_EXE "$<TARGET_FILE:vbcc>")

function (test_output_dir out test)
  # Use get_filename_component to remove the file extension and keep the directory structure
  get_filename_component(test_dir ${test} DIRECTORY)
  get_filename_component(test_name ${test} NAME_WE)
  # Create the output directory relative to the test directory
  set(${out} "${test_dir}/${test_name}/compile" PARENT_SCOPE)
endfunction()
