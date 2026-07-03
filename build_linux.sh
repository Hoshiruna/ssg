#!/bin/sh

case "${1:-}" in
	LauncherDebug|launcher-debug)
		exec dotnet build launcher/GIAN07.Launcher.csproj \
			--configuration Debug --nologo
		;;
	Launcher|launcher)
		exec dotnet publish launcher/GIAN07.Launcher.csproj \
			--configuration Release \
			--runtime linux-x64 \
			--self-contained true \
			-p:PublishSingleFile=true \
			-p:IncludeNativeLibrariesForSelfExtract=true \
			-p:EnableCompressionInSingleFile=true \
			-p:PublishTrimmed=false \
			-p:DebugSymbols=false \
			-p:DebugType=None \
			--output bin/launcher/Release/linux-x64 \
			--nologo
		;;
esac

./submodules_check.sh \
	libs/BLAKE3 \
	libs/dr_libs \
	libs/miniaudio \
	|| exit 1

cmake --preset linux || exit 1

case "${1:-all}" in
	Debug|debug|bin/GIAN07d)
		cmake --build --preset linux-debug
		;;
	Release|release|bin/GIAN07)
		cmake --build --preset linux-release
		;;
	LauncherDebug|launcher-debug)
		cmake --build --preset linux-launcher-debug
		;;
	Launcher|launcher)
		cmake --build --preset linux-launcher-release
		;;
	all)
		cmake --build --preset linux-debug &&
		cmake --build --preset linux-release &&
		cmake --build --preset linux-launcher-debug &&
		cmake --build --preset linux-launcher-release
		;;
	*)
		echo "Usage: $0 [Debug|Release|LauncherDebug|Launcher|all]" >&2
		exit 2
		;;
esac
