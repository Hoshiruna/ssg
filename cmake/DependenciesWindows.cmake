set(_sdl_root "${CMAKE_SOURCE_DIR}/libs/SDL3")
ssg_require_file("${_sdl_root}/include/SDL3/SDL.h" "libs/SDL3")

set(_sdl_sources)
foreach(_dir IN ITEMS
	"src"
	"src/atomic"
	"src/audio"
	"src/audio/directsound"
	"src/audio/disk"
	"src/audio/dummy"
	"src/audio/wasapi"
	"src/camera"
	"src/camera/dummy"
	"src/camera/mediafoundation"
	"src/core"
	"src/core/windows"
	"src/cpuinfo"
	"src/dialog"
	"src/dialog/windows"
	"src/dynapi"
	"src/events"
	"src/filesystem"
	"src/filesystem/windows"
	"src/gpu"
	"src/gpu/d3d12"
	"src/gpu/vulkan"
	"src/haptic"
	"src/haptic/hidapi"
	"src/haptic/windows"
	"src/hidapi"
	"src/io"
	"src/io/generic"
	"src/io/windows"
	"src/joystick"
	"src/joystick/hidapi"
	"src/joystick/virtual"
	"src/joystick/windows"
	"src/libm"
	"src/loadso/windows"
	"src/locale"
	"src/locale/windows"
	"src/main"
	"src/main/generic"
	"src/main/windows"
	"src/misc"
	"src/misc/windows"
	"src/power"
	"src/power/windows"
	"src/process"
	"src/process/windows"
	"src/render"
	"src/render/direct3d"
	"src/render/direct3d11"
	"src/render/direct3d12"
	"src/render/gpu"
	"src/render/opengl"
	"src/render/opengles2"
	"src/render/vulkan"
	"src/render/software"
	"src/sensor"
	"src/sensor/windows"
	"src/stdlib"
	"src/storage"
	"src/storage/generic"
	"src/thread"
	"src/thread/windows"
	"src/time"
	"src/time/windows"
	"src/timer"
	"src/timer/windows"
	"src/tray"
	"src/tray/windows"
	"src/video"
	"src/video/dummy"
	"src/video/offscreen"
	"src/video/windows"
	"src/video/yuv2rgb"
)
	ssg_append_glob(_sdl_sources "${_sdl_root}/${_dir}/*.c")
endforeach()

ssg_append_glob(_sdl_sources "${_sdl_root}/src/joystick/gdk/*.cpp")
ssg_append_glob(_sdl_sources "${_sdl_root}/src/video/windows/*.cpp")
list(APPEND _sdl_sources
	"${_sdl_root}/src/core/windows/SDL_gameinput.cpp"
	"${_sdl_root}/src/thread/generic/SDL_syscond.c"
	"${_sdl_root}/src/thread/generic/SDL_sysrwlock.c"
	"${_sdl_root}/src/core/windows/version.rc"
)

foreach(_excluded IN ITEMS
	"${_sdl_root}/src/audio/SDL_audiodev.c"
	"${_sdl_root}/src/events/imKStoUCS.c"
	"${_sdl_root}/src/events/SDL_keysym_to_scancode.c"
	"${_sdl_root}/src/events/SDL_scancode_tables.c"
	"${_sdl_root}/src/render/direct3d11/SDL_render_winrt.cpp"
	"${_sdl_root}/src/render/direct3d12/SDL_render_winrt.cpp"
)
	list(REMOVE_ITEM _sdl_sources "${_excluded}")
endforeach()

add_library(ssg_sdl3 SHARED ${_sdl_sources})
add_library(SDL3::SDL3 ALIAS ssg_sdl3)
set_target_properties(ssg_sdl3 PROPERTIES
	CXX_SCAN_FOR_MODULES OFF
	DEBUG_POSTFIX "d"
	OUTPUT_NAME "SDL3"
)
target_compile_definitions(ssg_sdl3 PRIVATE DLL_EXPORT)
target_include_directories(ssg_sdl3
	PUBLIC "${_sdl_root}/include"
	PRIVATE
		"${_sdl_root}/include/build_config"
		"${_sdl_root}/src"
)
target_link_libraries(ssg_sdl3 PRIVATE
	advapi32
	imm32
	gdi32
	kernel32
	ole32
	oleaut32
	setupapi
	shell32
	user32
	uuid
	version
	winmm
)

set(_ogg_root "${CMAKE_SOURCE_DIR}/libs/libogg")
set(_vorbis_root "${CMAKE_SOURCE_DIR}/libs/libvorbis")
ssg_require_file("${_ogg_root}/include/ogg/ogg.h" "libs/libogg")
ssg_require_file("${_vorbis_root}/include/vorbis/vorbisfile.h" "libs/libvorbis")

file(GLOB _ogg_sources CONFIGURE_DEPENDS "${_ogg_root}/src/*.c")
add_library(ssg_ogg STATIC ${_ogg_sources})
target_include_directories(ssg_ogg PUBLIC "${_ogg_root}/include")

file(GLOB _vorbis_sources CONFIGURE_DEPENDS "${_vorbis_root}/lib/*.c")
list(FILTER _vorbis_sources EXCLUDE REGEX
	"/(barkmel|misc|psytune|tone|vorbisenc)\\.c$")
add_library(ssg_vorbis STATIC ${_vorbis_sources})
target_include_directories(ssg_vorbis PUBLIC "${_vorbis_root}/include")
target_link_libraries(ssg_vorbis PUBLIC ssg_ogg)

ssg_add_blake3(ssg_blake3)

set(_webp_root "${CMAKE_SOURCE_DIR}/libs/libwebp_lossless")
ssg_require_file("${_webp_root}/src/webp/encode.h" "libs/libwebp_lossless")

set(_webp_sources)
foreach(_pattern IN ITEMS
	"src/dsp/alpha_processing*.c"
	"src/dsp/cpu.c"
	"src/dsp/enc*.c"
	"src/dsp/lossless*.c"
	"src/enc/*.c"
	"src/utils/*.c"
)
	ssg_append_glob(_webp_sources "${_webp_root}/${_pattern}")
endforeach()

list(FILTER _webp_sources EXCLUDE REGEX
	"/(picture_psnr_enc|near_lossless_enc|bit_reader_utils|filters_utils|huffman_utils|rescaler_utils|quant_levels_dec_utils|random_utils)\\.c$")
list(FILTER _webp_sources EXCLUDE REGEX
	"_(neon|mips[^/]*|msa)\\.c$")

set(_webp_x86_sources ${_webp_sources})
list(FILTER _webp_x86_sources INCLUDE REGEX "_(sse2|sse41|avx2)\\.c$")
if(_webp_x86_sources)
	set_source_files_properties(${_webp_x86_sources}
		PROPERTIES COMPILE_OPTIONS "/arch:SSE2")
endif()

add_library(ssg_webp STATIC ${_webp_sources})
target_compile_definitions(ssg_webp PRIVATE
	WEBP_DISABLE_STATS
	WEBP_NEAR_LOSSLESS=0
	WEBP_REDUCE_SIZE
	WEBP_USE_THREAD
)
target_include_directories(ssg_webp
	PUBLIC "${_webp_root}/src"
	PRIVATE "${_webp_root}"
)

add_library(ssg_dependencies INTERFACE)
target_compile_definitions(ssg_dependencies INTERFACE SDL3=1)
target_link_libraries(ssg_dependencies INTERFACE
	ssg_sdl3
	ssg_blake3
	ssg_ogg
	ssg_vorbis
	ssg_webp
)
