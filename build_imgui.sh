#!/bin/bash
TARGET=libImGui.a
UNITYBUILD_CPP_FILE="lib/gamelib/third_party/imgui/imgui_sdl2_gl2_ub.cpp"
INCLUDE_DIRS="-Ilib/imgui -Ilib/gamelib/third_party/imgui"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS `sdl2-config --cflags` $INCLUDE_DIRS $RELEASE_FLAGS"

mkdir -p build
c++ $CFLAGS -c ${UNITYBUILD_CPP_FILE} -o ${UNITYBUILD_CPP_FILE}.o
EXIT_STATUS=$?
if [ $EXIT_STATUS = 0 ]; then
	ar rcs build/$TARGET ${UNITYBUILD_CPP_FILE}.o
	rm ${UNITYBUILD_CPP_FILE}.o
else
	exit 1
fi
