### Text UI in HonkordGL (Complete API Guide)

This guide walks through the full TextUI API in `HonkordGL/TextUI.h`, including GPU and software paths, per-draw styling, draggable text, event processing, and shutdown.

---

## 1) What TextUI Provides

TextUI is an immediate-style text layer with:

- batched draw calls (`TextUIBegin` / `TextUIEnd`)
- per-draw style state (color, size, rect, movable/fixed)
- two backends:
  - GPU window display path (`TextUIBindDisplay`)
  - CPU software framebuffer path (`TextUIBindSoftware2D`)
- optional draggable text blocks (`TextUINextTextMovable` + `TextUIProcessEvent`)

---

## 2) Full API Surface

From `TextUI.h`:

- `TextUIBindDisplay(GpuRenderer&, ApplicationContextSettings&)`
- `TextUIBindSoftware2D(ApplicationContextSettings&, Software2DContext&)`
- `TextUIBegin()`
- `TextUIEnd()`
- `TextUINextColor(r,g,b,a)`
- `TextUITextSize(heightPixels)`
- `TextUITextRect(x,y,width,height)`
- `TextUINextTextMovable()`
- `TextUINextTextFixed()`
- `TextUIDraw(const char* utf8Text)`
- `TextUIProcessEvent(ApplicationContextSettings&, const Event&)`
- `TextUIShutdown()`

---

## 3) Binding: Choose One Path

TextUI binding is mutually exclusive: bind either display (GPU) or software (CPU).

### A) GPU binding (`TextUIBindDisplay`)

Use when rendering with `GpuRenderer`.

```cpp
HonkordGL::Graphics::GpuRenderer gpu(ctx);
gpu.CreateBestAvailableShaderBackend();
gpu.MakeCurrent();

if (HonkordGL::Graphics::TextUIBindDisplay(gpu, ctx) != 0) {
    // handle bind failure
}
```

### B) Software binding (`TextUIBindSoftware2D`)

Use when drawing into `Software2DContext` and presenting via `PresentSoftwareFramebuffer`.

```cpp
HonkordGL::Graphics::Software2DContext frame;
frame.Resize(fbw, fbh);

if (HonkordGL::Graphics::TextUIBindSoftware2D(ctx, frame) != 0) {
    // handle bind failure
}
```

Important: keep `ctx.frame_buffer_width` and `ctx.frame_buffer_height` aligned with your software surface dimensions.

---

## 4) Draw Lifecycle: Begin -> Configure -> Draw -> End

Only call styling/draw functions between `TextUIBegin()` and `TextUIEnd()`.

```cpp
HonkordGL::Graphics::TextUIBegin();
HonkordGL::Graphics::TextUINextColor(1.f, 1.f, 1.f, 1.f);
HonkordGL::Graphics::TextUITextSize(16.f);
HonkordGL::Graphics::TextUITextRect(12.f, 10.f, 420.f, 40.f);
HonkordGL::Graphics::TextUIDraw("Score: 1250");
HonkordGL::Graphics::TextUIEnd();
```

If called outside Begin/End, style setters and draw calls are ignored.

---

## 5) Per-Draw Style Functions

Each of these affects the **next** `TextUIDraw(...)` call.

### A) `TextUINextColor(r,g,b,a)`

- RGBA in 0..1 range
- affects next draw only
- resets to white after draw internally

```cpp
TextUINextColor(1.f, 0.85f, 0.25f, 1.f);
TextUIDraw("Warning");
```

### B) `TextUITextSize(heightPixels)`

- sets text size (pixel height)
- must be > 0

```cpp
TextUITextSize(22.f);
TextUIDraw("Big headline");
```

### C) `TextUITextRect(x,y,width,height)`

- layout box in client pixel space (origin top-left, y-down)
- used for wrapping/clipping region behavior

```cpp
TextUITextRect(20.f, 72.f, 300.f, 120.f);
TextUIDraw("Long text that should wrap within the configured rectangle.");
```

If you never set a rect, default is whole framebuffer/window.

---

## 6) Movable vs Fixed Text

TextUI supports dragging text regions with the left mouse button.

### A) Movable text

```cpp
TextUINextTextMovable();
TextUITextRect(40.f, 40.f, 220.f, 36.f);
TextUIDraw("Drag me with mouse");
```

### B) Fixed text

```cpp
TextUINextTextFixed(); // default behavior
TextUITextRect(8.f, 8.f, 400.f, 32.f);
TextUIDraw("Pinned HUD line");
```

Drag state persists across frames as long as the same bound app context is used and events are processed.

---

## 7) Event Processing for Draggable Text

Call `TextUIProcessEvent(...)` from your event loop, typically for every event polled.

```cpp
HonkordGL::Events::Event ev{};
while (HonkordGL::Events::PollEvent(ctx, ev)) {
    if (ev.type == HonkordGL::Events::EventType::QUIT)
        running = false;

    HonkordGL::Graphics::TextUIProcessEvent(ctx, ev);
}
```

Without this, `TextUINextTextMovable()` text will not drag.

---

## 8) Complete Software Path Example

```cpp
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/TextUI.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <cstdio>

using namespace HonkordGL::Graphics;

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "TextUI - Software";
    settings.client_rect.x = 100;
    settings.client_rect.y = 90;
    settings.client_rect.w = 960;
    settings.client_rect.z = 600;

    if (!backend.Initialize(nullptr))
        return 1;

    auto window = CreateWindowOnSharedBackend(backend, settings);
    if (!window) {
        backend.Shutdown();
        return 1;
    }
    window->CreateWindow();
    ApplicationContextSettings& ctx = window->RetrieveCurrentSettings();
    if (!ctx.window_handle) {
        backend.Shutdown();
        return 1;
    }

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    Software2DContext frame{};
    if (!frame.Resize(fbw, fbh)) {
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    if (TextUIBindSoftware2D(ctx, frame) != 0) {
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    backend.SetTargetFrameRate(60);
    bool running = true;
    char fpsLine[80]{};

    while (running) {
        HonkordGL::Events::Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            if (ev.type == HonkordGL::Events::EventType::QUIT)
                running = false;
            if (ev.type == HonkordGL::Events::EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                fbw = ev.width;
                fbh = ev.height;
                (void)frame.Resize(fbw, fbh);
            }
            TextUIProcessEvent(ctx, ev);
        }

        frame.Clear(14, 18, 26, 255);
        frame.FillRect(0, 0, fbw, 44, 28, 34, 48, 255);
        std::snprintf(fpsLine, sizeof fpsLine, "ticks: %llu", static_cast<unsigned long long>(backend.GetTicks()));

        TextUIBegin();
        TextUINextColor(1.f, 1.f, 1.f, 1.f);
        TextUITextSize(15.f);
        TextUITextRect(10.f, 8.f, static_cast<float>(fbw - 20), 30.f);
        TextUINextTextFixed();
        TextUIDraw(fpsLine);

        TextUINextColor(0.95f, 0.8f, 0.3f, 1.f);
        TextUITextSize(18.f);
        TextUITextRect(40.f, 90.f, 260.f, 40.f);
        TextUINextTextMovable();
        TextUIDraw("Drag this TextUI label");
        TextUIEnd();

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    TextUIShutdown();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}
```

---

## 9) Complete GPU Path Snippet

```cpp
GpuRenderer gpu(ctx);
if (gpu.CreateBestAvailableShaderBackend() != 0) return 1;
gpu.MakeCurrent();
gpu.SetDefaultViewport();
if (TextUIBindDisplay(gpu, ctx) != 0) return 1;

// each frame:
gpu.MakeCurrent();
gpu.SetDefaultViewport();
gpu.ClearColorBuffer(0.05f, 0.06f, 0.1f, 1.f);

TextUIBegin();
TextUINextColor(0.7f, 1.f, 0.8f, 1.f);
TextUITextSize(18.f);
TextUITextRect(20.f, 20.f, 360.f, 80.f);
TextUIDraw("GPU TextUI rendering");
TextUIEnd();

gpu.SwapBuffers();
```

---

## 10) Behavior Notes

- `TextUIDraw` supports ASCII / Latin-1 subset as documented.
- Newline characters are supported in draw text (`\n`).
- Draws are grouped by Begin/End; `TextUIEnd` persists draggable slot metadata for hit-testing.
- There is an internal max slot count for per-frame text entries; keep HUD text reasonably bounded.

---

## 11) Shutdown and Rebind

Call `TextUIShutdown()` once when your app is closing or before full renderer teardown.

```cpp
TextUIShutdown();
```

If you switch from GPU to software (or vice versa), call the appropriate bind function again; bindings are exclusive.

---

## 12) Practical Checklist

- Bind exactly one path (`TextUIBindDisplay` or `TextUIBindSoftware2D`)
- Process events each frame and forward to `TextUIProcessEvent`
- Use Begin/End around all TextUI draw commands
- Set size/rect/color before each text entry you want customized
- Call `TextUIShutdown` on exit

This is the complete TextUI API workflow in HonkordGL.
