# OpenGL family backend

## Scope

Desktop OpenGL, OpenGL ES when `HONKORDGL_GPU_RENDERER_OPENGL_ES`, WGL, GLX, EGL, NSOpenGL — vendor types stay in `src/` TUs.

## Requirements

Platform GL/EGL libraries (see `docs/platforms/*.md`).

## Public surface

`GpuRenderer`, `GpuShaderCompiler`, `OpenGlIntegration`, `RendererContext` attach paths.

## Compile definitions

`HONKORDGL_HAVE_OPENGL` in generated `BuildFeatures.h`; runtime `IsOpenGLBackendAvailable()`.
