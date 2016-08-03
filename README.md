# TwoTriangles
This app simply draws two (shaded) triangles, which is basically a native offline variant of an older version of [Shadertoy](http://shadertoy.com).

## Features
* Edit OpenGL fragment shader files (GLSL 1.10) with your favorite text editor and watch saved changes appear near instantly
* Modify uniform values by dragging to see the effects in realtime
* Load and store uniform values to disk
* Built-in 3D camera with keyboard controls (WASD for moving, arrow keys for looking)
* Load textures, cubemaps and HDR images

## Build and run on OS X
Clone repository and submodules:

```
$ git clone --recursive https://github.com/fabioarnold/TwoTriangles.git`
$ cd TwoTriangles
```

Install all dependencies using [Homebrew](http://brew.sh):
`brew install scons pkg-config sdl2`

Build imgui static lib

```
$ cd lib/imgui
$ sh build_staticlib.sh
$ cd -
```

Build stb_image static lib

```
$ cd lib/stb
$ sh build_staticlib.sh
$ cd -
```

Build nativefiledialog

```
$ cd lib/nativefiledialog/src
$ scons debug=0
$ cd -
```

Build and run TwoTriangles: `sh build.sh run`
