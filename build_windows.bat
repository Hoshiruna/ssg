@echo off
setlocal

if "%VCINSTALLDIR%" == "" (
	echo Error: The build must be run from within Visual Studio's `x64_x86 Cross Tools Command Prompt`.
	exit /b 1
)

sh ./submodules_check.sh ^
	libs/BLAKE3 ^
	libs/dr_libs ^
	libs/libogg ^
	libs/libvorbis ^
	libs/libwebp_lossless ^
	libs/miniaudio ^
	libs/SDL3
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --preset windows-msvc
if %errorlevel% neq 0 exit /b %errorlevel%

set "BUILD_CONFIG=%~1"
if "%BUILD_CONFIG%" == "" set "BUILD_CONFIG=all"

if /I "%BUILD_CONFIG%" == "Debug" goto build_debug
if /I "%BUILD_CONFIG%" == "bin/GIAN07d.exe" goto build_debug
if /I "%BUILD_CONFIG%" == "Release" goto build_release
if /I "%BUILD_CONFIG%" == "bin/GIAN07.exe" goto build_release
if /I "%BUILD_CONFIG%" == "all" goto build_all

echo Usage: %~nx0 [Debug^|Release^|all]
exit /b 2

:build_debug
cmake --build --preset windows-debug
exit /b %errorlevel%

:build_release
cmake --build --preset windows-release
exit /b %errorlevel%

:build_all
cmake --build --preset windows-debug
if %errorlevel% neq 0 exit /b %errorlevel%
cmake --build --preset windows-release
exit /b %errorlevel%
