# Changelog

All notable packaging and portability changes are summarized here. Library feature work may also appear in commit history.

## Unreleased

- `HONKORDGL_ENABLE_D3D11=OFF` / `HONKORDGL_ENABLE_METAL=OFF` swap in stub translation units and omit Direct3D / Metal link libraries; `HONKORDGL_ENABLE_SOFTWARE_ONLY=ON` cascades Vulkan/D3D11/Metal off at configure time.
- `HONKORDGL_ENABLE_OPENGL=OFF` fails at CMake configure with an explicit message (OpenGL-family sources still required).
- CI: Windows and macOS jobs run public header check plus install + `tests/cmake_consumer` smoke test; Linux matrix adds a software-only profile.

- Feature-based CMake options (`HONKORDGL_ENABLE_*`) for Linux windowing, Vulkan, and audio.
- Generated `BuildFeatures.h` and runtime backend queries (`BackendCapabilities.h`).
- Fixed `HONKORDGL_API` export macros for MSVC and GCC visibility.
- CMake install rules: exported target `HonkordGL::HonkordGL`, `HonkordGLConfig.cmake`, pkg-config `honkordgl.pc`.
- Documentation: support matrix, platform/backend contracts, known limitations.
- CI workflow for Linux matrix builds, header check, install + consumer smoke test.
- Portability tests: backend capability consistency, umbrella header compile.

Release checklist (before tagging Tier 1):

1. Clean clone; configure with default options on Linux, Windows, macOS.
2. `cmake --install` into a prefix; consume via `find_package(HonkordGL CONFIG REQUIRED)`.
3. Run CI on default branch.
4. Confirm `scripts/check_public_headers.py` passes.
