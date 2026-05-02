### Creating A Simple Window

This lesson shows how to open a window, process events, and present frames with HonkordGL.
It is organized by feature category so you can learn one building block at a time.

## 1) Core Window Creation Functions

These are the key pieces used in most windowed apps:

- `WindowBackend::Initialize(...)` - starts the backend/display session.
- `WindowBackend::CreateWindow(...)` - creates the native window from settings.
- `WindowBackend::ProcessEvents(...)` - pumps input/window events.
- `WindowBackend::PresentSoftwareFramebuffer(...)` - displays a CPU RGBA framebuffer.
- `WindowBackend::DestroyWindow(...)` and `WindowBackend::Shutdown(...)` - cleanup.

### Minimal settings snippet

```cpp
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/WindowApplication.h>

HonkordGL::ApplicationContextSettings ctx{};
ctx.client_rect.x = 100;
ctx.client_rect.y = 80;
ctx.client_rect.width = 960;
ctx.client_rect.height = 540;
ctx.title = "HonkordGL - Simple Window";
```

## 2) Making a Window Example

This example demonstrates the full lesson flow:
- initialize backend
- create window
- run loop
- process events
- present a color buffer
- shutdown cleanly

```cpp
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/WindowApplication.h>
#include <vector>
#include <cstdint>

int main()
{
    HonkordGL::WindowBackend backend;
    if (!backend.Initialize("HonkordGL Tutorial")) {
        return 1;
    }

    HonkordGL::ApplicationContextSettings ctx{};
    ctx.client_rect.x = 120;
    ctx.client_rect.y = 90;
    ctx.client_rect.width = 960;
    ctx.client_rect.height = 540;
    ctx.title = "Creating A Simple Window";

    if (!backend.CreateWindow(ctx)) {
        backend.Shutdown();
        return 1;
    }

    const int w = ctx.client_rect.width;
    const int h = ctx.client_rect.height;
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 4), 0);

    bool running = true;
    backend.SetTargetFrameRate(60);

    while (running) {
        backend.ProcessEvents(ctx);

        // Demo visual: solid dark-blue background.
        for (int i = 0; i < w * h; ++i) {
            const std::size_t p = static_cast<std::size_t>(i * 4);
            frame[p + 0] = 30;   // R
            frame[p + 1] = 40;   // G
            frame[p + 2] = 80;   // B
            frame[p + 3] = 255;  // A
        }
8
        backend.PresentSoftwareFramebuffer(ctx, frame.data(), w, h, w * 4);
        backend.DelayFrame();

        // Replace with your own close condition / event handling.
        // running = !shouldClose;
    }

    backend.DestroyWindow(ctx);
    backend.Shutdown();
    return 0;
}
```

## 3) Feature Demonstration Categories

### A) Event Pumping

Use `ProcessEvents(ctx)` every frame so close/resize/input callbacks can run.

```cpp
while (running) {
    backend.ProcessEvents(ctx);
    // update
    // render
}
```

### B) Frame Pacing

Use target FPS and backend delay to reduce CPU overrun.

```cpp
backend.SetTargetFrameRate(60);
while (running) {
    // ...
    backend.DelayFrame();
}
```

### C) Software Presentation

Render into an RGBA8 buffer and present it.

```cpp
backend.PresentSoftwareFramebuffer(ctx, pixels, width, height, width * 4);
```

### D) Runtime Window Updates

Update title/size without destroying the backend session.

```cpp
backend.SetWindowTitle(ctx, "Now Playing");
backend.ApplyWindowSettings(ctx); // applies changed geometry/title fields
```

### E) Cursor Controls

Switch cursor kind or provide custom pixel cursor where supported.

```cpp
backend.SetCursorKind(ctx, HonkordGL::CursorKind::Arrow);
// backend.SetCursorFromPixels(ctx, rgbaPixels, cw, ch, hotX, hotY);
```

## 4) Best Window Management

- Initialize backend once, and shut it down once.
- Process events every frame before simulation/render.
- Keep framebuffer stride correct (`width * 4` for RGBA8).
- Pair `CreateWindow` with `DestroyWindow`.
- If you use multiple windows, prefer a shared `WindowBackend`.

### Multi-window on shared backend (concept)

```cpp
HonkordGL::WindowBackend shared;
shared.Initialize("App");

// Create two contexts/windows on the same shared backend session.
shared.CreateWindow(ctxMain);
shared.CreateWindow(ctxTools);
```

That is the complete simple-window lesson path: create, run, present, and clean up.