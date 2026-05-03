### Changelog

- **2026-04-21** — Documented **Wayland** cursor theme + custom **`wl_shm`** cursors, **macOS** per-window cursor rects, and **`CreateWindowOnSharedBackend`** (`Video.h`) for multi-window setups.
- **2026-02-21 (1.0.0)** — Initial window-handling overview.

# Window handling

HonkordGL splits **window / display plumbing** from **rendering** (see [Rendering.md](./Rendering.md)). The window layer is built around **`ApplicationContextSettings`** (per-window state and handles), **`WindowBackend`** (portable driver façade), and optionally **`Window`** (`Video.h`) as a convenience wrapper.

---

## Core types

| Piece | Role |
|--------|------|
| **`ApplicationContextSettings`** (`WindowApplication.h`) | Single struct for **placement** (`client_rect`), **title/icon paths**, **DPI**, **framebuffer size**, **visibility**, **native handles** (`window_handle`, `device_context`, `device`, `surface`, `egl_display`), **`active_renderer`**, resize / present / destroy **callbacks**, and **thread IDs**. Filled and updated by the backend when a window is created or changed. |
| **`WindowBackend`** (`WindowBackend.h`) | **One** display connection per instance, chosen at **`Initialize()`** from built-in drivers (see **`WindowBackendKind`**). Owns **no** long-lived per-window state beyond what you pass in `ApplicationContextSettings`; it **dispatches** to the active platform implementation. |
| **`Window`** (`Video.h`) | Holds **`ApplicationContextSettings`**, an **owned** or **borrowed** **`WindowBackend`** (see multi-window below), optional **`SoftwareRenderer`**, and helpers (**`CreateWindow`**, **`TerminateWindow`**, **`DisplayFrame`**, cursors, frame pacing). **`InitializeWindowSubsystem()`** returns a process-wide default instance for apps that do not want to own a **`Window`** explicitly. |
| **`Events`** (`Events.h`) | **Input and window events** (keys, mouse, resize, etc.) — polled / dispatched in the app loop; not duplicated here in detail. |

---

## `WindowBackend` API (portable surface)

Typical lifecycle:

1. **`Initialize(display_or_app_name)`** — Tries drivers in **platform order** until one connects (e.g. Linux desktop: X11 → Wayland → optional DRM; macOS: Cocoa; Windows: Win32; Android: native glue). **`GetVideoDriverCount()`** reflects how many drivers this build can try.
2. **`CreateWindow(ctx)`** — Creates the native window; fills **`ctx.window_handle`**, sets **`ctx.native_platform`**, attaches display bits where needed, and may start **monitor hotplug** polling where implemented (RandR, Wayland outputs, etc.).
3. **`ProcessEvents(ctx)`** — Pumps the platform event loop for that window context.
4. **`DestroyWindow` / `Shutdown`** — Tear down window and display connection.

Other notable entry points:

| Category | Functions |
|----------|-----------|
| **Session / display** | **`OpenDisplay`**, **`CloseDisplay`**, **`GetDisplay`**, **`GetDefaultScreen`**, **`AttachDisplayToContext`**, platform **`BorrowFromSession`** / **`BorrowFromAndroid`** where available. |
| **Window updates** | **`SetWindowTitle`**, **`ApplyWindowSettings`** (resize / title without full recreate). |
| **CPU presentation** | **`PresentSoftwareFramebuffer`** — blit **RGBA8** pixels to the window (stride in bytes). Used by **`SoftwareRenderer`** and **`Window::DisplayFrame`**. |
| **Frame pacing** | **`SetTargetFrameRate`**, **`DelayFrame`**, **`GetTicks`**, **`SetTicks`**. |
| **Monitors** | **`GetMonitorCount`**, **`GetMonitorBounds`**, **`EnableRandrMonitorPolling`**, **`PollMonitorsChanged`** (capabilities vary by backend). |
| **Cursors** | **`SetCursorKind`**, **`SetCursorFromPixels`**, **`SetCursorFromImageFile`**, **`ResetCursor`**. **X11**, **Win32**, **Cocoa**, and **Wayland** implement built-in kinds and custom bitmaps where the OS/compositor allows. **Wayland** uses **`wl_cursor`** (theme from **`XCURSOR_THEME`** / size from **`XCURSOR_SIZE`**) plus **`wl_shm`** for custom ARGB cursors; the active cursor is reapplied on **`pointer_enter`** and when the pointer serial updates (enter / button) so it stays valid. Link the Wayland backend with **`-lwayland-cursor`**. **DRM** and **Android** have little or no pointer cursor API in this model — calls are effectively no-ops or return **false** for custom bitmaps. See **`WindowBackend.h`** for exact **`SetCursorFromPixels`** contract (invalid parameters → **false**). |
| **IPC helper** | **`CreateIpcHelper`** — binary layout shared with **`IpcHelperWindow`** for cross-process helper windows (X11 / Wayland / Cocoa-oriented). |
| **X11-only queries** | **`HasXkbExtension`**, **`HasXRandrExtension`**, **`HasXInput2Extension`**, version/opcodes, **`RandrEventBase`** — meaningful when the active kind is X11 (or an X11-derived BSD driver). |

---

## `Window` wrapper (`Video.h`)

Adds a **higher-level** shape on top of the backend:

- **Multi-window (one display)** — Call **`Initialize()`** / **`OpenDisplay()`** once on a **`WindowBackend`**, then for each top-level surface either:
  - **`Window(WindowBackend& shared_backend, ApplicationContextSettings& settings)`**, or  
  - **`CreateWindowOnSharedBackend(shared_backend, settings)`** — returns **`std::unique_ptr<Window>`** with the same shared-backend semantics.  
  Each **`Window`** calls **`CreateWindow()`** with its own **`ApplicationContextSettings`**; **`TerminateWindow()`** destroys **that** native window only and does **not** **`Shutdown()`** the shared backend. Default **`Window`** constructors own a private **`WindowBackend`** (one display connection per **`Window`** if you do not use the shared form).
- **`CreateWindow`** — Ensures backend **`Initialize`** when this **`Window`** owns the backend, then **`CreateWindow`** on stored settings.
- **`ResetApplication`** — Merges new settings while **preserving** live native handles when a window already exists.
- **`TerminateWindow`** — Closes window and tears down so **`Initialize`** can run again (or only the window if the backend is shared).
- **`ClearFrame` / `DisplayFrame`** — Software framebuffer clear + **`PresentSoftwareFramebuffer`** via internal **`SoftwareRenderer`**.
- **`SetTargetFPS`**, **`DelayFrame`**, **`DelayFPS`**, **`GetTicks`** — Delegate to the backend’s pacing API.
- **Cursor** — Same family of calls as **`WindowBackend`**, bound to this window’s **`ApplicationContextSettings`** (requires **`CreateWindow`** / initialized backend so **`window_handle`** is set).

**`WindowFlags`** — Enumerates many policy tags (fullscreen, borderless, Vulkan, Metal, …); in practice these are **mostly reserved** for future window-policy APIs and are **not** a complete GLFW/SDL-style feature matrix today.

---

## Diagnostics

- **`SetInternalApiError`**, **`GetInternalApiError`**, **`ClearInternalApiError`**, **`ErrorCallback`** — Thread-local last error string from internal window/graphics code (useful when a backend call fails).

---

## What is in good shape today

- **Multi-platform drivers** — X11, Wayland, Win32, Cocoa, Android, optional **DRM/KMS** on Linux when built with **`HONKORDGL_ENABLE_DRM`**; NetBSD / FreeBSD / Raspberry Pi variants as described in **`WindowBackend.h`**.
- **Stable center of truth** — **`ApplicationContextSettings`** carries everything the rest of the library (rendering, input, software present) expects.
- **Software framebuffer present path** — **`PresentSoftwareFramebuffer`** + **`Window` / `SoftwareRenderer`** integration for CPU-drawn frames.
- **Runtime window updates** — Title and geometry **`ApplyWindowSettings`** without always destroying the native window.
- **Monitors (where wired)** — Hotplug / polling hooks on major desktops; **`GetMonitorBounds`** for layout.
- **Frame pacing** — Optional FPS cap via **`DelayFrame`** and tick counter.
- **Cursors on major desktops** — Built-in **`CursorKind`** mapping and custom pixels on **X11**, **Wayland** (theme + SHM), **Win32**, and **Cocoa** (including **cursor rects** on the content view so multiple **`NSWindow`** instances each keep a consistent cursor).

---

## Gaps and limitations

| Topic | Detail |
|-------|--------|
| **`WindowFlags`** | Largely **placeholders**; not a full declarative window-style system (decorations, always-on-top, etc.) wired across all backends yet. |
| **Cursor (DRM / Android)** | No full desktop-style pointer in typical DRM or touch-only Android flows; **`SetCursorKind`** / **`SetCursorFromPixels`** are not comparable to X11/Wayland/Win32/Cocoa there. |
| **macOS resize** | Cursor rects are installed when you call **`SetCursor*`**; after **`ApplyWindowSettings`** resizes the view, call **`SetCursorKind`** (or **`ResetCursor`**) again if you need rects recomputed for the new bounds. |
| **Backend parity** | Not every **`WindowBackend`** method is equally capable on every OS (e.g. X11-specific queries are **irrelevant** on Wayland-only sessions). You cannot run every native backend on every OS in one process; the **API** is shared, implementations are **per platform**. |
| **DRM / kiosk** | Optional build; different usage model (no traditional desktop window) — treat as a **separate** deployment path. |
| **Examples** | Samples often show one main window; use **`Window(WindowBackend&, …)`** or **`CreateWindowOnSharedBackend`** plus one initialized backend for several top-level windows on the same display. |

---
