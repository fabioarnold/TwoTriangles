#!/bin/bash

OS_NAME="$(uname -s)"

TARGET="twotris"

if [[ $1 = "clean" ]]; then
	rm -r build
	pushd lib/nativefiledialog/src > /dev/null
	scons -c
	popd > /dev/null
	exit 0
fi

DEBUG_FLAGS="-Wall -O0 -g -DDEBUG"
RELEASE_FLAGS="-Os"
if [[ $1 = "release" ]]; then
	CFLAGS="$CFLAGS -std=c++11 $RELEASE_FLAGS"
else
	CFLAGS="$CFLAGS -std=c++11 $DEBUG_FLAGS"
fi
LDFLAGS="-Lbuild"

# gamelib
INCLUDE_DIRS="-Ilib/gamelib/src"

# GLFW
LIB_GLFW="`pkg-config --libs glfw3`"

# OpenGL and Windowing
LIB_OPENGL="-lGL -lGLEW"

# dear imgui
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/imgui -Ilib/gamelib/third_party/imgui"
LIB_IMGUI="-lImGui"
if [ ! -f build/libImGui.a ]; then
	echo "building dear imgui..."
	sh build_imgui.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# nativefiledialog
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/nativefiledialog/src/include"
LIB_NFD="-Llib/nativefiledialog/src -lnfd"
if [ ! -f lib/nativefiledialog/src/libnfd.a ]; then
	echo "building nativefiledialog..."
	pushd lib/nativefiledialog/src > /dev/null
	scons debug=0
	popd > /dev/null
fi

# stb_image
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/stb"
LIB_STB_IMAGE="-lstb_image"
CFLAGS="$CFLAGS -DUSE_STB_IMAGE"
if [ ! -f build/libstb_image.a ]; then
	echo "building stb_image..."
	sh build_stb_image.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# platform specific flags
if [[ $OS_NAME = "Darwin" ]]; then
	LIB_OPENGL="-framework OpenGL -framework Cocoa"
	LIB_NFD="$LIB_NFD -framework AppKit"
elif [[ $OS_NAME = "Linux" ]]; then
	CFLAGS="$CFLAGS -DLINUX_DESKTOP"
	LIB_NFD="$LIB_NFD `pkg-config --cflags --libs gtk+-3.0`"
fi

# final compiler flags
CFLAGS="$CFLAGS `pkg-config --cflags glfw3` $INCLUDE_DIRS"
LDFLAGS="$LDFLAGS $LIB_GLFW $LIB_OPENGL $LIB_IMGUI $LIB_NFD $LIB_STB_IMAGE"

mkdir -p build
c++ $CFLAGS src/main_glfw_ub.cpp $LDFLAGS -o build/$TARGET
EXIT_STATUS=$?
if [[ $EXIT_STATUS = 0 && $1 = "run" ]]; then
	./build/$TARGET
else
	exit $EXIT_STATUS
fi

