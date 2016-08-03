#!/bin/bash

OS_NAME="$(uname -s)"

TARGET="twotris"

DEBUG_FLAGS="-O0 -g -DDEBUG"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS -std=c++11 $DEBUG_FLAGS"

# gamelib
INCLUDE_DIRS="-Ilib/gamelib/src"

# SDL2 and SDL2_net
LIB_SDL2="`pkg-config --libs sdl2 SDL2_net`"

# OpenGL and Windowing
LIB_OPENGL="-lGL -lGLEW"

# imgui
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/imgui"
LIB_IMGUI="-Llib/imgui/build -lImGui"

# nativefiledialog
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/nativefiledialog/src/include"
LIB_NFD="-Llib/nativefiledialog/src -lnfd"

# stb_image
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/stb"
LIB_STB_IMAGE="-Llib/stb/build -lstb_image"
CFLAGS="$CFLAGS -DUSE_STB_IMAGE"

# platform specific flags
if [[ $OS_NAME == "Darwin" ]]; then
	LIB_OPENGL="-framework OpenGL -framework Cocoa"
	LIB_NFD="$LIB_NFD -framework AppKit"
elif [[ $OS_NAME == "Linux" ]]; then
	CFLAGS="$CFLAGS -DLINUX_DESKTOP"
	LIB_NFD="$LIB_NFD `pkg-config --cflags --libs gtk+-3.0`"
fi

# final compiler flags
CFLAGS="$CFLAGS `pkg-config --cflags sdl2` $INCLUDE_DIRS"
LDFLAGS="$LDFLAGS $LIB_SDL2 $LIB_OPENGL $LIB_IMGUI $LIB_NFD $LIB_STB_IMAGE"

if [ ! -d "build" ]; then
	mkdir build
fi
c++ $CFLAGS src/main_sdl2_ub.cpp $LDFLAGS -o build/$TARGET
EXIT_STATUS=$?
if [[ $EXIT_STATUS = 0 && $1 = "run" ]]; then
	./build/$TARGET
fi
