include_guard(GLOBAL)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH VERONA_TESTSUITE_SOURCE_DIR)

function(_verona_fixture_key out value)
	string(SHA256 key "${value}")
	set(${out} "${key}" PARENT_SCOPE)
endfunction()

function(_verona_validate_stage backend stage allowed)
	if(NOT stage IN_LIST allowed)
		list(JOIN allowed ", " expected)
		message(FATAL_ERROR
			"Invalid ${backend}_STAGE '${stage}'; expected one of: ${expected}")
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
	cmake_parse_arguments(DEFAULTS "" "VBC_STAGE;LLVM_STAGE" "" ${ARGN})
	if(DEFAULTS_UNPARSED_ARGUMENTS OR DEFAULTS_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR
			"Invalid verona_fixture_defaults() arguments: "
			"${DEFAULTS_UNPARSED_ARGUMENTS}${DEFAULTS_KEYWORDS_MISSING_VALUES}")
	endif()
	if(NOT DEFINED DEFAULTS_VBC_STAGE OR NOT DEFINED DEFAULTS_LLVM_STAGE)
		message(FATAL_ERROR
			"verona_fixture_defaults() requires VBC_STAGE and LLVM_STAGE")
	endif()

	get_property(already_set GLOBAL PROPERTY VERONA_FIXTURE_DEFAULTS SET)
	if(already_set)
		message(FATAL_ERROR "verona_fixture_defaults() may only be called once")
	endif()

	set(vbc_stages none compile run)
	set(llvm_stages none emit-ir assemble codegen link run)
	_verona_validate_stage(VBC "${DEFAULTS_VBC_STAGE}" "${vbc_stages}")
	_verona_validate_stage(LLVM "${DEFAULTS_LLVM_STAGE}" "${llvm_stages}")
	set_property(GLOBAL PROPERTY VERONA_FIXTURE_DEFAULTS TRUE)
	set_property(GLOBAL PROPERTY VERONA_FIXTURE_DEFAULT_VBC_STAGE "${DEFAULTS_VBC_STAGE}")
	set_property(GLOBAL PROPERTY VERONA_FIXTURE_DEFAULT_LLVM_STAGE "${DEFAULTS_LLVM_STAGE}")
endfunction()

function(verona_fixture_rule)
	cmake_parse_arguments(RULE "" "MATCH;VBC_STAGE;LLVM_STAGE" "LABELS" ${ARGN})
	if(RULE_UNPARSED_ARGUMENTS OR RULE_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR
			"Invalid verona_fixture_rule() arguments: "
			"${RULE_UNPARSED_ARGUMENTS}${RULE_KEYWORDS_MISSING_VALUES}")
	endif()
	if(NOT DEFINED RULE_MATCH)
		message(FATAL_ERROR "verona_fixture_rule() requires MATCH")
	endif()
	if(
		NOT DEFINED RULE_VBC_STAGE
		AND NOT DEFINED RULE_LLVM_STAGE
		AND NOT DEFINED RULE_LABELS)
		message(FATAL_ERROR "Fixture rule '${RULE_MATCH}' does not override metadata")
	endif()

	string(SUBSTRING "${RULE_MATCH}" 0 1 anchor)
	if(NOT anchor STREQUAL "^")
		message(FATAL_ERROR "Fixture rule '${RULE_MATCH}' must be anchored")
	endif()
	set(vbc_stages none compile run)
	set(llvm_stages none emit-ir assemble codegen link run)
	if(DEFINED RULE_VBC_STAGE)
		_verona_validate_stage(VBC "${RULE_VBC_STAGE}" "${vbc_stages}")
	endif()
	if(DEFINED RULE_LLVM_STAGE)
		_verona_validate_stage(LLVM "${RULE_LLVM_STAGE}" "${llvm_stages}")
	endif()

	get_property(matches GLOBAL PROPERTY VERONA_FIXTURE_RULE_MATCHES)
	if(RULE_MATCH IN_LIST matches)
		message(FATAL_ERROR "Duplicate fixture rule '${RULE_MATCH}'")
	endif()

	_verona_fixture_key(key "${RULE_MATCH}")
	set_property(GLOBAL APPEND PROPERTY VERONA_FIXTURE_RULE_MATCHES "${RULE_MATCH}")
	if(DEFINED RULE_VBC_STAGE)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${key}_VBC_STAGE" "${RULE_VBC_STAGE}")
	endif()
	if(DEFINED RULE_LLVM_STAGE)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${key}_LLVM_STAGE" "${RULE_LLVM_STAGE}")
	endif()
	if(DEFINED RULE_LABELS)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${key}_LABELS" "${RULE_LABELS}")
	endif()
endfunction()

function(verona_fixture_group)
	cmake_parse_arguments(
		GROUP
		""
		"VBC_STAGE;LLVM_STAGE;LLVM_VALIDATOR"
		"SOURCES;LABELS"
		${ARGN})
	if(GROUP_UNPARSED_ARGUMENTS OR GROUP_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR
			"Invalid verona_fixture_group() arguments: "
			"${GROUP_UNPARSED_ARGUMENTS}${GROUP_KEYWORDS_MISSING_VALUES}")
	endif()
	if(NOT DEFINED GROUP_SOURCES OR GROUP_SOURCES STREQUAL "")
		message(FATAL_ERROR "verona_fixture_group() requires SOURCES")
	endif()
	if(
		NOT DEFINED GROUP_VBC_STAGE
		AND NOT DEFINED GROUP_LLVM_STAGE
		AND NOT DEFINED GROUP_LLVM_VALIDATOR
		AND NOT DEFINED GROUP_LABELS)
		message(FATAL_ERROR "Fixture group does not override metadata")
	endif()

	set(vbc_stages none compile run)
	set(llvm_stages none emit-ir assemble codegen link run)
	if(DEFINED GROUP_VBC_STAGE)
		_verona_validate_stage(VBC "${GROUP_VBC_STAGE}" "${vbc_stages}")
	endif()
	if(DEFINED GROUP_LLVM_STAGE)
		_verona_validate_stage(LLVM "${GROUP_LLVM_STAGE}" "${llvm_stages}")
	endif()
	if(
		DEFINED GROUP_LLVM_VALIDATOR
		AND NOT EXISTS "${VERONA_TESTSUITE_SOURCE_DIR}/${GROUP_LLVM_VALIDATOR}")
		message(FATAL_ERROR
			"LLVM group validator '${GROUP_LLVM_VALIDATOR}' does not exist")
	endif()

	get_property(grouped_sources GLOBAL PROPERTY VERONA_FIXTURE_GROUP_SOURCES)
	foreach(group_source IN LISTS GROUP_SOURCES)
		_verona_normalize_fixture_source(source "${group_source}")
		if(source IN_LIST grouped_sources)
			message(FATAL_ERROR "Fixture '${source}' belongs to multiple groups")
		endif()

		_verona_fixture_key(key "${source}")
		list(APPEND grouped_sources "${source}")
		if(DEFINED GROUP_VBC_STAGE)
			set_property(GLOBAL PROPERTY "VERONA_FIXTURE_GROUP_${key}_VBC_STAGE" "${GROUP_VBC_STAGE}")
		endif()
		if(DEFINED GROUP_LLVM_STAGE)
			set_property(GLOBAL PROPERTY "VERONA_FIXTURE_GROUP_${key}_LLVM_STAGE" "${GROUP_LLVM_STAGE}")
		endif()
		if(DEFINED GROUP_LLVM_VALIDATOR)
			set_property(GLOBAL PROPERTY "VERONA_FIXTURE_GROUP_${key}_LLVM_VALIDATOR" "${GROUP_LLVM_VALIDATOR}")
		endif()
		if(DEFINED GROUP_LABELS)
			set_property(GLOBAL PROPERTY "VERONA_FIXTURE_GROUP_${key}_LABELS" "${GROUP_LABELS}")
		endif()
	endforeach()
	set_property(GLOBAL PROPERTY VERONA_FIXTURE_GROUP_SOURCES "${grouped_sources}")
endfunction()

function(verona_fixture)
	cmake_parse_arguments(
		FIXTURE
		""
		"SOURCE;VBC_STAGE;LLVM_STAGE;LLVM_VALIDATOR"
		"LABELS"
		${ARGN})
	if(FIXTURE_UNPARSED_ARGUMENTS OR FIXTURE_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR
			"Invalid verona_fixture() arguments: "
			"${FIXTURE_UNPARSED_ARGUMENTS}${FIXTURE_KEYWORDS_MISSING_VALUES}")
	endif()
	if(NOT DEFINED FIXTURE_SOURCE)
		message(FATAL_ERROR "verona_fixture() requires SOURCE")
	endif()
	if(
		NOT DEFINED FIXTURE_VBC_STAGE
		AND NOT DEFINED FIXTURE_LLVM_STAGE
		AND NOT DEFINED FIXTURE_LLVM_VALIDATOR
		AND NOT DEFINED FIXTURE_LABELS)
		message(FATAL_ERROR "Fixture '${FIXTURE_SOURCE}' does not override metadata")
	endif()

	_verona_normalize_fixture_source(source "${FIXTURE_SOURCE}")
	set(vbc_stages none compile run)
	set(llvm_stages none emit-ir assemble codegen link run)
	if(DEFINED FIXTURE_VBC_STAGE)
		_verona_validate_stage(VBC "${FIXTURE_VBC_STAGE}" "${vbc_stages}")
	endif()
	if(DEFINED FIXTURE_LLVM_STAGE)
		_verona_validate_stage(LLVM "${FIXTURE_LLVM_STAGE}" "${llvm_stages}")
	endif()
	if(
		DEFINED FIXTURE_LLVM_VALIDATOR
		AND NOT EXISTS "${VERONA_TESTSUITE_SOURCE_DIR}/${FIXTURE_LLVM_VALIDATOR}")
		message(FATAL_ERROR
			"LLVM validator '${FIXTURE_LLVM_VALIDATOR}' for '${source}' does not exist")
	endif()

	get_property(overrides GLOBAL PROPERTY VERONA_FIXTURE_OVERRIDES)
	if(source IN_LIST overrides)
		message(FATAL_ERROR "Duplicate fixture override for '${source}'")
	endif()

	_verona_fixture_key(key "${source}")
	set_property(GLOBAL APPEND PROPERTY VERONA_FIXTURE_OVERRIDES "${source}")
	if(DEFINED FIXTURE_VBC_STAGE)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${key}_VBC_STAGE" "${FIXTURE_VBC_STAGE}")
	endif()
	if(DEFINED FIXTURE_LLVM_STAGE)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${key}_LLVM_STAGE" "${FIXTURE_LLVM_STAGE}")
	endif()
	if(DEFINED FIXTURE_LLVM_VALIDATOR)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${key}_LLVM_VALIDATOR" "${FIXTURE_LLVM_VALIDATOR}")
	endif()
	if(DEFINED FIXTURE_LABELS)
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${key}_LABELS" "${FIXTURE_LABELS}")
	endif()
endfunction()

function(verona_fixture_metadata source out_vbc out_llvm out_validator out_labels)
	_verona_fixture_key(key "${source}")
	get_property(has_vbc GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE" SET)
	get_property(has_llvm GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LLVM_STAGE" SET)
	if(NOT has_vbc OR NOT has_llvm)
		message(FATAL_ERROR "Fixture '${source}' has no effective metadata")
	endif()
	get_property(vbc GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE")
	get_property(llvm GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LLVM_STAGE")
	get_property(validator GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LLVM_VALIDATOR")
	get_property(labels GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LABELS")
	set(${out_vbc} "${vbc}" PARENT_SCOPE)
	set(${out_llvm} "${llvm}" PARENT_SCOPE)
	set(${out_validator} "${validator}" PARENT_SCOPE)
	set(${out_labels} "${labels}" PARENT_SCOPE)
endfunction()

function(_verona_apply_optional_property property out)
	get_property(is_set GLOBAL PROPERTY "${property}" SET)
	if(is_set)
		get_property(value GLOBAL PROPERTY "${property}")
		set(${out} "${value}" PARENT_SCOPE)
	endif()
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
	get_property(default_llvm GLOBAL PROPERTY VERONA_FIXTURE_DEFAULT_LLVM_STAGE)
	get_property(rules GLOBAL PROPERTY VERONA_FIXTURE_RULE_MATCHES)
	get_property(grouped_sources GLOBAL PROPERTY VERONA_FIXTURE_GROUP_SOURCES)
	get_property(overrides GLOBAL PROPERTY VERONA_FIXTURE_OVERRIDES)
	foreach(source IN LISTS grouped_sources)
		if(NOT source IN_LIST canonical)
			message(FATAL_ERROR
				"Fixture group source '${source}' is not a canonical fixture")
		endif()
	endforeach()
	foreach(source IN LISTS overrides)
		if(NOT source IN_LIST canonical)
			message(FATAL_ERROR
				"Fixture override '${source}' does not name a canonical fixture")
		endif()
	endforeach()

	set(matched_rules)
	foreach(source IN LISTS canonical)
		set(vbc_stage "${default_vbc}")
		set(llvm_stage "${default_llvm}")
		set(llvm_validator "")
		set(labels)
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
				_verona_apply_optional_property(
					"VERONA_FIXTURE_RULE_${rule_key}_VBC_STAGE" vbc_stage)
				_verona_apply_optional_property(
					"VERONA_FIXTURE_RULE_${rule_key}_LLVM_STAGE" llvm_stage)
				get_property(rule_labels GLOBAL PROPERTY "VERONA_FIXTURE_RULE_${rule_key}_LABELS")
				list(APPEND labels ${rule_labels})
			endif()
		endforeach()

		if(source IN_LIST grouped_sources)
			_verona_fixture_key(group_key "${source}")
			_verona_apply_optional_property(
				"VERONA_FIXTURE_GROUP_${group_key}_VBC_STAGE" vbc_stage)
			_verona_apply_optional_property(
				"VERONA_FIXTURE_GROUP_${group_key}_LLVM_STAGE" llvm_stage)
			_verona_apply_optional_property(
				"VERONA_FIXTURE_GROUP_${group_key}_LLVM_VALIDATOR" llvm_validator)
			get_property(group_labels GLOBAL PROPERTY "VERONA_FIXTURE_GROUP_${group_key}_LABELS")
			list(APPEND labels ${group_labels})
		endif()

		if(source IN_LIST overrides)
			_verona_fixture_key(override_key "${source}")
			_verona_apply_optional_property(
				"VERONA_FIXTURE_OVERRIDE_${override_key}_VBC_STAGE" vbc_stage)
			_verona_apply_optional_property(
				"VERONA_FIXTURE_OVERRIDE_${override_key}_LLVM_STAGE" llvm_stage)
			_verona_apply_optional_property(
				"VERONA_FIXTURE_OVERRIDE_${override_key}_LLVM_VALIDATOR" llvm_validator)
			get_property(override_labels GLOBAL PROPERTY "VERONA_FIXTURE_OVERRIDE_${override_key}_LABELS")
			list(APPEND labels ${override_labels})
		endif()

		if(NOT llvm_validator STREQUAL "" AND llvm_stage STREQUAL "none")
			message(FATAL_ERROR
				"Fixture '${source}' cannot use LLVM_VALIDATOR with LLVM_STAGE none")
		endif()
		list(REMOVE_DUPLICATES labels)

		_verona_fixture_key(key "${source}")
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_${key}_VBC_STAGE" "${vbc_stage}")
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LLVM_STAGE" "${llvm_stage}")
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LLVM_VALIDATOR" "${llvm_validator}")
		set_property(GLOBAL PROPERTY "VERONA_FIXTURE_${key}_LABELS" "${labels}")
	endforeach()

	list(REMOVE_DUPLICATES matched_rules)
	foreach(match IN LISTS rules)
		if(NOT match IN_LIST matched_rules)
			message(FATAL_ERROR "Fixture rule '${match}' matches no canonical fixtures")
		endif()
	endforeach()
endfunction()
