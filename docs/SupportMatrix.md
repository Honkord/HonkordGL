# HonkordGL support matrix

Tiers (see `DevelopmentPlan.md`):

| Tier | Meaning |
|------|---------|
| **1** | Built in CI, install rules tested, headers checked, documented dependencies. |
| **2** | Configures and compiles; some backends stubbed or partial. |
| **3** | Detected in code macros only; not necessarily buildable in CMake. |

## Tier 1 (this repository today)

| OS | Compiler | Window | GL family | Vulkan | D3D11 | Metal | Audio |
|----|----------|--------|-----------|--------|-------|-------|-------|
| Linux desktop | GCC / Clang | X11 and/or Wayland (CMake options) | EGL + GL + GLX paths | Optional (`HONKORDGL_ENABLE_VULKAN`) | — | — | ALSA or PipeWire (optional) |
| Windows | MSVC / MinGW | Win32 | WGL + OpenGL | Stubs / noop swapchain | Optional (`HONKORDGL_ENABLE_D3D11`) | — | PipeWire TU (stubs on Windows) / path varies |
| macOS | Clang | Cocoa | NSOpenGL | MoltenVK / loader path in-tree | — | Optional (`HONKORDGL_ENABLE_METAL`) | Core Audio |

**CMake feature switches** are listed in `cmake/HonkordGL.cmake` (options prefixed `HONKORDGL_ENABLE_`). Generated capability flags are in the installed header `HonkordGL/BuildFeatures.h` and the runtime helpers in `HonkordGL/BackendCapabilities.h`.

## Tier 2 / 3 (planned or experimental)

Android, BSD desktop, Raspberry Pi dedicated builds, Emscripten, headless Linux — see `DevelopmentPlan.md` §1 and platform-specific notes under `docs/platforms/`.

## CI coverage

GitHub Actions workflow `.github/workflows/ci.yml` exercises **Linux** matrix builds (GCC/Clang, X11-only, Wayland-only, Vulkan off, audio off, software-only profile, static lib), public header boundary script, install + CMake consumer smoke test, plus **Windows (MSVC)** and **macOS** configure/build with the same header check and install/consumer smoke path.
