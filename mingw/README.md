# MinGW CMake Build

This folder contains a Windows cross-build CMake entrypoint and MinGW toolchain files.

## Configure (x64)

```bash
cmake -S mingw -B build/cmake-mingw-x64 \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/HonkordGL/mingw/toolchain-x86_64-win32.cmake \
  -DHONKORDGL_BUILD_EXAMPLES=ON
```

## Build (x64)

```bash
cmake --build build/cmake-mingw-x64 -j
```

## Configure (x86)

```bash
cmake -S mingw -B build/cmake-mingw-x86 \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/HonkordGL/mingw/toolchain-i686-win32.cmake \
  -DHONKORDGL_BUILD_EXAMPLES=ON
```

## Build examples only

```bash
cmake --build build/cmake-mingw-x64 -j --target MovingSquare Tetris Checkerboard AsteroidGame GPURaytracing SplitScreen CameraPlayer
```