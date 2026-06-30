function(ssg_generate_version_header output_path)
	set(_version_tag "(unknown)")
	find_package(Git QUIET)

	if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
			"${CMAKE_SOURCE_DIR}/.git/HEAD")

		execute_process(
			COMMAND "${GIT_EXECUTABLE}" diff-files --quiet
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			RESULT_VARIABLE _dirty
			ERROR_QUIET
		)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" tag --points-at HEAD
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			OUTPUT_VARIABLE _tag
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)

		if(_dirty EQUAL 0 AND _tag)
			string(REGEX REPLACE "\r?\n.*$" "" _version_tag "${_tag}")
		else()
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
				OUTPUT_VARIABLE _description
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" rev-parse --short=4 HEAD
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
				OUTPUT_VARIABLE _commit
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)
			if(_description)
				set(_version_tag "WIP (${_description}^${_commit})")
			elseif(_commit)
				set(_version_tag "WIP (${_commit})")
			endif()
		endif()
	endif()

	string(REPLACE "\\" "\\\\" SSG_VERSION_TAG_ESCAPED "${_version_tag}")
	string(REPLACE "\"" "\\\"" SSG_VERSION_TAG_ESCAPED
		"${SSG_VERSION_TAG_ESCAPED}")
	get_filename_component(_output_dir "${output_path}" DIRECTORY)
	file(MAKE_DIRECTORY "${_output_dir}")
	configure_file(
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.h.in"
		"${output_path}"
		@ONLY
	)
endfunction()
