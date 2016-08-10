#!/bin/bash
TARGET=libstb_image.a
UNITYBUILD_CPP_FILE="lib/gamelib/third_party/stb/stb_image.cpp"
INCLUDE_DIRS="-Ilib/stb"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS $INCLUDE_DIRS $RELEASE_FLAGS"

mkdir -p build
cc $CFLAGS -c ${UNITYBUILD_CPP_FILE} -o ${UNITYBUILD_CPP_FILE}.o
EXIT_STATUS=$?
if [ $EXIT_STATUS = 0 ]; then
	ar rcs build/$TARGET ${UNITYBUILD_CPP_FILE}.o
	rm ${UNITYBUILD_CPP_FILE}.o
else
	exit 1
fi
