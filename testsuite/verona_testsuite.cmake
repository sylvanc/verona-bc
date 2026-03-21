find_program(
  DIFF_TOOL
  NAMES diff)

set(DIR_OF_TESTSUITE_CMAKE ${trieste_SOURCE_DIR}/cmake)

if(DIFF_TOOL STREQUAL DIFF_TOOL-NOTFOUND)
  set(DIFF_TOOL "")
endif()

function(testsuite name)
  message(STATUS "Building test suite: ${name}")
  set(UPDATE_DUMPS_TARGETS)
  file(GLOB test_collections CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cmake)
  list(REMOVE_ITEM test_collections verona_testsuite.cmake)
  file(GLOB_RECURSE all_files CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

  set(CLEAN_GOLDEN_DIRS)

  foreach(test_collection ${test_collections})
    set(test_set)
    unset(TESTSUITE_RESULT_FILES)
    unset(TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REGEX)
    unset(TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REPLACE)
    unset(TESTSUITE_SKIP_CLEAN_GOLDEN_DIR)
    unset(TESTSUITE_REQUIRE_DEPENDENCY_EXIT_CODE_REGEX)

    include(${CMAKE_CURRENT_SOURCE_DIR}/${test_collection})

    set(tests ${all_files})
    list(FILTER tests INCLUDE REGEX ${TESTSUITE_REGEX})

    foreach(test ${tests})
      test_output_dir(output_dir_relative ${test})
      if(DEFINED TESTSUITE_REQUIRE_DEPENDENCY_EXIT_CODE_REGEX)
        string(
          REGEX REPLACE
            "${TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REGEX}"
            "${TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REPLACE}"
            dependency_output_dir
            "${output_dir_relative}")
        set(dependency_exit_code_file
          "${CMAKE_CURRENT_SOURCE_DIR}/${dependency_output_dir}/exit_code.txt")
        if(EXISTS "${dependency_exit_code_file}")
          file(READ "${dependency_exit_code_file}" dependency_exit_code)
          if(NOT dependency_exit_code MATCHES "${TESTSUITE_REQUIRE_DEPENDENCY_EXIT_CODE_REGEX}")
            continue()
          endif()
        endif()
      endif()

      string(MAKE_C_IDENTIFIER "${output_dir_relative}" output_fixture)
      get_filename_component(test_dir ${test} DIRECTORY)
      get_filename_component(test_file ${test} NAME)
      set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/${output_dir_relative})
      set(test_output_cmd
        ${CMAKE_COMMAND}
        -DTESTFILE=${test_file}
        -DTEST_EXE=${TESTSUITE_EXE}
        -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}
        -DCOLLECTION=${CMAKE_CURRENT_SOURCE_DIR}/${test_collection}
        -DOUTPUT_DIR=${output_dir}
        -P ${DIR_OF_TESTSUITE_CMAKE}/runcommand.cmake)
      set(test_update_dep)

      add_test(NAME ${output_dir_relative}
        COMMAND ${test_output_cmd})
      set_tests_properties(${output_dir_relative} PROPERTIES FIXTURES_SETUP ${output_fixture})

      if(DEFINED TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REGEX)
        string(
          REGEX REPLACE
            "${TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REGEX}"
            "${TESTSUITE_DEPENDS_ON_OUTPUT_DIR_REPLACE}"
            test_dependency
            "${output_dir_relative}")
        if(NOT test_dependency STREQUAL output_dir_relative)
          string(MAKE_C_IDENTIFIER "${test_dependency}" test_dependency_fixture)
          set_tests_properties(${output_dir_relative} PROPERTIES
            DEPENDS ${test_dependency}
            FIXTURES_REQUIRED ${test_dependency_fixture})
          set(test_update_dep "${test_dependency}_fake")
        endif()
      endif()

      add_custom_command(OUTPUT "${output_dir_relative}_fake"
        COMMAND ${test_output_cmd}
        DEPENDS ${test_update_dep})
      set_source_files_properties("${output_dir_relative}_fake" PROPERTIES SYMBOLIC "true")
      list(APPEND test_set "${output_dir_relative}_fake")

      set(WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir})
      toolinvoke(launch_json_args ${test_file} ${output_dir})
      unset(WORKING_DIR)
      string(REPLACE "\"" "\\\"" launch_json_args "${launch_json_args}")
      string(REPLACE ";" "\", \"" launch_json_args "${launch_json_args}")
      list(APPEND LAUNCH_JSON
  "    {
        \"name\": \"${output_dir_relative}\",
        \"type\": \"cppdbg\",
        \"request\": \"launch\",
        \"program\": \"${TESTSUITE_EXE}\",
        \"args\": [\"${launch_json_args}\"],
        \"stopAtEntry\": false,
        \"cwd\": \"${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}\",
      },")

      set(golden_dir ${CMAKE_CURRENT_SOURCE_DIR}/${output_dir_relative})
      if(NOT TESTSUITE_SKIP_CLEAN_GOLDEN_DIR)
        list(APPEND CLEAN_GOLDEN_DIRS ${golden_dir})
      endif()

      if(DEFINED TESTSUITE_RESULT_FILES)
        set(results)
        foreach(result ${TESTSUITE_RESULT_FILES})
          if(EXISTS ${golden_dir}/${result})
            list(APPEND results ${result})
          endif()
        endforeach()
        set(update_results ${TESTSUITE_RESULT_FILES})
      else()
        file(GLOB_RECURSE results CONFIGURE_DEPENDS RELATIVE ${golden_dir} ${golden_dir}/*)
        set(update_results ${results})
        if(NOT "exit_code.txt" IN_LIST update_results)
          list(APPEND update_results exit_code.txt)
        endif()
      endif()

      list(LENGTH results res_length)
      if(res_length EQUAL 0)
        message(
          WARNING
            "Test does not have results directory: ${golden_dir}\nRun `update-dump` to generate golden files.")
        add_custom_command(OUTPUT ${output_dir_relative}_fake
          COMMAND
            ${CMAKE_COMMAND}
            -E make_directory
            ${golden_dir}
          APPEND)
      else()
        foreach(result ${results})
          add_test(NAME ${output_dir_relative}-${result}
            COMMAND
              ${CMAKE_COMMAND}
              -Doriginal_file=${golden_dir}/${result}
              -Dnew_file=${output_dir}/${result}
              -Ddiff_tool=${DIFF_TOOL}
              -P ${DIR_OF_TESTSUITE_CMAKE}/compare.cmake)
          set_tests_properties(${output_dir_relative}-${result} PROPERTIES DEPENDS ${output_dir_relative})
        endforeach()
      endif()

      foreach(result ${update_results})
        add_custom_command(OUTPUT "${output_dir_relative}_fake"
          COMMAND
            ${CMAKE_COMMAND}
            -Dsrc=${output_dir}/${result}
            -Ddst=${golden_dir}/${result}
            -P ${DIR_OF_TESTSUITE_CMAKE}/copy_if_different_and_exists.cmake
          APPEND)
      endforeach()
    endforeach()

    add_custom_target("update-dump-${test_collection}" DEPENDS ${test_set})
    list(APPEND UPDATE_DUMPS_TARGETS "update-dump-${test_collection}")
  endforeach()

  list(REMOVE_DUPLICATES CLEAN_GOLDEN_DIRS)

  string(REPLACE ";" "\n" LAUNCH_JSON2 "${LAUNCH_JSON}")

  if(TRIESTE_GENERATE_LAUNCH_JSON)
    file(GENERATE OUTPUT ${CMAKE_SOURCE_DIR}/.vscode/launch.json
      CONTENT
  "{
    \"version\": \"0.2.0\",
    \"configurations\": [
      ${LAUNCH_JSON2}
    ]
  }")
  endif()

  if(TARGET update-dump)
    add_dependencies(update-dump ${UPDATE_DUMPS_TARGETS})
  else()
    add_custom_target(update-dump DEPENDS ${UPDATE_DUMPS_TARGETS})
  endif()

  if(CLEAN_GOLDEN_DIRS)
    add_custom_target(update-dump-clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${CLEAN_GOLDEN_DIRS}
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target update-dump
      USES_TERMINAL)
  else()
    add_custom_target(update-dump-clean
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target update-dump
      USES_TERMINAL)
  endif()
endfunction()
