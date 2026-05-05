# Linux platform contract

## Purpose

Desktop Linux builds use selectable **X11**, **Wayland**, and optional **DRM** integration layers. GPU paths use **EGL**, **GL**, **GLX**, and optionally **Vulkan**.

## System packages (typical Debian/Ubuntu names)

- **X11**: `libx11-dev`, `libxrandr-dev`, `libxi-dev`, `libxcursor-dev`
- **Wayland**: `libwayland-dev`, wayland-protocol sources or bundled generated C under `src/Wayland/generated/`
- **OpenGL**: `libegl1-mesa-dev`, `libgl1-mesa-dev`
- **Vulkan** (optional): `libvulkan-dev`
- **ALSA** (optional audio): `libasound2-dev`
- **PipeWire** (optional audio): `libpipewire-0.3-dev`, `pkg-config`

## CMake options

- `HONKORDGL_ENABLE_X11`, `HONKORDGL_ENABLE_WAYLAND` — at least one must be `ON` for desktop Linux.
- `HONKORDGL_ENABLE_DRM` — defines `HONKORDGL_ENABLE_DRM` for full DRM dequeue paths in `EventsDRM.cpp`.
- `HONKORDGL_ENABLE_VULKAN` — `AUTO` | `ON` | `OFF`.
- `HONKORDGL_ENABLE_AUDIO`, `HONKORDGL_ENABLE_ALSA`, `HONKORDGL_ENABLE_PIPEWIRE`.

## Link libraries

See the `target_link_libraries` block for `UNIX AND NOT APPLE` in `cmake/HonkordGL.cmake` (X11, Wayland client/egl/cursor, EGL, GL, Vulkan::Vulkan when enabled, ALSA or PipeWire when enabled).

## Runtime

X11/Wayland compositor, EGL/GL drivers, optional Vulkan loader, ALSA or PipeWire stack for audio.

## Unsupported behavior

Integration entry points return `UNSUPPORTED_PLATFORM`, `UNSUPPORTED_BACKEND`, or stub results when the matching TU is not compiled or the backend is inactive — see integration headers.
