# HonkordGL Development Plan

## Portability Goal

Deliver a **tiered, documented support matrix** (see [`SupportMatrix.md`](SupportMatrix.md)), **feature-selected CMake builds**, **predictable stubs** for unavailable backends, and **CI plus install rules** so downstream projects can depend on HonkordGL without dragging in every desktop dependency.

## Current Portability Status

The library is partly portable today. Public headers use opaque native handles and avoid directly exposing vendor SDK types in the main API surface. Platform detection is centralized through `PlatformAdapter.h`, and several backend-specific integration APIs return stable unsupported-platform results when unavailable.

The main limitation is that portability is broader in the headers than in the build system. CMake currently supports Windows, Linux desktop, and macOS only. Other platforms may be detected by macros, but detection does not mean the platform is buildable, tested, or release-ready.

For a **tabular gap list** (release status, monolithic target, CMake completeness, CI coverage, experimental targets), see **§ Formal gaps (stated goals vs. current reality)** below.

The project should describe portability in tiers:

- Tier 1: fully built, tested, and documented platforms.
- Tier 2: platforms or backends that compile with partial/stubbed functionality.
- Tier 3: detected or planned platforms without complete build/runtime support.
- Unsupported: targets outside the active support matrix.

## Formal gaps (stated goals vs. current reality)

This section records **acknowledged gaps** so the plan, support matrix, and consumer expectations stay aligned. It is normative for “what is true today” versus “what the sections below recommend.”

| Gap | Current reality | Target / remediation |
|-----|-----------------|----------------------|
| **Release lifecycle** | **Version 1 is still in active development.** Public API, CMake option names, and default feature sets may change before a declared stable release. | Meet **§ Definition Of Done** and **§ Recommended Milestones** before claiming stability; record breaking changes in `CHANGELOG.md`. |
| **Build modularization** | The shipped library is still primarily a **single CMake target** (`HonkordGL`) that aggregates core, windowing, audio, and GPU paths. | Implement **§ 2 Split Core And Backend Modules** so consumers can link only the slices they need. |
| **Feature-selected CMake** | Vulkan, audio, X11/Wayland, **D3D11**, and **Metal** are selectable; disabling D3D11/Metal swaps in stub translation units and drops those link libraries. **`HONKORDGL_ENABLE_OPENGL=OFF` is rejected** until GpuRenderer / GLSL paths are split; **`HONKORDGL_ENABLE_SOFTWARE_ONLY`** disables Vulkan + D3D11 + Metal at configure time (OpenGL stack may remain). Remaining gaps: monolithic target, DRM/KMS granularity — see [`KnownLimitations.md`](KnownLimitations.md). | Finish **§ 2–3** for optional OpenGL-off desktop builds and smaller install footprints. |
| **Header vs. build portability** | Public headers and platform **macros can imply wider portability** than the **CMake presets and CI jobs** actually configure, build, and test. **Tier 2** may mean “builds with stubs or partial behavior”; **Tier 3** may mean “detected in code only.” | Keep [`SupportMatrix.md`](SupportMatrix.md) Tier labels accurate; avoid implying Tier 1 support for a platform that only has header-level detection. |
| **CI vs. Tier 1 claims** | **Linux**, **Windows (MSVC)**, and **macOS** workflows configure and build the library, run `scripts/check_public_headers.py`, install to a prefix, and compile `tests/cmake_consumer/`. Linux additionally matrix-tests compilers and several `HONKORDGL_ENABLE_*` combinations. | Add MinGW / Xcode generators only if listed as Tier 1 in the support matrix. |
| **Experimental targets** | Android, Emscripten, BSD, headless Linux, etc. may appear in docs or **conditional code paths** without production CMake presets or Tier 1 guarantees. | Promote platforms per **§ Milestone 4**—thin vertical slice, then CI, then tier promotion. |
| **Repository and release hygiene** | Merge conflicts, stray build artifacts, or a dirty tree block reproducible “clean clone” verification. | Follow **§ 12 Clean Release Hygiene** and **Milestone 1** (resolve conflicts, no committed binaries, documented install flow). |

**Related:** runtime and backend caveats that affect behavior even when the build succeeds are listed in [`KnownLimitations.md`](KnownLimitations.md).

## 1. Define A Clear Support Matrix

The project should document exactly which operating systems, compilers, graphics APIs, audio APIs, and windowing systems are supported.

Recommended initial support matrix:

- Linux desktop: X11 and/or Wayland, OpenGL/EGL, optional Vulkan, ALSA or PipeWire.
- Windows: Win32, WGL/OpenGL, Direct3D 11, optional Vulkan in the future.
- macOS: Cocoa, NSOpenGL or Metal, CoreAudio.

Planned or experimental targets should be listed separately:

- Android: native app glue, OpenGL ES, AAudio.
- BSD: X11-based windowing where available.
- Raspberry Pi: X11 or EGL/GLES-focused build.
- Emscripten: browser canvas, WebGL/WebAudio-style backend.
- Headless Linux: software-only or offscreen rendering.

Each platform entry should state whether the following work:

- Configure and build.
- Public headers compile standalone.
- Window creation.
- Event processing.
- Software framebuffer present.
- OpenGL or OpenGL ES rendering.
- Vulkan, Metal, or Direct3D integration.
- Audio.
- Joystick/input.
- Examples.
- Tests.

## 2. Split Core And Backend Modules

The current single library target pulls together many responsibilities. For better portability, the codebase should move toward smaller targets that can be enabled independently.

Recommended target layout:

- `HonkordGLCore`: public types, platform adapter, math, renderer-neutral objects, common utilities.
- `HonkordGLWindow`: portable window and event abstraction.
- `HonkordGLAudio`: common audio API with platform backends.
- `HonkordGLSoftware`: software renderer, CPU blit paths, text UI.
- `HonkordGLGpuOpenGL`: OpenGL/WGL/GLX/EGL/NSOpenGL support.
- `HonkordGLGpuVulkan`: Vulkan renderer and integration helpers.
- `HonkordGLGpuD3D11`: Direct3D 11 renderer and native handle export.
- `HonkordGLGpuMetal`: Metal renderer and native handle export.

This lets consumers build only what their platform supports. It also avoids forcing Linux users to install X11, Wayland, ALSA, EGL, GL, and Vulkan dependencies when they only need a subset.

## 3. Make CMake Feature-Based

CMake should expose feature switches for platform and backend selection instead of linking all desktop dependencies by default.

Recommended options:

```cmake
option(HONKORDGL_ENABLE_X11 "Enable X11 window backend" ON)
option(HONKORDGL_ENABLE_WAYLAND "Enable Wayland window backend" ON)
option(HONKORDGL_ENABLE_DRM "Enable Linux KMS/DRM backend" OFF)
option(HONKORDGL_ENABLE_OPENGL "Enable OpenGL renderer support" ON)
option(HONKORDGL_ENABLE_VULKAN "Enable Vulkan renderer support" AUTO)
option(HONKORDGL_ENABLE_D3D11 "Enable Direct3D 11 renderer support" ON)
option(HONKORDGL_ENABLE_METAL "Enable Metal renderer support" ON)
option(HONKORDGL_ENABLE_AUDIO "Enable audio support" ON)
option(HONKORDGL_ENABLE_ALSA "Enable ALSA audio backend" ON)
option(HONKORDGL_ENABLE_PIPEWIRE "Enable PipeWire audio backend" OFF)
option(HONKORDGL_ENABLE_SOFTWARE_ONLY "Build without GPU backends" OFF)
```

Each option should only add sources, include directories, compile definitions, and libraries required by that backend. If a dependency is missing, CMake should either disable the feature with a warning or fail only when the user explicitly requested it.

## 4. Keep Public Headers Vendor-Neutral

The public API should continue avoiding direct includes of vendor SDK headers such as `windows.h`, Vulkan, Metal, Direct3D, EGL, GL, or platform internals. The existing public-header check is valuable and should stay enabled in CI.

Public headers should prefer:

- Opaque handles.
- HonkordGL-owned enums.
- Plain C/C++ standard-library types.
- Forward declarations where possible.
- Backend-specific escape hatches that are clearly labeled.

Backend-specific APIs should document how to cast opaque handles after the application includes the matching vendor SDK header.

## 5. Standardize Optional Backend Stubs

Every optional backend should follow the same unsupported-platform pattern.

Rules:

- Public function declarations may exist on every platform.
- Unsupported platforms return stable result codes.
- Missing runtime backends return `UNSUPPORTED_BACKEND`.
- Invalid arguments return `INVALID_ARGUMENT`.
- Missing compile-time support returns `UNSUPPORTED_PLATFORM` or a dedicated `NOT_COMPILED` code.
- Native handle query functions should be safe to call even when the backend is unavailable.

Recommended query helpers:

```cpp
bool IsOpenGLBackendAvailable() noexcept;
bool IsVulkanBackendAvailable() noexcept;
bool IsDirect3D11BackendAvailable() noexcept;
bool IsMetalBackendAvailable() noexcept;
bool IsAudioBackendAvailable() noexcept;
```

These make portable application code easier to write because users can query capabilities before selecting a renderer.

## 6. Fix Windows Export Macros

The public export macro should be corrected and made explicit. The current code appears to check `_MSV_VER`, which should likely be `_MSC_VER`.

Recommended direction:

```cpp
#if defined(_WIN32)
#  if defined(HONKORDGL_BUILDING_DLL)
#    define HONKORDGL_API __declspec(dllexport)
#  elif defined(HONKORDGL_USING_DLL)
#    define HONKORDGL_API __declspec(dllimport)
#  else
#    define HONKORDGL_API
#  endif
#else
#  if defined(HONKORDGL_BUILDING_DLL) && defined(__GNUC__)
#    define HONKORDGL_API __attribute__((visibility("default")))
#  else
#    define HONKORDGL_API
#  endif
#endif
```

CMake should define these macros based on whether the library is being built shared or static.

## 7. Improve Linux Portability

Linux support should be split into multiple selectable configurations instead of assuming a full desktop stack.

Important Linux build profiles:

- X11-only.
- Wayland-only.
- X11 + Wayland.
- DRM/KMS-only.
- Software-only.
- Headless/offscreen.
- OpenGL desktop.
- OpenGL ES/EGL.
- Vulkan-enabled.
- Vulkan-stubbed.
- ALSA audio.
- PipeWire audio.
- No-audio.

This would make the library usable in containers, CI, WSL, embedded Linux, Raspberry Pi, minimal distributions, and servers.

## 8. Add Continuous Integration

Portability should be proven continuously.

Recommended CI matrix:

- Ubuntu GCC, default desktop build.
- Ubuntu Clang, default desktop build.
- Ubuntu software-only build.
- Ubuntu X11-only build.
- Ubuntu Wayland-only build.
- Ubuntu Vulkan disabled.
- Ubuntu Vulkan enabled.
- Windows MSVC.
- Windows MinGW, if supported.
- macOS Clang.
- Public header boundary check.
- Public header self-containment check.
- CMake install/package check.

The CI should verify both configure-time behavior and compile/link behavior. For platforms where runtime windows cannot be opened, tests should still validate headers, stubs, software paths, and capability queries.

## 9. Add Packaging And Install Support

Consumers should be able to install and find the library using standard build-system mechanisms.

Recommended packaging work:

- Add `install(TARGETS HonkordGL ...)`.
- Install public headers under `include/HonkordGL`.
- Generate and install `HonkordGLConfig.cmake`.
- Generate and install `HonkordGLConfigVersion.cmake`.
- Export CMake targets with namespace `HonkordGL::`.
- Provide platform-specific pkg-config files when useful.
- Document system packages required by each backend.

Example consumer target:

```cmake
find_package(HonkordGL CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE HonkordGL::HonkordGL)
```

## 10. Create A Portability Test Suite

The project should include tests that specifically protect portability.

Useful tests:

- Every public header compiles by itself.
- Umbrella headers compile in C++17 mode.
- Public headers do not include forbidden vendor SDKs.
- Unsupported backend functions return expected result codes.
- Capability query functions match the compiled feature set.
- Software renderer builds without GPU libraries.
- Examples can be disabled entirely.
- Shared and static builds both link.
- Installed package can be consumed by a small external CMake project.

## 11. Document Backend Contracts

Each backend should have a small document explaining:

- Required system packages.
- Required compile definitions.
- Required link libraries.
- Runtime dependencies.
- Supported renderer modes.
- Native handles exposed through integration APIs.
- Expected unsupported-platform behavior.
- Known limitations.

Suggested documents:

- `docs/platforms/linux.md`
- `docs/platforms/windows.md`
- `docs/platforms/macos.md`
- `docs/backends/opengl.md`
- `docs/backends/vulkan.md`
- `docs/backends/direct3d11.md`
- `docs/backends/metal.md`
- `docs/backends/audio.md`

## 12. Clean Release Hygiene

Before claiming release-grade portability, the repository should be cleaned up.

Required cleanup:

- Resolve merge conflicts in markdown and license files.
- Avoid committing built binaries.
- Keep examples and tests optional for library consumers.
- Document dependency installation commands.
- Add a known limitations section.
- Add a changelog or release checklist.
- Verify clean clone, configure, build, install, and consume flow.

## Recommended Milestones

### Milestone 1: Desktop Portability Baseline

- Resolve current merge conflicts.
- Fix Windows export macros.
- Add support matrix documentation.
- Add CI for Linux, Windows, and macOS.
- Keep public-header boundary check enabled.

### Milestone 2: Feature-Based Builds

- Split CMake options by backend.
- Support Linux X11-only and Wayland-only builds.
- Support no-audio and software-only builds.
- Ensure Vulkan can be explicitly enabled, disabled, or stubbed.

### Milestone 3: Packaging

- Add install rules.
- Export CMake package targets.
- Add a package consumption test.
- Document dependency installation per platform.

### Milestone 4: Experimental Platforms

- Add one platform at a time with a thin vertical slice.
- Start with public headers, software-only build, and one simple example.
- Promote a platform to Tier 1 only after CI and documentation exist.

## Definition Of Done

HonkordGL can be considered portable when:

- The support matrix is explicit and accurate.
- Every Tier 1 platform builds in CI.
- Public headers compile cleanly without vendor SDK leakage.
- Optional backends can be disabled independently.
- Unsupported features fail predictably with documented result codes.
- Static and shared builds work.
- Installed packages can be consumed by external projects.
- Documentation matches the actual build behavior.
