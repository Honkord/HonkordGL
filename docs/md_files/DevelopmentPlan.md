# Development plan

## Exposing more hardware without leaking internal APIs and macros

Goal: give programmers access to more device capabilities (limits, optional features, queues, etc.) while keeping vendor SDKs, generated protocol code, and build-only macros out of the public include surface.

### Principles

1. **Own the vocabulary** — Public API uses its own enums, structs, and opaque handles (`GpuDeviceId`, texture/buffer handles, queue kinds). Implementations map these to Vulkan, D3D, GL, EGL, etc. only in translation units under `src/` (and related internal headers). Public headers do not include vendor SDK headers or `internal/` implementation details.

2. **Capabilities instead of macros** — Do not expose `#define GL_MAX_*`, platform `#ifdef` ladders, or similar as part of the user contract. Prefer init-time or runtime structures such as `HardwareCaps` / `GpuLimits` (e.g. max texture dimension, alignment requirements, timestamp resolution). Populate them inside the implementation from whatever macros, extensions, or queries are needed.

3. **Optional and graded features** — Separate a stable baseline from opt-in hardware paths: e.g. explicit `RequestDevice(...)`, `EnableFeature("...")`, or named capability toggles that can fail with a stable, documented outcome. Users gain power without compiling against the library’s internal feature matrix.

4. **Stable errors** — Map internal failures (`VkResult`, `HRESULT`, driver quirks) to stable result codes or small enums plus optional human-readable messages. Avoid raw vendor error types in public headers unless they are an intentional, versioned part of the API.

5. **Narrow extension points** — For unpredictable advanced use cases, prefer callbacks or small plugin interfaces that receive only HonkordGL-defined opaque context and types, not raw command lists or device pointers.

6. **Version boundary** — If the implementation must churn, document an explicit API level or keep a thin stable surface (e.g. init with a version constant) so internal refactors do not require users to depend on compile-time macros that only exist inside the library build.

7. **Build hygiene** — User code must compile with only installed public headers and documented defines. Internal macros and generated files (e.g. Wayland protocol headers) stay off the default consumer include path; they are implementation details of the build that produces the library.

8. **Documentation** — Treat capability structs, optional features, and failure modes as part of the API: which fields are best-effort, which are required for a given code path, and behavior when hardware or drivers do not match advertised limits.

### Summary

Widen what users can **request** and **query** (caps, limits, optional features); narrow what they must **see** (Honkord types, opaque handles, stable errors). Keep all vendor symbols, internal macros, and generated glue strictly on the implementation side of that boundary.
