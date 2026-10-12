include_guard(GLOBAL)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH VERONA_TESTSUITE_SOURCE_DIR)

function(_verona_fixture_key out value)
  string(SHA256 key "${value}")
  set(${out} "${key}" PARENT_SCOPE)
endfunction()

function(_verona_validate_vbc_stage stage)
  set(allowed none compile run)
  if(NOT stage IN_LIST allowed)
    message(FATAL_ERROR
      "Invalid VBC_STAGE '${stage}'; expected one of: none, compile, run")
  endif()
endfunction()

function(_verona_normalize_fixture_source out source)
  if(source STREQUAL "" OR source MATCHES [[\\|;|\$<]])
    message(FATAL_ERROR "Invalid fixture source '${source}'")
  endif()

  set(path "${source}")
  cmake_path(IS_ABSOLUTE path is_absolute)
  if(is_absolute)
    message(FATAL_ERROR "Fixture source '${source}' must be relative")
  endif()

  cmake_path(NORMAL_PATH path OUTPUT_VARIABLE normalized)
  if(
    normalized STREQUAL "."
    OR normalized MATCHES "^\\.\\.(/|$)"
    OR NOT normalized STREQUAL source)
    message(FATAL_ERROR "Fixture source '${source}' is not canonical")
  endif()

  if(NOT normalized MATCHES "^(v/.*[.]v|vir/.*[.]vir)$")
    message(FATAL_ERROR
      "Fixture source '${source}' must be a .v file under v/ or a .vir file under vir/")
  endif()

  if(NOT EXISTS "${VERONA_TESTSUITE_SOURCE_DIR}/${normalized}")
    message(FATAL_ERROR "Fixture source '${source}' does not exist")
  endif()

  set(${out} "${normalized}" PARENT_SCOPE)
endfunction()

function(verona_fixture_defaults)
  cmake_parse_arguments(DEFAULTS "" "VBC_STAGE" "" ${ARGN})
  if(DEFAULTS_UNPARSED_ARGUMENTS OR DEFAULTS_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR
      "Invalid verona_fixture_defaults() arguments: "
      "${DEFAULTS_UNPARSED_ARGUMENTS}${DEFAULTS_KEYWORDS_MISSING_VALUES}")
  endif()
  if(NOT DEFINED DEFAULTS_VBC_STAGE)
    message(FATAL_ERROR "verona_fixture_defaults() requires VBC_STAGE")
  endif()

  get_property(already_set GLOBAL PROPERTY VERONA_FIXTURE_DEFAULTS SET)
  if(already_set)
    message(FATAL_ERROR "verona_fixture_defaults() may only be called once")
  endif()

  _verona_validate_vbc_stage("${DEFAULTS_VBC_STAGE}")
  set_property(GLOBAL PROPERTY VERONA_FIXTURE_DEFAULTS TRUE)
  set_property(GLOBAL PROPERTY VERONA_FIXTURE_DEFAULT_VBC_STAGE "${DEFAULTS_VBC_STAGE}")
endfunction()

function(verona_fixture_rule)
  cmake_parse_arguments(RULE "" "MATCH;VBC_STAGE" "" ${ARGN})
  if(RULE_UNPARSED_ARGUMENTS OR RULE_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR
      "Invalid verona_fixture_rule() arguments: "
      "${RULE_UNPARSED_ARGUMENTS}${RULE_KEYWORDS_MISSING_VALUES}")
  endif()
  if(NOT DEFINED RULE_MATCH OR NOT DEFINED RULE_VBC_STAGE)
    message(FATAL_ERROR "verona_fixture_rule() requires MATCH and VBC_STAGE")
  endif()

  string(SUBSTRING "${RULE_MATCH}" 0 1 anchor)
  if(NOT anchor STREQUAL "^")
    message(FATAL_ERROR "Fixture rule '${RULE_MATCH}' must be anchored")
  endif()
  _verona_validate_vbc_stage("${RULE_VBC_STAGE}")

  get_property(matches GLOBAL PROPERTY VERONA_FIXTURE_RULE_MATCHES)
  if(RULE_MATCH IN_LIST matches)
    message(FATAL_ERROR "Duplicate fixture rule '${RULE_MATCH}'")
  endif()

  _verona_fixture_key(key "${RULE_MATCH}")
  set_property(GLOBAL APPEND PROPERTY VERONA_FIXTURE_RULE_MATCHES "${RULE_MATCH}")
  set_property(GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${key}_VBC_STAGE" "${RULE_VBC_STAGE}")
endfunction()

function(verona_fixture)
  cmake_parse_arguments(FIXTURE "" "SOURCE;VBC_STAGE" "" ${ARGN})
  if(FIXTURE_UNPARSED_ARGUMENTS OR FIXTURE_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR
      "Invalid verona_fixture() arguments: "
      "${FIXTURE_UNPARSED_ARGUMENTS}${FIXTURE_KEYWORDS_MISSING_VALUES}")
  endif()
  if(NOT DEFINED FIXTURE_SOURCE OR NOT DEFINED FIXTURE_VBC_STAGE)
    message(FATAL_ERROR "verona_fixture() requires SOURCE and VBC_STAGE")
  endif()

  _verona_normalize_fixture_source(source "${FIXTURE_SOURCE}")
  _verona_validate_vbc_stage("${FIXTURE_VBC_STAGE}")
  get_property(overrides GLOBAL PROPERTY VERONA_FIXTURE_OVERRIDES)
  if(source IN_LIST overrides)
    message(FATAL_ERROR "Duplicate fixture override for '${source}'")
  endif()

  _verona_fixture_key(key "${source}")
  set_property(GLOBAL APPEND PROPERTY VERONA_FIXTURE_OVERRIDES "${source}")
  set_property(GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${key}_VBC_STAGE" "${FIXTURE_VBC_STAGE}")
endfunction()

function(verona_fixture_metadata source out_vbc)
  _verona_fixture_key(key "${source}")
  get_property(has_vbc GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE" SET)
  if(NOT has_vbc)
    message(FATAL_ERROR "Fixture '${source}' has no effective metadata")
  endif()
  get_property(vbc GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE")
  set(${out_vbc} "${vbc}" PARENT_SCOPE)
endfunction()

function(verona_validate_compiler_fixtures)
  get_property(has_defaults GLOBAL PROPERTY VERONA_FIXTURE_DEFAULTS SET)
  if(NOT has_defaults)
    message(FATAL_ERROR "Compiler fixture metadata requires defaults")
  endif()

  file(
    GLOB_RECURSE discovered
    CONFIGURE_DEPENDS
    RELATIVE "${VERONA_TESTSUITE_SOURCE_DIR}"
    "${VERONA_TESTSUITE_SOURCE_DIR}/v/*.v"
    "${VERONA_TESTSUITE_SOURCE_DIR}/vir/*.vir")

  set(canonical)
  foreach(source IN LISTS discovered)
    if(source MATCHES "^v/")
      cmake_path(GET source PARENT_PATH parent)
      cmake_path(GET parent FILENAME directory_name)
      cmake_path(GET source STEM file_name)
      if(NOT directory_name STREQUAL file_name)
        continue()
      endif()
    endif()
    list(APPEND canonical "${source}")
  endforeach()

  get_property(default_vbc GLOBAL PROPERTY VERONA_FIXTURE_DEFAULT_VBC_STAGE)
  get_property(rules GLOBAL PROPERTY VERONA_FIXTURE_RULE_MATCHES)
  get_property(overrides GLOBAL PROPERTY VERONA_FIXTURE_OVERRIDES)
  foreach(source IN LISTS overrides)
    if(NOT source IN_LIST canonical)
      message(FATAL_ERROR
        "Fixture override '${source}' does not name a canonical fixture")
    endif()
  endforeach()

  set(matched_rules)
  foreach(source IN LISTS canonical)
    set(vbc_stage "${default_vbc}")
    set(matched_rule "")
    foreach(match IN LISTS rules)
      if(source MATCHES "${match}")
        if(NOT matched_rule STREQUAL "")
          message(FATAL_ERROR
            "Fixture '${source}' matches overlapping rules '${matched_rule}' and '${match}'")
        endif()
        set(matched_rule "${match}")
        list(APPEND matched_rules "${match}")
        _verona_fixture_key(rule_key "${match}")
        get_property(vbc_stage GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${rule_key}_VBC_STAGE")
      endif()
    endforeach()

    if(source IN_LIST overrides)
      _verona_fixture_key(override_key "${source}")
      get_property(vbc_stage GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${override_key}_VBC_STAGE")
    endif()

    _verona_fixture_key(key "${source}")
    set_property(GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE" "${vbc_stage}")
  endforeach()

  list(REMOVE_DUPLICATES matched_rules)
  foreach(match IN LISTS rules)
    if(NOT match IN_LIST matched_rules)
      message(FATAL_ERROR "Fixture rule '${match}' matches no canonical fixtures")
    endif()
  endforeach()
endfunction()
