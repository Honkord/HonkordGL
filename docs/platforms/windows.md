# Windows platform contract

## Purpose

Win32 windowing, **WGL** + OpenGL, **Direct3D 11** swapchain path, input/audio stacks as compiled in `cmake/HonkordGL.cmake`.

## Toolchain

MSVC or MinGW-w64; C++17.

## System / SDK

Windows SDK (Win32, DXGI, D3D11, D3D compiler). OpenGL via `opengl32`.

## CMake options

- `HONKORDGL_ENABLE_D3D11` — documented; full removal of D3D11 sources is not complete when `OFF` (warning at configure time).
- `HONKORDGL_ENABLE_AUDIO` — disables audio implementation via `HONKORDGL_AUDIO_DISABLED`.

## Link libraries

`user32`, `gdi32`, `winmm`, `ole32`, `ws2_32`, `d3d11`, `dxgi`, `d3dcompiler`, `opengl32` (see `HonkordGL.cmake`).

## Native handles

Use `Direct3DIntegration.h` and Win32/D3D headers in your TU after querying opaque handles.
