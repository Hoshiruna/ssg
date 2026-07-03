cmake_minimum_required(VERSION 4.0.3)

# CMake's import-std support is still gated by a version-specific UUID.
if(CMAKE_VERSION VERSION_LESS "4.3")
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
		"d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_LESS "4.4")
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
		"451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
elseif(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
	message(FATAL_ERROR
		"This CMake version needs a newer import-std feature UUID. "
		"Pass it as -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<uuid>.")
endif()

project(ssg LANGUAGES C CXX)

if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
	message(FATAL_ERROR
		"SSG imports the C++ standard library as a module and must be built "
		"with the Ninja or Ninja Multi-Config generator.")
endif()

if(WIN32)
	if(NOT MSVC)
		message(FATAL_ERROR "Windows builds currently require the MSVC toolchain.")
	endif()
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
		message(FATAL_ERROR
			"SSG is a 32-bit application. Configure it from Visual Studio's "
			"x64_x86 Cross Tools Command Prompt.")
	endif()
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "The CMake build currently supports Windows and Linux.")
endif()

if(MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.36")
	message(FATAL_ERROR "MSVC 19.36 (Visual Studio 2022 17.6) or newer is required.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
	message(FATAL_ERROR "GCC 15 or newer is required.")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
		AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.1.2")
	message(FATAL_ERROR "Clang 18.1.2 or newer is required.")
endif()

# CMake compiles MSVC's std/std.compat module interfaces as a separate
# internal target. Keep /utf-8 in the global C++ flags so the module producer
# and every importing target define the same _UTF8 environment.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "(^| )/utf-8($| )")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

if(NOT 23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
	message(FATAL_ERROR
		"The selected compiler and standard library do not expose the C++23 "
		"standard library module to CMake.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep compatibility with the project's existing resource and launch layout.
# The generator expression prevents multi-config generators from appending a
# configuration subdirectory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<0:>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<0:>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

set(SSG_APP_ID "$ENV{APP_ID}" CACHE STRING
	"Application ID embedded in SDL metadata")
set(SSG_PATH_SKELETON "$ENV{PATH_SKELETON}" CACHE PATH
	"Directory copied into a new Linux user-data directory")

include(cmake/Version.cmake)
ssg_generate_version_header("${CMAKE_BINARY_DIR}/generated/obj/version.h")

foreach(_name IN ITEMS SSG_APP_ID SSG_PATH_SKELETON)
	string(REPLACE "\\" "\\\\" ${_name}_ESCAPED "${${_name}}")
	string(REPLACE "\"" "\\\"" ${_name}_ESCAPED "${${_name}_ESCAPED}")
endforeach()

set(SSG_APP_ID_DEFINE "")
if(SSG_APP_ID)
	set(SSG_APP_ID_DEFINE "#define APP_ID \"${SSG_APP_ID_ESCAPED}\"")
endif()

set(SSG_PATH_SKELETON_DEFINE "")
if(SSG_PATH_SKELETON)
	set(SSG_PATH_SKELETON_DEFINE
		"#define PATH_SKELETON \"${SSG_PATH_SKELETON_ESCAPED}\"")
endif()

configure_file(
	cmake/platform_constants.h.in
	"${CMAKE_BINARY_DIR}/generated/obj/platform_constants.h"
	@ONLY
)

include(cmake/Dependencies.cmake)

file(GLOB SSG_GAME_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/GIAN07/*.cpp"
	"${CMAKE_SOURCE_DIR}/GIAN07/*.CPP"
	"${CMAKE_SOURCE_DIR}/game/*.cpp"
	"${CMAKE_SOURCE_DIR}/game/codecs/*.cpp"
)

set(SSG_PLATFORM_SOURCES
	"${CMAKE_SOURCE_DIR}/MAIN/main_sdl.cpp"
	"${CMAKE_SOURCE_DIR}/platform/miniaudio/miniaudio.c"
	"${CMAKE_SOURCE_DIR}/platform/miniaudio/snd_miniaudio.cpp"
)

if(WIN32)
	file(GLOB _platform_sources CONFIGURE_DEPENDS
		"${CMAKE_SOURCE_DIR}/platform/windows/*.cpp"
		"${CMAKE_SOURCE_DIR}/platform/sdl/*.cpp"
	)
	list(APPEND SSG_PLATFORM_SOURCES
		${_platform_sources}
		"${CMAKE_SOURCE_DIR}/GIAN07/GIAN07.rc"
	)
else()
	file(GLOB _platform_sources CONFIGURE_DEPENDS
		"${CMAKE_SOURCE_DIR}/platform/c/*.cpp"
		"${CMAKE_SOURCE_DIR}/platform/sdl/*.cpp"
		"${CMAKE_SOURCE_DIR}/platform/pangocairo/*.cpp"
	)
	list(APPEND SSG_PLATFORM_SOURCES ${_platform_sources})
endif()

add_executable(GIAN07 ${SSG_GAME_SOURCES} ${SSG_PLATFORM_SOURCES})
set_target_properties(GIAN07 PROPERTIES
	CXX_MODULE_STD ON
	CXX_SCAN_FOR_MODULES ON
	DEBUG_POSTFIX "d"
	OUTPUT_NAME "GIAN07"
	WIN32_EXECUTABLE "$<BOOL:${WIN32}>"
)
target_compile_features(GIAN07 PRIVATE cxx_std_23)
target_compile_definitions(GIAN07 PRIVATE
	"$<$<CONFIG:Debug>:PBG_DEBUG>"
	"$<$<PLATFORM_ID:Windows>:WIN32>"
	"$<$<PLATFORM_ID:Linux>:LINUX;SDL3=1>"
)
target_include_directories(GIAN07 PRIVATE
	"${CMAKE_BINARY_DIR}/generated"
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/GIAN07"
)
target_link_libraries(GIAN07 PRIVATE ssg_dependencies)

if(MSVC)
	target_compile_options(GIAN07 PRIVATE
		/EHsc
		"$<$<CONFIG:Release>:/Zi>"
	)
	target_link_options(GIAN07 PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
else()
	# This must also be present while the standard library module is compiled.
	target_compile_options(GIAN07 PRIVATE -pthread)
endif()

find_program(DOTNET_EXECUTABLE dotnet)
if(DOTNET_EXECUTABLE)
	if(WIN32)
		set(SSG_LAUNCHER_RUNTIME "win-x64")
	else()
		set(SSG_LAUNCHER_RUNTIME "linux-x64")
	endif()
	add_custom_target(GIAN07_launcher
		COMMAND
			"${DOTNET_EXECUTABLE}" publish
			"${CMAKE_SOURCE_DIR}/launcher/GIAN07.Launcher.csproj"
			--configuration "$<CONFIG>"
			--runtime "${SSG_LAUNCHER_RUNTIME}"
			--self-contained true
			-p:PublishSingleFile=true
			-p:IncludeNativeLibrariesForSelfExtract=true
			-p:EnableCompressionInSingleFile=true
			-p:PublishTrimmed=false
			-p:DebugSymbols=false
			-p:DebugType=None
			--output
			"${CMAKE_SOURCE_DIR}/bin/launcher/$<CONFIG>/${SSG_LAUNCHER_RUNTIME}"
			--nologo
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		COMMENT "Publishing the Avalonia configuration launcher as one file"
		USES_TERMINAL
	)

	add_custom_target(SSG_all)
	add_dependencies(SSG_all GIAN07 GIAN07_launcher)
else()
	message(STATUS
		"dotnet was not found; the GIAN07_launcher and SSG_all targets are unavailable.")
endif()

install(TARGETS GIAN07 RUNTIME DESTINATION bin)
