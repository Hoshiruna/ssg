#!/bin/sh

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
	all)
		cmake --build --preset linux-debug &&
		cmake --build --preset linux-release
		;;
	*)
		echo "Usage: $0 [Debug|Release|all]" >&2
		exit 2
		;;
esac
