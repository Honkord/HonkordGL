# Metal backend

## Scope

Metal attach path in `Cocoa/MetalRendererContext.mm` when enabled.

## Requirements

macOS with Metal-capable GPU; Xcode/clang toolchain.

## CMake

`HONKORDGL_ENABLE_METAL` — optional Metal stripping not fully implemented.

## Public surface

`MetalIntegration.h`.

## Capability flag

`HONKORDGL_HAVE_METAL` / `IsMetalBackendAvailable()`.
