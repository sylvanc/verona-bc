if(VERONA_ENABLE_LLVM_BACKEND)
  set(TESTSUITE_REGEX "^vir/llvm_[^/]+/llvm_[^/]+\\.vir$")

  find_program(
    VERONA_LLVM_AS
    NAMES llvm-as
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH)
  find_program(
    VERONA_LLC
    NAMES llc
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH)

  if(NOT VERONA_LLVM_AS OR NOT VERONA_LLC)
    message(FATAL_ERROR "LLVM backend tests require llvm-as and llc")
  endif()
else()
  # A collection must always define a selector, even when its component is
  # disabled. No discovered source path is empty, so this selects nothing.
  set(TESTSUITE_REGEX "^$")
endif()

set(TESTSUITE_DEFINE llvm_test_define)

function(llvm_test_define test)
  get_filename_component(test_dir "${test}" DIRECTORY)
  get_filename_component(test_file "${test}" NAME)
  get_filename_component(test_name "${test}" NAME_WE)
  get_filename_component(test_dir_name "${test_dir}" NAME)
  if(NOT test_name STREQUAL test_dir_name)
    return()
  endif()

  set(test_root "${test_dir}/${test_name}")
  set(llvm_root "${test_root}/llvm")
  set(emit_node "${llvm_root}/emit-ir")
  set(assemble_node "${llvm_root}/assemble")
  set(codegen_node "${llvm_root}/codegen")
  set(link_node "${llvm_root}/link")
  set(run_node "${llvm_root}/run")

  set(llvm_ir_name "${test_name}.ll")
  set(llvm_bc_name "${test_name}.bc")
  set(native_object_name "${test_name}${CMAKE_CXX_OUTPUT_EXTENSION}")
  set(native_name "${test_name}${CMAKE_EXECUTABLE_SUFFIX}")

  testsuite_output_path(
    llvm_ir NODE "${emit_node}" FILE "${llvm_ir_name}")
  testsuite_output_path(
    llvm_final_ast
    NODE "${emit_node}"
    FILE "${test_name}_final.trieste")
  testsuite_output_path(
    llvm_bc NODE "${assemble_node}" FILE "${llvm_bc_name}")
  testsuite_output_path(
    native_object NODE "${codegen_node}" FILE "${native_object_name}")
  testsuite_output_path(
    native NODE "${link_node}" FILE "${native_name}")

  set(test_working_directory "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}")

  testsuite_add_test(
    NAME "${emit_node}"
    WORKING_DIRECTORY "${test_working_directory}"
    TIMEOUT 60
    VALIDATOR
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/llvm/cmake/validate_llvm_ir.cmake"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ARTIFACTS "${llvm_ir_name}"
    COMMAND
      "${CMAKE_INSTALL_PREFIX}/vbcc/$<TARGET_FILE_NAME:vbcc>"
      build "${test_file}"
      --emit llvm-ir
      --output-file "${llvm_ir}"
      -o "${llvm_final_ast}")

  testsuite_add_test(
    NAME "${assemble_node}"
    WORKING_DIRECTORY "${test_working_directory}"
    TIMEOUT 60
    DEPENDS "${emit_node}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ARTIFACTS "${llvm_bc_name}"
    COMMAND "${VERONA_LLVM_AS}" "${llvm_ir}" -o "${llvm_bc}")

  testsuite_add_test(
    NAME "${codegen_node}"
    WORKING_DIRECTORY "${test_working_directory}"
    TIMEOUT 60
    DEPENDS "${assemble_node}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ARTIFACTS "${native_object_name}"
    COMMAND
      "${VERONA_LLC}"
      -filetype=obj
      "${llvm_bc}"
      -o "${native_object}")

  if(MSVC)
    set(link_arguments
      "${native_object}"
      "${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:libvrt>"
      "/Fe:${native}")
  else()
    set(link_arguments
      "${native_object}"
      "${CMAKE_INSTALL_PREFIX}/lib/$<TARGET_FILE_NAME:libvrt>"
      -o "${native}")
  endif()

  testsuite_add_test(
    NAME "${link_node}"
    WORKING_DIRECTORY "${test_working_directory}"
    TIMEOUT 60
    DEPENDS "${codegen_node}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    ARTIFACTS "${native_name}"
    COMMAND "${CMAKE_CXX_COMPILER}" ${link_arguments})

  testsuite_add_test(
    NAME "${run_node}"
    WORKING_DIRECTORY "${test_working_directory}"
    DEPENDS "${link_node}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    COMMAND "${native}")
endfunction()
