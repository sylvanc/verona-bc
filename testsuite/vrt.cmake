set(TESTSUITE_REGEX
  "^vrt/(abi|api|behavior|internal)/[^/]+/[^/]+\\.(c|cc)$")
set(TESTSUITE_DEFINE vrt_test_define)

function(vrt_test_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(
      ${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endfunction()

function(vrt_c_test_properties target)
  set_target_properties(
    ${target}
    PROPERTIES
      C_EXTENSIONS OFF
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
      LINKER_LANGUAGE CXX)
  vrt_test_compile_options(${target})
endfunction()

function(vrt_add_run_node name target working_directory)
  testsuite_add_test(
    NAME "${name}"
    WORKING_DIRECTORY "${working_directory}"
    GOLDENS exit_code.txt stderr.txt stdout.txt
    COMMAND "$<TARGET_FILE:${target}>")
endfunction()

function(vrt_add_fixture_test test)
  get_filename_component(fixture_dir "${test}" DIRECTORY)
  get_filename_component(source_name "${test}" NAME_WE)
  get_filename_component(fixture_name "${fixture_dir}" NAME)
  get_filename_component(category_dir "${fixture_dir}" DIRECTORY)
  get_filename_component(category "${category_dir}" NAME)
  get_filename_component(extension "${test}" EXT)

  if(NOT source_name STREQUAL fixture_name)
    message(FATAL_ERROR
      "VRT fixture source '${test}' must match its directory name")
  endif()

  string(REPLACE "-" "_" target_name "${category}_${fixture_name}")
  set(target "vrt_${target_name}")
  add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/${test}")

  if(extension STREQUAL ".c")
    vrt_c_test_properties(${target})
  else()
    vrt_test_compile_options(${target})
  endif()

  if(category STREQUAL "api" OR category STREQUAL "internal")
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/vrt")
  endif()

  target_link_libraries(${target} PRIVATE vbc::vrt)
  if(category STREQUAL "api" AND fixture_name STREQUAL "thread")
    target_link_libraries(${target} PRIVATE Threads::Threads)
  endif()

  vrt_add_run_node(
    "${fixture_dir}" ${target} "${CMAKE_CURRENT_SOURCE_DIR}/${fixture_dir}")
endfunction()

function(vrt_test_define test)
  if(test MATCHES "^vrt/(abi|api|behavior|internal)/[^/]+/[^/]+\\.(c|cc)$")
    vrt_add_fixture_test("${test}")
  else()
    message(FATAL_ERROR "Unexpected VRT test source '${test}'")
  endif()
endfunction()
