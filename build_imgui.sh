#!/bin/bash
TARGET=libImGui.a
UNITYBUILD_FILE_CPP=lib/gamelib/third_party/imgui/imgui_glfw_gl2_ub.cpp
INCLUDE_DIRS="-Ilib/imgui -Ilib/gamelib/third_party/imgui"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-Os"
CFLAGS="$CFLAGS $INCLUDE_DIRS $RELEASE_FLAGS"

mkdir -p build
c++ $CFLAGS -c $UNITYBUILD_FILE_CPP -o ${TARGET}.o
EXIT_STATUS=$?
if [ $EXIT_STATUS = 0 ]; then
	ar rcs build/$TARGET ${TARGET}.o
	rm ${TARGET}.o
else
	exit 1
fi
