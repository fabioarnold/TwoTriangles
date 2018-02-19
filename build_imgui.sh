#!/bin/bash
TARGET=libImGui.a
read -d '' UNITY_BUILD_CPP << EOF
#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_impl_glfw_gl2.cpp"
EOF
INCLUDE_DIRS="-Ilib/imgui -Ilib/gamelib/third_party/imgui"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS $INCLUDE_DIRS $RELEASE_FLAGS"

mkdir -p build
printf '%s' "$UNITY_BUILD_CPP" | c++ $CFLAGS -c -o ${TARGET}.o -x c++ -
EXIT_STATUS=$?
if [ $EXIT_STATUS = 0 ]; then
	ar rcs build/$TARGET ${TARGET}.o
	rm ${TARGET}.o
else
	exit 1
fi
