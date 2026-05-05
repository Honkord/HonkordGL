# Known limitations

- **Software-only profile**: `HONKORDGL_ENABLE_SOFTWARE_ONLY=ON` forces Vulkan, Direct3D 11, and Metal **off** at configure time. **OpenGL/EGL/WGL/NSOpenGL may still link** for desktop window systems and `GpuRenderer` / GLSL helpers until DevelopmentPlan §2–3 splits those paths.
- **OpenGL-off**: `HONKORDGL_ENABLE_OPENGL=OFF` is rejected at configure time (GpuRenderer and shader compilation units still require OpenGL headers today). **D3D11-off** and **Metal-off** use dedicated stub sources and omit Direct3D / Metal link libraries when disabled.
- **DRM/KMS**: Full KMS-only window stack is optional; `EventsDRM.cpp` is always linked for poll routing; full DRM window drivers are not exposed as a separate Tier-1 target yet.
- **Experimental OS** targets (Android, Emscripten, …): macros exist; CMake does not yet ship production presets for all of them.

See `CHANGELOG.md` for release-facing notes.
