# macOS platform contract

## Purpose

**Cocoa** windowing, **NSOpenGL**, **Metal** renderer context, **Core Audio**, optional **Vulkan** loader path via `VulkanRendererContext.cpp`.

## Toolchain

Apple Clang; Objective-C++ enabled in CMake when `APPLE`.

## Frameworks

`Cocoa`, `OpenGL`, `Metal`, `QuartzCore`, `CoreAudio`, `AudioToolbox`, `IOKit` — see `cmake/HonkordGLApple.cmake`.

## CMake options

- `HONKORDGL_ENABLE_METAL` — documented; Metal sources may still link when `OFF` (warning).
- `HONKORDGL_ENABLE_AUDIO` — maps to Core Audio unless disabled (`HONKORDGL_AUDIO_DISABLED`).

## Native handles

Use `MetalIntegration.h` / `VulkanIntegration.h`; cast opaque handles only after including Apple/Vulkan headers in implementation files.
