function(ssg_append_glob list_name pattern)
	file(GLOB _matches CONFIGURE_DEPENDS "${pattern}")
	set(_result ${${list_name}} ${_matches})
	list(REMOVE_DUPLICATES _result)
	set(${list_name} "${_result}" PARENT_SCOPE)
endfunction()

function(ssg_require_file path submodule)
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR
			"Missing ${path}. Initialize the ${submodule} submodule first.")
	endif()
endfunction()

function(ssg_add_blake3 target_name)
	set(_root "${CMAKE_SOURCE_DIR}/libs/BLAKE3/c")
	ssg_require_file("${_root}/blake3.c" "libs/BLAKE3")

	set(_sources
		"${_root}/blake3.c"
		"${_root}/blake3_dispatch.c"
		"${_root}/blake3_portable.c"
	)

	if(MSVC)
		set(_sse2 "${_root}/blake3_sse2.c")
		set(_avx2 "${_root}/blake3_avx2.c")
		set(_avx512 "${_root}/blake3_avx512.c")
		list(APPEND _sources ${_sse2} ${_avx2} ${_avx512})
		set_source_files_properties("${_sse2}" PROPERTIES COMPILE_OPTIONS "/arch:SSE2")
		set_source_files_properties("${_avx2}" PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
		set_source_files_properties("${_avx512}" PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
	else()
		string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _processor)
		if(_processor MATCHES "^(x86_64|amd64)$")
			enable_language(ASM)
			file(GLOB _assembly CONFIGURE_DEPENDS "${_root}/*_unix.S")
			list(APPEND _sources ${_assembly})
		else()
			set(_generic TRUE)
		endif()
	endif()

	add_library(${target_name} STATIC ${_sources})
	target_include_directories(${target_name} PUBLIC "${_root}")
	set_target_properties(${target_name} PROPERTIES CXX_SCAN_FOR_MODULES OFF)

	if(MSVC)
		target_compile_definitions(${target_name} PRIVATE BLAKE3_NO_SSE41)
	elseif(_generic)
		target_compile_definitions(${target_name} PRIVATE
			BLAKE3_NO_SSE2
			BLAKE3_NO_SSE41
			BLAKE3_NO_AVX2
			BLAKE3_NO_AVX512
		)
	endif()
endfunction()

if(WIN32)
	include(cmake/DependenciesWindows.cmake)
else()
	find_package(PkgConfig REQUIRED)
	find_package(Threads REQUIRED)

	pkg_check_modules(SSG_PLATFORM REQUIRED IMPORTED_TARGET GLOBAL
		sdl3
		pangocairo
		fontconfig
	)
	pkg_check_modules(SSG_CODECS REQUIRED IMPORTED_TARGET GLOBAL
		libwebp
		ogg
		vorbis
		vorbisfile
	)
	pkg_check_modules(SSG_BLAKE3 QUIET IMPORTED_TARGET GLOBAL libblake3)

	add_library(ssg_dependencies INTERFACE)
	target_link_libraries(ssg_dependencies INTERFACE
		PkgConfig::SSG_PLATFORM
		PkgConfig::SSG_CODECS
		Threads::Threads
	)

	if(SSG_BLAKE3_FOUND)
		target_link_libraries(ssg_dependencies INTERFACE PkgConfig::SSG_BLAKE3)
	else()
		ssg_add_blake3(ssg_blake3)
		target_link_libraries(ssg_dependencies INTERFACE ssg_blake3)
	endif()
endif()
