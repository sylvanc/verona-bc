if(NOT DEFINED PROGRAM)
  message(FATAL_ERROR "PROGRAM is required")
endif()

if(NOT DEFINED EXPECTED_EXIT)
  message(FATAL_ERROR "EXPECTED_EXIT is required")
endif()

execute_process(
  COMMAND "${PROGRAM}"
  TIMEOUT 10
  RESULT_VARIABLE actual_exit
  OUTPUT_VARIABLE program_output
  ERROR_VARIABLE program_error)

if(NOT "${actual_exit}" STREQUAL "${EXPECTED_EXIT}")
  message(FATAL_ERROR
    "${PROGRAM} returned ${actual_exit}, expected ${EXPECTED_EXIT}\n"
    "stdout:\n${program_output}\n"
    "stderr:\n${program_error}")
endif()
