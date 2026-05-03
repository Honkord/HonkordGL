# MinGW CMake Build

Toolchain files live under **`cmake/toolchains/`**; **`mingw/toolchain-*.cmake`** includes them for compatibility.

## Configure (x64)

```bash
cmake -S . -B build/cmake-mingw-x64 \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/HonkordGL/cmake/toolchains/mingw-x86_64.cmake \
  -DHONKORDGL_BUILD_EXAMPLES=ON
```

## Build (x64)

```bash
cmake --build build/cmake-mingw-x64 -j
```

## Configure (x86)

```bash
cmake -S . -B build/cmake-mingw-x86 \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/HonkordGL/cmake/toolchains/mingw-i686.cmake \
  -DHONKORDGL_BUILD_EXAMPLES=ON
```

## Build examples only

```bash
cmake --build build/cmake-mingw-x64 -j --target MovingSquare Tetris Checkerboard AsteroidGame GPURaytracing SplitScreen CameraPlayer
```

## Legacy

`cmake -S mingw` still works (thin wrapper around the root project).
