### Drawing 2D Shapes (Software Renderer)

This tutorial explains how to draw 2D shapes with HonkordGL using the CPU software renderer path.
You will render into `Software2DContext` (RGBA8 pixels) and present that framebuffer with `WindowBackend::PresentSoftwareFramebuffer(...)`.

---

## 1) What You Need

Core headers:

- `HonkordGL/WindowBackend.h`
- `HonkordGL/WindowApplication.h`
- `HonkordGL/Video.h`
- `HonkordGL/Events.h`
- `HonkordGL/Software2DContext.h`

Core objects:

- `WindowBackend` - platform window/session + frame pacing + present
- `ApplicationContextSettings` - window settings and runtime state
- `Software2DContext` - CPU framebuffer with shape helpers (`Clear`, `FillRect`, `FillSpan`, `SetPixel`)

---

## 2) Rendering Model (Software Path)

Each frame:

1. Poll events (`PollEvent`)
2. Update game state
3. Draw shapes into `Software2DContext`
4. Present the RGBA8 buffer using `PresentSoftwareFramebuffer`
5. Delay frame (`DelayFrame`)

This is the same pattern used by software-based examples in `works/`.

---

## 3) Walkthrough: Full Shape Scene

This example draws all requested primitive types:

- rectangles
- lines
- circles
- ellipses

```cpp
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::WindowBackend;

static void DrawCircleFilled(
    Software2DContext& fb, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    if (radius <= 0)
        return;

    const int r2 = radius * radius;
    const int y0 = std::max(0, cy - radius);
    const int y1 = std::min(fb.Height() - 1, cy + radius);
    for (int y = y0; y <= y1; ++y) {
        const int dy = y - cy;
        const int dy2 = dy * dy;
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy2 <= r2)
            ++dx;
        fb.FillSpan(y, cx - dx, cx + dx + 1, r, g, b, 255);
    }
}

static void DrawLine(
    Software2DContext& fb, int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        fb.SetPixel(x0, y0, r, g, b, 255);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void DrawEllipseFilled(
    Software2DContext& fb, int cx, int cy, int rx, int ry, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    if (rx <= 0 || ry <= 0)
        return;

    const int y0 = std::max(0, cy - ry);
    const int y1 = std::min(fb.Height() - 1, cy + ry);
    const float invRx2 = 1.0f / static_cast<float>(rx * rx);
    const float invRy2 = 1.0f / static_cast<float>(ry * ry);

    for (int y = y0; y <= y1; ++y) {
        const int dy = y - cy;
        const float yTerm = static_cast<float>(dy * dy) * invRy2;
        if (yTerm > 1.0f)
            continue;
        const float xNorm = std::sqrt(std::max(0.0f, 1.0f - yTerm));
        const int dx = static_cast<int>(xNorm * static_cast<float>(rx));
        fb.FillSpan(y, cx - dx, cx + dx + 1, r, g, b, 255);
    }
}

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL - Drawing Shapes (Software)";
    settings.client_rect.x = 100;
    settings.client_rect.y = 100;
    settings.client_rect.w = 960;
    settings.client_rect.z = 600;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed.\n";
        return 1;
    }

    auto window = CreateWindowOnSharedBackend(backend, settings);
    if (!window) {
        std::cerr << "CreateWindowOnSharedBackend failed.\n";
        backend.Shutdown();
        return 1;
    }

    window->CreateWindow();
    ApplicationContextSettings& ctx = window->RetrieveCurrentSettings();
    if (!ctx.window_handle) {
        std::cerr << "CreateWindow failed.\n";
        backend.Shutdown();
        return 1;
    }

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;

    Software2DContext frame{};
    if (!frame.Resize(fbw, fbh)) {
        std::cerr << "Software2DContext::Resize failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    backend.SetTargetFrameRate(60);
    bool running = true;

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
        }

        // 1) Clear full frame
        frame.Clear(18, 22, 34, 255);

        // 2) Draw a UI-like top bar
        frame.FillRect(0, 0, fbw, 42, 30, 36, 52, 255);

        // 3) Animate one rectangle using backend ticks
        const double t = static_cast<double>(backend.GetTicks()) / 60.0;
        const float x = 80.0f + std::cos(t * 1.1) * 220.0f;
        const float y = 140.0f + std::sin(t * 1.5) * 80.0f;
        frame.FillRect(static_cast<int>(x), static_cast<int>(y), 140, 100, 236, 111, 54, 255);

        // 4) Circle + ellipse
        DrawCircleFilled(frame, fbw / 2 + 160, fbh / 2 + 40, 60, 88, 185, 255);
        DrawEllipseFilled(frame, fbw / 2 + 340, fbh / 2 + 40, 110, 60, 150, 110, 245);

        // 5) Lines (Bresenham)
        DrawLine(frame, 40, fbh - 80, 280, fbh - 180, 255, 235, 100);
        DrawLine(frame, 280, fbh - 180, 520, fbh - 80, 255, 235, 100);
        DrawLine(frame, 520, fbh - 80, 40, fbh - 80, 255, 235, 100);

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}
```

---

## 4) Shape Walkthrough by Primitive

### A) Rectangles

Use `FillRect` for boxes, panels, and sprite-like blocks:

```cpp
frame.FillRect(40, 50, 180, 90, 230, 80, 40, 255);
```

### B) Lines

`Software2DContext` does not expose a direct `DrawLine` helper, so draw lines with `SetPixel` in a small helper (Bresenham):

```cpp
DrawLine(frame, 20, 20, 300, 120, 255, 255, 120);
```

### C) Circles

Use scanlines (`FillSpan`) per row for a filled circle:

```cpp
DrawCircleFilled(frame, 420, 260, 60, 80, 180, 255);
```

### D) Ellipses

Ellipse (sometimes written as "eclipse" by mistake) uses the same scanline idea with separate radii:

```cpp
DrawEllipseFilled(frame, 650, 260, 120, 70, 180, 120, 255);
```

---

## 5) Shape APIs in `Software2DContext`

Useful built-ins for 2D software drawing:

- `Clear(r, g, b, a)` - fills the whole framebuffer
- `FillRect(x, y, w, h, r, g, b, a)` - clipped rectangle
- `FillSpan(y, x0, x1, r, g, b, a)` - fast horizontal line segment
- `SetPixel(x, y, r, g, b, a)` - direct pixel write

`FillSpan` is especially handy for scanline rasterization (circles, triangles, custom polygons).

---

## 6) Resize Handling

On `RESIZE`, update framebuffer dimensions and reallocate:

```cpp
if (ev.type == HonkordGL::Events::EventType::RESIZE) {
    ctx.frame_buffer_width = ev.width;
    ctx.frame_buffer_height = ev.height;
    fbw = ev.width;
    fbh = ev.height;
    (void)frame.Resize(fbw, fbh);
}
```

If you skip this, your presented image can become stretched, clipped, or invalid after resize.

---

## 7) Tips for Better Software Rendering

- Draw background first, foreground last.
- Use integer coordinates for crisp edges.
- Prefer `FillRect`/`FillSpan` loops over repeated `SetPixel` for performance.
- Keep frame pacing enabled (`SetTargetFrameRate` + `DelayFrame`).
- Use `frame.StrideBytes()` when presenting instead of assuming `width * 4`.

---

## 8) Next Step Ideas

After this lesson, try:

- drawing outlined and filled triangles with scanline methods
- adding a camera transform (world-to-screen mapping)
- adding debug overlay text with `TextUIBindSoftware2D` + `TextUIDraw`
- building a simple HUD and collision visualizer

That is the full software-renderer workflow for drawing 2D shapes in HonkordGL: allocate framebuffer, rasterize CPU-side shapes, and present each frame.