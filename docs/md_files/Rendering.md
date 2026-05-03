### Changelog: 2026-02-21 (1.0.0)
# Rendering

HonkordGL separates **windowing** (`WindowBackend`, `ApplicationContextSettings`) from **rendering**. Two main paths exist:

| API | Purpose |
|-----|---------|
| **`GpuRenderer`** | Attach a native graphics API and draw with shaders, buffers, and textures. |
| **`SoftwareRenderer`** | Draw into a CPU **`Software2DContext`**, then present via the window (CPU blit or optional GPU texture upload + fullscreen quad). |

Headers: `HonkordGL/GpuRenderer.h`, `HonkordGL/SoftwareRenderer.h`, `HonkordGL/WindowApplication.h`.

---

# Shared context

- **`ApplicationContextSettings`** — Client size, framebuffer size, `window_handle`, and fields used by backends (`device`, display/surface handles, etc.). Filled by **`WindowBackend::CreateWindow`** (and related calls).
- **`WindowBackend`** — Platform window creation, events, and **CPU presentation** via **`PresentSoftwareFramebuffer`** when no GPU path is used.

Everything that renders assumes a valid window binding unless you are doing off-screen work (not covered here).

---

# `GpuRenderer`

High-level wrapper around the platform’s graphics stack (OpenGL / EGL / GLES, Vulkan, Metal where implemented, etc.). It does **not** own the window; **`app` must outlive** the `GpuRenderer`.

## Typical flow

1. **`Bind(ApplicationContextSettings& app)`** — Target window/context.
2. **`Create(...)`** or a **`Create*`** helper — Attach a backend (see below).
3. **`MakeCurrent()`** — Before GL-family calls the implementation issues.
4. Draw using **`CompileShaderProgram`**, **`CreateBuffer`**, **`CreateTexture2DRgba8`**, **`DrawArrays`**, etc.
5. **`SwapBuffers()`** — Present the frame.
6. **`Destroy()`** when tearing down this renderer’s attachment (idempotent).

## Choosing a backend

- **`Create(RendererContextSettings)`** — Explicit backend (`Renderers::OPENGL`, `EGL`, `VULKAN`, …).
- **`CreateAutomatic()`** — Sensible defaults (e.g. GLX vs EGL on X11, order may follow `HONKORDGL_USE_EGL`).
- **`CreateBestAvailable()`** — Probe until one backend attaches.
- **`CreateBestAvailableShaderBackend()`** — Prefer OpenGL / EGL paths that can run **user GLSL** from this API (skips APIs that do not expose that in this build).
- **`CreateVulkan()`** / **`CreateMetal()`** — Specific APIs where supported (e.g. Linux Vulkan, macOS Metal).
- **`CreateBestAvailableExhaustive()`** — Cross-platform order (e.g. Vulkan → GL → EGL on Linux; Metal → GL on macOS; D3D stub → WGL on Windows). The winner may **not** support **`CreateTexture2DRgba8`** (swapchain-only paths).
- **`CreateBestAvailableForCpuFramebufferPresent()`** — OpenGL / GLES / EGL family suitable for **RGBA8 texture upload** and blit; skips Vulkan/Metal when probing.

Use **`SupportsTextureRgbUpload()`** after attach if you need **`CreateTexture2DRgba8`** / **`UpdateTexture2DRgba8`**.

## Other notes

- **`ActiveBackend()`** / **`ActiveBackendLabel()`** — Which API won.
- **`LoadShaderCompilerProcs()`** / **`GetGraphicsProc()`** — Dynamic entry points where required (e.g. WGL).
- Adapter strings: **`GetAdapterRendererDescription`**, **`GetAdapterString`**, etc., when implemented for the active backend.

---

# `SoftwareRenderer`

Owns an internal **`Software2DContext`** (RGBA8) sized from the bound **`ApplicationContextSettings`**. You draw into **`Surface()`** in software (rects, text, etc.).

## Typical flow

1. **`Bind(ApplicationContextSettings& app, WindowBackend& backend)`** after the window exists.
2. **`EnsureSurface()`** — Allocates/resizes the CPU buffer to match the window (framebuffer or client size).
3. Draw into **`Surface()`**.
4. **`Present()`** — Either:
   - **CPU:** **`WindowBackend::PresentSoftwareFramebuffer`**, or  
   - **GPU (optional):** upload pixels to a GPU texture, draw a fullscreen quad, **`SwapBuffers`** (see below).

## Optional hardware-accelerated present

- **`InitHardwareAcceleratedPresent()`** — After **`window_handle`** is set, tries to attach a GPU context and compile a simple blit shader. Uses **`GpuRenderer::CreateBestAvailableExhaustive()`**; if the attached backend cannot upload textures, it **detaches** and falls back to **`CreateBestAvailableForCpuFramebufferPresent()`**. Skipped on **DRM**; returns `false` if everything fails (CPU **`Present()`** still works).
- **`UsesHardwareAcceleratedPresent()`** — Whether the GPU path is active.
- **`PresentBackendLabel()`** — e.g. `"OPENGL"`, `"EGL"`, or **`"CPU"`** when not using the GPU blit.

`InitHardwareAcceleratedPresent` is **optional** and not called automatically from **`Window::CreateWindow`**; call it when you want GPU present without wiring **`GpuRenderer`** yourself.

## Relationship to `GpuRenderer`

Both can use the same **`ApplicationContextSettings`**. Only one presenter should “own” the attached GPU context at a time: avoid calling **`InitHardwareAcceleratedPresent`** while another **`GpuRenderer`** is already attached to the same window unless you coordinate **`Destroy`** / **`Bind`** explicitly.

---

# Feature coverage

This section summarizes what the rendering stack **provides today** versus what is **missing, partial, or out of scope** for the portable `GpuRenderer` / `SoftwareRenderer` APIs. (The project README also notes early-stage quality and performance expectations.)

## What’s implemented

| Area | Notes |
|------|--------|
| **Window + attach** | Native windows via **`WindowBackend`**; **`AttachRendererContext`** / **`GpuRenderer::Create*`** attach a backend into **`ApplicationContextSettings`**. Linux (X11 / Wayland), Windows (WGL), macOS (OpenGL + Metal path), Android (GLES/EGL-style), DRM considerations where coded. |
| **OpenGL family (primary path)** | Desktop OpenGL (e.g. GLX, WGL, NSOpenGL), **EGL** where applicable, **OpenGL ES** on embedded/Android. Core-profile defaults in **`RendererContextSettings`**. |
| **`GpuRenderer` resource API** | **GLSL** compile/link (**`CompileShaderProgram`**), **buffers**, **VAOs**, **vertex attributes**, **RGBA8 textures** (**`CreateTexture2DRgba8`** / **`UpdateTexture2DRgba8`**), **draw** (**`DrawArrays`** / **`DrawIndexed`**), viewport, color clear, depth test toggle, swap buffers. Dynamic **`GetGraphicsProc`** on loaders that need it (e.g. WGL). |
| **Backend selection** | **`CreateAutomatic`**, **`CreateBestAvailable`**, **`CreateBestAvailableShaderBackend`**, **`CreateBestAvailableExhaustive`**, **`CreateBestAvailableForCpuFramebufferPresent`**, plus explicit **`CreateVulkan`** / **`CreateMetal`** where the platform build supports them. |
| **Vulkan / Metal (attach)** | **Linux desktop**: Vulkan can be selected (swapchain-oriented attach). **macOS**: Metal can be selected. These are useful for **presentation / probing** and samples that target those APIs internally—not for the full portable **`GpuRenderer`** shader/buffer/texture surface above without OpenGL. |
| **CPU 2D** | **`Software2DContext`** (RGBA8), **`SoftwareBlitCollector`** (and alt), **`SoftwareRenderer`** with **`PresentSoftwareFramebuffer`**. |
| **Optional GPU present for CPU frames** | **`SoftwareRenderer::InitHardwareAcceleratedPresent`** uploads the CPU buffer after **`CreateBestAvailableExhaustive`** (Vulkan/Metal may fall back to GL or D3D 11 on Windows when textures are unsupported). Blit uses **OpenGL/GLES** or **D3D11** (HLSL fullscreen pass) depending on the attached backend. |
| **Higher-level 2D helpers** | **`Sprite`**, **`Lines`**, **`Eclipse`** (2D geometry with transforms). |
| **Text** | **`TextUI`** — GPU-oriented path when a GLSL-capable context is active; CPU/software binding where configured. |
| **Samples** | e.g. **`DeferredRendererSample`** — deferred-style sample built on the internal GPU API (not a second portable renderer front-end). |

## Gaps and limitations

| Topic | Detail |
|-------|--------|
| **Direct3D 11** | **Windows**: **`AttachRendererContext`** with **`Renderers::DIRECT3D`** creates a DXGI swap chain, device, and render-target view. **`GpuRenderer::CreateBestAvailableExhaustive`** tries D3D before WGL. |
| **WebGPU / WebGL** | Enumerated in **`Renderers`** and string labels; **no** matching attach path here for a browser or WASM target in this C++ library layout. |
| **“Exhaustive” vs texture upload** | **`CreateBestAvailableExhaustive`** may land on **Vulkan** or **Metal** first. **`SupportsTextureRgbUpload()`** will be false until you attach **OpenGL family**; apps that need CPU→GPU texture upload should use **`CreateBestAvailableForCpuFramebufferPresent`** or the **`SoftwareRenderer`** fallback behavior. |
| **SoftwareRenderer GPU path** | Present is **RGBA upload + fullscreen blit** (OpenGL shaders or D3D11 HLSL on Windows), not a full custom engine renderer. |
| **OpenGL ES 2** | Some **geometry / VAO** features may be unavailable or return **`GpuShaderCompileError::UNSUPPORTED_BACKEND`** on minimal ES2 paths (see **`GpuTypes.h`** comments). |
| **Concurrent attach** | **`GpuRenderer::Create`** and **`AttachRendererContext`** (Win32) **replace** an existing attach when present, so a new **`Create`** after another owner’s attach detaches the previous raster context automatically. |
| **Project maturity** | Overall performance, polish, and backend completeness are still evolving; treat edge cases and platform matrix as “best effort” unless covered by your own tests. |

---

## See also
