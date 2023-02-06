# Simple FFmpeg Player
A simple ffmpeg player for Windows and Linux.

## Description
This is the simplest way of programming player using ffmpeg libraries without additional dependencies. No SDL, no multithreading, no Boost and other stuff.
It uses only ffmpeg libraries for decoding video (without audio) and OpenGL for rendering.

## Building
By default all builds are statically linked and debug.

### Windows
To build on Windows you need a C compiler. So you can install it with Visual Studio and after installation just run/click **build.bat**. All needed ffmpeg **.lib* files, headers files are included. You can find binary in *windows/bin* folder after compilation finished.

### Linux
To build on Linux you need a C compiler, X11 dev files and ffmpeg-dev libraries. For deb based distributions: **build-essential**, **libavformat-dev**, **libavcodec-dev**, **libswscale-dev**, **libgl-dev**.
Just run *build.sh* and you will find your binary in linux/bin.

## License
The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Acknowledgments

Inspiration:
* [simple-opengl-loader](https://github.com/tsherif/simple-opengl-loader)
* [simplest_ffmpeg_player](https://github.com/leixiaohua1020/simplest_ffmpeg_player)
* [ffplay](https://github.com/FFmpeg/FFmpeg/blob/master/fftools/ffplay.c)