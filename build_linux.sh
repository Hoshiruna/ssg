#!/bin/sh

case "${1:-}" in
	ConfiguratorDebug|configurator-debug)
		exec dotnet build configurator/GIAN07.Configurator.csproj \
			--configuration Debug --nologo
		;;
	Configurator|configurator)
		exec dotnet publish configurator/GIAN07.Configurator.csproj \
			--configuration Release \
			--runtime linux-x64 \
			--self-contained true \
			-p:PublishSingleFile=true \
			-p:IncludeNativeLibrariesForSelfExtract=true \
			-p:EnableCompressionInSingleFile=true \
			-p:PublishTrimmed=false \
			-p:DebugSymbols=false \
			-p:DebugType=None \
			--output bin/configurator/Release/linux-x64 \
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
	ConfiguratorDebug|configurator-debug)
		cmake --build --preset linux-configurator-debug
		;;
	Configurator|configurator)
		cmake --build --preset linux-configurator-release
		;;
	all)
		cmake --build --preset linux-debug &&
		cmake --build --preset linux-release &&
		cmake --build --preset linux-configurator-debug &&
		cmake --build --preset linux-configurator-release
		;;
	*)
		echo "Usage: $0 [Debug|Release|ConfiguratorDebug|Configurator|all]" >&2
		exit 2
		;;
esac
