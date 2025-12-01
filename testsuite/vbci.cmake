# Arguments for testing vbci samples
macro(toolinvoke ARGS testfile outputdir)
# Use the following line to disable tracing output from the vbci tool
  set(${ARGS} ${testfile}) 
# Use the following line to enable tracing output from the vbci tool
#  set(${ARGS} ${testfile} -l Trace) 
endmacro()

# Regular expression to match test files
# This regex matches files with the .infix extension
set(TESTSUITE_REGEX ".*\\.vbc")

set(TESTSUITE_EXE "$<TARGET_FILE:vbci>")

function (test_output_dir out test)
  # Use get_filename_component to remove the file extension and keep the directory structure
  get_filename_component(test_dir ${test} DIRECTORY)
  get_filename_component(parent_dir ${test_dir} DIRECTORY)
  # Create the output directory relative to the test directory
  set(${out} "${parent_dir}/run" PARENT_SCOPE)
endfunction()
