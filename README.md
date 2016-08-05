# TwoTriangles
This app simply draws two (shaded) triangles, which is basically a native offline variant of an older version of [Shadertoy](http://shadertoy.com).

![Screencast](https://raw.githubusercontent.com/wiki/fabioarnold/TwoTriangles/images/screencast.gif)

## Features
* Edit OpenGL fragment shader files (GLSL 1.10) with your favorite text editor and watch saved changes appear near instantly
* Modify uniform values by dragging to see the effects in realtime
* Load and store uniform values to disk
* Built-in 3D camera with keyboard controls (WASD for moving, arrow keys for looking around)
* Load textures, cubemaps and HDR images

## Installing

### Windows
You can find a Windows build on the releases page: https://github.com/fabioarnold/TwoTriangles/releases

### Arch Linux
You can obtain a package from the AUR: https://aur.archlinux.org/packages/twotris-git

## Building from source

### Install dependencies

#### Windows
You need [Visual Studio 2015](https://www.visualstudio.com).

#### OS X
Assuming you already have Xcode installed, install dependencies using [Homebrew](http://brew.sh):

```
$ brew install scons pkg-config sdl2
```

#### Arch Linux
Install dependencies using pacman:

```
# pacman -S base-devel scons pkg-config glew sdl2
```

### Get the code

Clone repository and submodules:

```
$ git clone --recursive https://github.com/fabioarnold/TwoTriangles.git
$ cd TwoTriangles
```

### Build and run

#### OS X and Arch Linux

Build and run TwoTriangles: `sh build.sh run`

#### Windows

Open projects/visualstudio/TwoTriangles.sln in Visual Studio 2015 and the change the configuration from Debug to Release (I only configured Release x64 for now). Build the solution and place SDL.dll and glew32.dll, which you can find in the lib directory, in your target folder (e.g. projects/visualstudio/x64/Release). You should be able to run the program now.

## Credits
* [dear imgui](https://github.com/ocornut/imgui) by Omar Cornut
* [Native File Dialog](https://github.com/mlabbe/nativefiledialog) by Michael Labbe
* [Sean's Tool Box](https://github.com/nothings/stb) by Sean T. Barrett
