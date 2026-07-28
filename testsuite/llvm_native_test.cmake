file(MAKE_DIRECTORY "${OUTPUT_DIR}")

get_filename_component(test_name "${VIR}" NAME_WE)
set(llvm_ir "${OUTPUT_DIR}/${test_name}.ll")
set(llvm_bc "${OUTPUT_DIR}/${test_name}.bc")
set(native_object "${OUTPUT_DIR}/${test_name}${OBJECT_SUFFIX}")
set(native "${OUTPUT_DIR}/${test_name}${NATIVE_SUFFIX}")

execute_process(
  COMMAND
    "${VBCC}"
    build
    "${VIR}"
    --emit
    llvm-ir
    --output-file
    "${llvm_ir}"
  RESULT_VARIABLE compiler_result
  OUTPUT_VARIABLE compiler_output
  ERROR_VARIABLE compiler_error)

if(NOT compiler_result EQUAL 0)
  message(FATAL_ERROR
    "LLVM compilation failed (${compiler_result})\n"
    "stdout:\n${compiler_output}\n"
    "stderr:\n${compiler_error}")
endif()

execute_process(
  COMMAND "${LLVM_AS}" "${llvm_ir}" -o "${llvm_bc}"
  RESULT_VARIABLE verifier_result
  OUTPUT_VARIABLE verifier_output
  ERROR_VARIABLE verifier_error)

if(NOT verifier_result EQUAL 0)
  message(FATAL_ERROR
    "LLVM verification failed (${verifier_result})\n"
    "stdout:\n${verifier_output}\n"
    "stderr:\n${verifier_error}")
endif()

execute_process(
  COMMAND "${LLC}" -filetype=obj "${llvm_bc}" -o "${native_object}"
  RESULT_VARIABLE object_result
  OUTPUT_VARIABLE object_output
  ERROR_VARIABLE object_error)

if(NOT object_result EQUAL 0)
  message(FATAL_ERROR
    "native object generation failed (${object_result})\n"
    "stdout:\n${object_output}\n"
    "stderr:\n${object_error}")
endif()

if(MSVC)
  execute_process(
    COMMAND "${CXX}" "${native_object}" "${RUNTIME}" "/Fe:${native}"
    RESULT_VARIABLE linker_result
    OUTPUT_VARIABLE linker_output
    ERROR_VARIABLE linker_error)
else()
  execute_process(
    COMMAND "${CXX}" "${native_object}" "${RUNTIME}" -o "${native}"
    RESULT_VARIABLE linker_result
    OUTPUT_VARIABLE linker_output
    ERROR_VARIABLE linker_error)
endif()

if(NOT linker_result EQUAL 0)
  message(FATAL_ERROR
    "native link failed (${linker_result})\n"
    "stdout:\n${linker_output}\n"
    "stderr:\n${linker_error}")
endif()

execute_process(
  COMMAND "${native}"
  TIMEOUT 20
  RESULT_VARIABLE native_result
  OUTPUT_VARIABLE native_output
  ERROR_VARIABLE native_error)

if(NOT "${native_result}" STREQUAL "${EXPECTED_EXIT}")
  message(FATAL_ERROR
    "native execution returned ${native_result}, expected ${EXPECTED_EXIT}\n"
    "stdout:\n${native_output}\n"
    "stderr:\n${native_error}")
endif()
