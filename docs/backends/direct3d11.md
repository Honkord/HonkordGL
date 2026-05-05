# Direct3D 11 backend

## Scope

Windows swapchain and texture helpers in `Win32/D3D11RendererContext.cpp`.

## Requirements

Windows SDK, DXGI, D3D11.

## CMake

`HONKORDGL_ENABLE_D3D11` (full source exclusion when `OFF` is still WIP).

## Public surface

`Direct3DIntegration.h` for native handle queries.

## Capability flag

`HONKORDGL_HAVE_D3D11` / `IsDirect3D11BackendAvailable()` (Windows builds).
