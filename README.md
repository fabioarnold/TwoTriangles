# TwoTriangles
This app simply draws two (shaded) triangles, which is basically a native offline variant of an older version of [Shadertoy](http://shadertoy.com).

![Screencast](https://raw.githubusercontent.com/wiki/fabioarnold/TwoTriangles/images/screencast.gif)

## Features
* Edit OpenGL fragment shader files (GLSL 1.10) with your favorite text editor and watch saved changes appear near instantly
* Modify uniform values by dragging to see the effects in realtime
* Load and store uniform values to disk
* Built-in 3D camera with keyboard controls (WASD for moving, arrow keys for looking around)
* Load textures, cubemaps and HDR images

## Install dependencies

### OS X
Assuming you already have Xcode install dependencies using [Homebrew](http://brew.sh):

```
$ brew install scons pkg-config sdl2
```

### Arch Linux
Install dependencies using pacman:

```
# pacman -S base-devel scons pkg-config sdl2
```

## Build and run

Clone repository and submodules:

```
$ git clone --recursive https://github.com/fabioarnold/TwoTriangles.git
$ cd TwoTriangles
```

Build the third party libraries:

```
$ cd lib/imgui
$ sh build_staticlib.sh
$ cd ../stb
$ sh build_staticlib.sh
$ cd ../nativefiledialog/src
$ scons debug=0
$ cd ../../..
$ cd -
```

Build and run TwoTriangles: `sh build.sh run`
