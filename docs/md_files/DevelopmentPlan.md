# Development plan

This document records the intended direction for HonkordGL’s **public surface**, **hardware exposure**, and **implementation boundary**. It consolidates the library’s stated goals with a concrete plan to stay **low-level** without shipping **internal** APIs or leaking **vendor implementation details**.

---

## 1. Current state (baseline)

### Does it achieve all its requirements?

**No.** The project positions itself as a low-level multimedia library, but the **shipping API is layered**:

- **Mid-level GPU API:** `GpuRenderer` wraps context lifecycle, buffers, VAOs, shader compile/link, draws, textures, viewports, limits, and optional features behind stable types. Public headers intentionally avoid Khronos/vendor SDK types in the main boundary.
- **Higher-level / convenience:** Sprites, software raster paths, deferred samples, camera, lines, UI helpers, audio, and windowing are not “driver-level only.”
- **Escape hatches for custom rendering:** Native handles and procedure resolution are exposed on purpose (`OpenGlIntegration`, `GpuRenderer::GetGraphicsProc`, `RendererContextGetGraphicsProc`) so applications can integrate loaders or interop with native APIs.

So the library is **low-level in spirit** (the application manages GPU state and rendering), but it is **not** a thin pass-through of raw OpenGL / Vulkan / D3D / Metal everywhere.

### What's in good shape today

**Mostly for the OpenGL family**, with **explicit backend limits** documented in code and headers:

- **Capabilities:** `GpuLimits` and `QueryHardwareLimits` are best-effort from `glGet*` (and equivalents where applicable). Applications should treat limits as runtime truth rather than compile-time `GL_MAX_*` macros in user code.
- **Integration:** `OpenGlNativeRenderingHandles` documents GLX / WGL / EGL / NSOpenGL slots for binding third-party stacks.
- **Non-GL backends:** Vulkan / Metal / D3D paths may not support the same helpers as the GL path (for example, texture upload helpers are oriented toward OpenGL-family backends; exhaustive probing may attach a backend without full `GpuRenderer` texture support).
- **Adapter selection:** `RendererContextSettings::requested_gpu_device` is reserved; non-default selection is not implemented in the current build — so “pick this GPU” is not part of the stable story yet.

**Bottom line:** Hardware access is **strongest** where users combine **opaque native handles** + **`GetGraphicsProc`** + **limits / optional features**. Full parity across every backend through Honkord’s C++ helpers alone is **not** the current guarantee.

---

## 2. Our Goal:

Target outcome:

- Applications get **enough** control for real engines (context/surface, native handles, procedure address, limits/caps, present).
- **No** installed headers from `src/internal/`, generated protocol glue, or vendor SDKs on the default consumer include path.
- **No** requirement for users to compile against internal macros or unstable struct layouts.

This is achieved by separating **two contracts**:

1. **Small stable public surface** — what is installed, documented, and versioned (`include/HonkordGL/`, ABI-stable where promised).
2. **Everything else** — names under `src/`, internal helpers, third-party vendored code — **implementation only**, not part of the consumer contract.

---

## 3. Split “core” from optional layers

Today `HonkordGL.h` aggregates core rendering with sprites, software raster, and other helpers. For a **low-level-first** distribution:

| Layer | Contents (conceptual) | Shipping shape |
|--------|------------------------|----------------|
| **Core** | Platform bootstrap, window/surface, attach context, present, `GetGraphicsProc`, limits/caps, opaque native handle bundles | Default install; single small umbrella header optional |
| **Optional** | Sprites, deferred samples, software renderer, mid-level draw helpers beyond strict minimum | Separate headers and/or link units so “honest low-level” apps never pull them in |

**Implemented layout:**

- **`include/HonkordGL/Core.h`** — Windowing, events, `Video`, renderer attach, `GpuApiBoundary`, limits, `GpuRenderer`, `OpenGlIntegration`, `GpuShaderCompiler` (no sprites/software/IPC game helpers).
- **`include/HonkordGL/Extras.h`** — Includes `Core.h`, then joystick/audio, software raster stack, sprites/lines/eclipse, deferred sample, camera/text UI, platform IPC helpers.
- **`include/HonkordGL/HonkordGL.h`** — Umbrella; includes `Extras.h` (therefore the full classic bundle).

This does **not** require exposing `internal/`; it only requires **not** lumping optional layers into the default mental model or single include if the project markets a strict low-level core.

---

## 4. Boundary rules (public vs implementation)

### Rules already aligned with the codebase

- **Public headers:** POD structs, opaque handles, enums, stable integer/result codes. **`GpuTypes`**, **`OpenGlIntegration`** (no GL headers in the public header), and **`GpuApiBoundary`** illustrate the intended style.
- **Private:** Any translation unit that includes `gl.h`, WSI types, Metal/Vulkan headers, or Wayland-generated protocols stays out of installed includes.

### Hardening

- **Automated check:** `scripts/check_public_headers.py` scans `include/HonkordGL/**/*.h` (excluding `third_party/`) for forbidden include paths (`internal/`, `../`, `/src/`) and vendor SDK angle-brackets (`<GL/…>`, `<vulkan/…>`, etc.). CMake runs it before linking `HonkordGL` when `HONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY=ON` (default) and Python 3 is available; disable with `-DHONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY=OFF` if needed.
- **Documentation:** State explicitly that **only** installed headers + documented defines are supported; internal macros are not API.

---

## 5. Must be cautious

Low-level does **not** mean re-exporting every `gl*` symbol. It means a **documented, stable kit**:

| Need | Stable public shape |
|------|---------------------|
| Drive your own GL / Vulkan / D3D | Current context + **`GetGraphicsProc` / `RendererContextGetGraphicsProc`** + **native handle bundle** (`OpenGlNativeRenderingHandles` for GL family; analogous bundles if other backends are promoted to the same contract). |
| Know device limits | **`GpuLimits`**, optional features, and clear semantics for `0` / unknown / N/A. |
| Binary stability (if promised) | **Opaque context/session types** (PIMPL or C handles) so internal struct layouts can change without breaking users. |

Deeper concerns (allocators, swapchain internals, pool reuse) remain **implementation details** as long as handles, procedure resolution, and caps let callers use vendor APIs **outside** Honkord headers.

---

## 6. Positioning `GpuRenderer`

`GpuRenderer` is currently a **mid-level** helper (textures, VAO, compile helpers, draws). For a strict low-level story:

- **Document** it explicitly as the **optional portable GL-style helper**, not as the entire definition of “HonkordGL.”
- Alternatively **thin** the default “core” to attach / swap / proc / handles / limits only, and move or isolate richer draw helpers behind an optional module.

Either approach keeps “low-level” honest **without** exposing internal implementation headers.

---

## 7. Version the surface, not the implementation

Continue using **`HonkordGlMakeApiLevel` / `minimum_honkord_gl_api_level`** (`GpuApiBoundary.h`, `RendererContextSettings`):

- Expand stable structs by **appending** fields or using **versioned capability/feature structs** with explicit size or kind fields.
- Internal refactors stay in `.cpp`; the **documented** API level is what applications target.

---

## 8. Principles: exposing more hardware without leaking internals

These principles overlap with section 4 and remain in force.

1. **Own the vocabulary** — Public API uses HonkordGL-owned enums, structs, and opaque handles. Implementations map to Vulkan, D3D, GL, EGL, etc. only in `src/` (and related internal headers). Public headers do not include vendor SDK headers or `internal/` implementation details.

2. **Capabilities instead of macros** — Do not expose `#define GL_MAX_*` or platform `#ifdef` ladders as the user contract. Prefer init-time or runtime structures such as `GpuLimits`. Populate them inside the implementation from macros, extensions, or queries as needed.

3. **Optional and graded features** — Separate a stable baseline from opt-in paths: named capability toggles or feature requests with stable, documented failure outcomes.

4. **Stable errors** — Map internal failures (`VkResult`, `HRESULT`, driver quirks) to stable result codes or small enums plus optional UTF-8 diagnostics. Avoid raw vendor error types in public headers unless intentionally versioned.

5. **Narrow extension points** — For advanced cases, prefer callbacks or small plugin interfaces that receive only HonkordGL-defined opaque context and types.

6. **Version boundary** — Document API level; internal refactors must not force dependence on compile-time macros that exist only inside the library build.

7. **Build hygiene** — User code compiles with only installed public headers and documented defines. Generated files (e.g. Wayland protocols) stay implementation-side.

8. **Documentation** — Treat capability structs, optional features, and failure modes as part of the API: best-effort fields, required fields per code path, behavior when hardware or drivers under-deliver.

---

## 9. Summary

- **Widen** what users can **request** and **query** (caps, limits, optional features, native handles, proc addresses).
- **Narrow** what they must **see** (Honkord-owned types, opaque handles, stable errors, optional layered headers).
- **Ship** a small **core** surface for window + context + present + hardware introspection + interop; **isolate** mid-level and high-level helpers so “low-level” is not contradicted by default includes.
- Keep all vendor symbols, internal macros, and generated glue strictly on the **implementation** side of the boundary.
