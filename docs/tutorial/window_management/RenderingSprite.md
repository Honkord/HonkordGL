### Rendering Sprites with HonkordGL (Full API Walkthrough)

This guide walks through the full `HonkordGL::Graphics::Sprite` API: loading textures, transforms, atlas frames, pivots, shape drawing, collision tests, and rendering flow.

`Sprite` is a GPU textured quad API (OpenGL-family backend). For CPU-only drawing, use `Software2DContext` instead.

---

## 1) Prerequisites and Render Flow

Typical frame flow:

1. Create window (`WindowBackend`)
2. Create GPU backend (`GpuRenderer`)
3. Make context current (`GpuRenderer::MakeCurrent`)
4. Load sprite textures (`Sprite::CreateFrom...`)
5. Per frame: poll events -> clear -> set viewport -> configure sprite -> `Draw` -> `SwapBuffers`

Required headers:

```cpp
#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/Sprite.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>
```

---

## 2) Minimal Working Sprite Example

```cpp
#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/Sprite.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <cstdint>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::Sprite;
using HonkordGL::Graphics::WindowBackend;

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL - Sprite Walkthrough";
    settings.client_rect.x = 90;
    settings.client_rect.y = 80;
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

    GpuRenderer gpu(ctx);
    if (gpu.CreateBestAvailableShaderBackend() != 0) {
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }
    gpu.MakeCurrent();
    gpu.SetDefaultViewport();

    // 2x2 white texture
    std::uint8_t px[16] = {
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
    };
    Sprite sprite{};
    if (!sprite.CreateFromRGBA(ctx, 2, 2, px, 0)) {
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    sprite.SetPosition(220.f, 180.f);
    sprite.SetSize(180.f, 140.f);

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
            }
        }

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.ClearColorBuffer(0.07f, 0.08f, 0.12f, 1.f);

        sprite.Draw(ctx);

        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    sprite.Destroy();
    gpu.Destroy();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}
```

---

## 3) Texture Creation APIs

`Sprite` supports multiple texture upload/loading paths.

### A) `CreateFromRGBA(...)`

Uploads RGBA8 row-major data.

```cpp
std::uint8_t rgba[] = {
    255, 0, 0, 255,   0, 255, 0, 255,
    0, 0, 255, 255,   255, 255, 0, 255,
};
sprite.CreateFromRGBA(2, 2, rgba, 0); // stride 0 -> width*4
```

Context-aware overload:

```cpp
sprite.CreateFromRGBA(ctx, 2, 2, rgba, 0);
```

### B) `CreateFromPixelBuffer(...)`

Same concept as RGBA upload but with `unsigned char const*`.

```cpp
unsigned char const* pixels = ...;
sprite.CreateFromPixelBuffer(256, 256, pixels, 256 * 4);
```

### C) `CreateFromImageFile(...)`

Loads image files via stb_image (png/jpeg/bmp/gif/tga and others supported by stb).

```cpp
sprite.CreateFromImageFile("assets/player.png");
// or:
sprite.CreateFromImageFile(ctx, "assets/player.png");
```

### D) Validation and cleanup

```cpp
if (!sprite.IsValid()) {
    // handle load/upload failure
}
sprite.Destroy();
```

---

## 4) Placement, Size, Rotation, Scale

```cpp
sprite.SetPosition(320.f, 240.f);          // top-left in client pixels
sprite.SetSize(128.f, 128.f);              // draw size
sprite.SetRotationDegrees(35.f);           // CCW around pivot
sprite.SetScale(1.25f, 0.9f);              // non-uniform
sprite.SetScale(1.1f);                     // uniform
```

Useful getters:

```cpp
float x = sprite.PositionX();
float y = sprite.PositionY();
float w = sprite.DrawWidth();
float h = sprite.DrawHeight();
float rot = sprite.RotationDegrees();
```

---

## 5) Atlas / Sub-Region Rendering

Two ways to use sprite sheets:

### A) `SetImageFrame(...)`

Pick a source rectangle in texels and keep destination controlled by `SetPosition/SetSize`.

```cpp
HonkordGL::Graphics::Recti frame{};
frame.x = 64;   // source x in texture
frame.y = 32;   // source y in texture
frame.w = 32;   // source width
frame.z = 32;   // source height
sprite.SetImageFrame(frame);
```

Reset to full texture:

```cpp
sprite.ClearImageFrame();
```

Inspect current frame:

```cpp
if (sprite.HasImageFrame()) {
    auto r = sprite.ImageFrame();
}
```

### B) `ApplyRects(dst, src)`

Set destination and source rectangles together in one call.

```cpp
HonkordGL::Graphics::Recti dst{100, 120, 180, 120}; // client pixels
HonkordGL::Graphics::Recti src{0, 0, 48, 32};       // texture texels
sprite.ApplyRects(dst, src);
```

Note: `ApplyRects` replaces frame/placement mode from `SetImageFrame` and `SetPosition/SetSize`.

---

## 6) Pivot Control

Pivot affects rotation and scaling center.

```cpp
sprite.SetImagePivot(16.f, 16.f);  // local offset from sprite top-left in destination pixels
sprite.SetRotationDegrees(90.f);
```

Reset to default center pivot:

```cpp
sprite.ResetImagePivot();
```

Query pivot state:

```cpp
bool custom = sprite.HasCustomImagePivot();
float px = sprite.ImagePivotX();
float py = sprite.ImagePivotY();
```

---

## 7) Entity Sync with `Lines` / `Eclipse`

If you already animate transforms on `Lines` or `Eclipse`, copy those transforms to a sprite:

```cpp
// Assume `lines` was transformed elsewhere
sprite.ApplyImageEntityFrom(lines);

// Assume `eclipse` was transformed elsewhere
sprite.ApplyImageEntityFrom(eclipse);
```

This copies entity position, rotation, scale, and pivot conventions.

---

## 8) Shape Drawing API on `Sprite`

`Sprite` also supports recording and flushing vector-like primitives.

### A) Begin/End shape pass

```cpp
sprite.BeginShape();
// add shape commands
sprite.EndShape(ctx);
```

### B) Filled triangle

```cpp
sprite.TriangleFill(
    120.f, 120.f,
    220.f, 300.f,
    360.f, 160.f,
    0.2f, 0.85f, 0.35f, 1.f);
```

### C) Circle with optional border ring

```cpp
sprite.Circle(
    520.f, 240.f, 64.f,
    0.9f, 0.2f, 0.2f, 1.f,   // fill RGBA
    0.25f, 0.05f, 0.05f, 1.f, // border RGBA
    3.0f);                     // border thickness
```

Border only (no fill):

```cpp
sprite.Circle(
    520.f, 240.f, 64.f,
    0.f, 0.f, 0.f, 0.f,       // fill alpha 0
    1.f, 1.f, 1.f, 1.f,
    2.0f);
```

---

## 9) Collision / Picking Helpers

### A) Sprite-vs-sprite transformed bounds

```cpp
if (playerSprite.IntersectsEntityBounds(enemySprite)) {
    // collision response
}
```

### B) Point hit test

```cpp
if (buttonSprite.ContainsClientPoint(static_cast<float>(mouseX), static_cast<float>(mouseY))) {
    // hover/click logic
}
```

---

## 10) Recommended Per-Frame Order

```cpp
gpu.MakeCurrent();
gpu.SetDefaultViewport();
gpu.ClearColorBuffer(0.08f, 0.08f, 0.11f, 1.f);

// Update transforms first
sprite.SetPosition(...);
sprite.SetRotationDegrees(...);
sprite.SetScale(...);

// Draw textured sprites
sprite.Draw(ctx);

// Optional shape pass
sprite.BeginShape();
sprite.TriangleFill(...);
sprite.Circle(...);
sprite.EndShape(ctx);

gpu.SwapBuffers();
```

---

## 11) Platform Notes and Limitations

- Desktop platforms use OpenGL-family backend for sprite rendering.
- On DRM-only path, sprite textured draw is skipped (no GL context in this build).
- Prefer `CreateFromRGBA(ctx, ...)` / `CreateFromImageFile(ctx, ...)` when you want platform-aware checks.
- Always call draw/upload with a current GPU context (`GpuRenderer::MakeCurrent`).

---

## 12) API Checklist (Quick Reference)

- Lifecycle: `CreateFrom...`, `IsValid`, `Destroy`
- Transform: `SetPosition`, `SetSize`, `SetRotationDegrees`, `SetScale`
- Atlas: `SetImageFrame`, `ClearImageFrame`, `ApplyRects`, `ImageFrame`
- Pivot: `SetImagePivot`, `ResetImagePivot`, pivot getters
- Entity sync: `ApplyImageEntityFrom(Lines/Eclipse)`
- Draw: `Draw(ctx)`
- Shape path: `BeginShape`, `TriangleFill`, `Circle`, `EndShape(ctx)`
- Queries: `TextureWidth/Height`, position/size/scale getters
- Interaction: `IntersectsEntityBounds`, `ContainsClientPoint`

This covers the complete practical surface of the `Sprite` API in HonkordGL.
