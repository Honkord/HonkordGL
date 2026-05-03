# Event Handling

This guide explains how to handle window/input events in HonkordGL and shows code snippets for each feature used in a typical loop.

## 1) Event Flow Overview

HonkordGL exposes event handling through:

- `HonkordGL::Events::PollEvent(...)` for dequeuing normalized events.
- `HonkordGL::Graphics::WindowBackend::ProcessEvents(...)` for backend event pumping.
- Optional callbacks in `ApplicationContextSettings`:
  - `ResizeCallback`
  - `PresentCallback`
  - `DestroyCallback`

Typical frame order:

1. Pump backend events (`ProcessEvents`).
2. Drain queued events (`PollEvent` in a loop).
3. Update game/app state.
4. Render/present.

## 2) Minimal Poll Loop Example

```cpp
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/Events.h>

void Tick(HonkordGL::Graphics::WindowBackend& backend, HonkordGL::Graphics::ApplicationContextSettings& ctx)
{
    backend.ProcessEvents(ctx); // backend-level pump

    HonkordGL::Events::Event ev{};
    while (HonkordGL::Events::PollEvent(ctx, ev)) {
        switch (ev.type) {
        case HonkordGL::Events::EventType::QUIT:
            // running = false;
            break;
        case HonkordGL::Events::EventType::RESIZE:
            // ev.width / ev.height
            break;
        case HonkordGL::Events::EventType::MOUSE_MOVE:
            // ev.x / ev.y
            break;
        case HonkordGL::Events::EventType::KEY_PRESS:
            // ev.key, ev.character, ev.modifiers
            break;
        default:
            break;
        }
    }
}
```

## 3) Callback-Based Integration

You can attach callbacks through `ApplicationContextSettings` and still use polling.

```cpp
static void OnResize(HonkordGL::Graphics::ApplicationContextSettings * ctx, int w, int h)
{
    (void)ctx;
    // Reallocate framebuffer, update viewport, etc.
}

static void OnPresent(HonkordGL::Graphics::ApplicationContextSettings * ctx)
{
    (void)ctx;
    // Frame-present hook.
}

static void OnDestroy(HonkordGL::Graphics::ApplicationContextSettings * ctx)
{
    (void)ctx;
    // Cleanup resources tied to the window.
}

void InstallCallbacks(HonkordGL::Graphics::ApplicationContextSettings& ctx)
{
    ctx.ResizeCallback = &OnResize;
    ctx.PresentCallback = &OnPresent;
    ctx.DestroyCallback = &OnDestroy;
}
```

## 4) Key and Modifier Handling Snippet

```cpp
using namespace HonkordGL::Events;

void HandleKeyboard(const Event& ev)
{
    if (ev.type != EventType::KEY_PRESS && ev.type != EventType::KEY_RELEASE)
        return;

    const bool ctrl = (ev.modifiers & static_cast<unsigned>(ModifierFlags::CTRL)) != 0;
    const bool shift = (ev.modifiers & static_cast<unsigned>(ModifierFlags::SHIFT)) != 0;

    if (ev.type == EventType::KEY_PRESS && ctrl && ev.key == KeyCode::S) {
        // Save action
    }
    if (ev.type == EventType::KEY_PRESS && shift && ev.key == KeyCode::TAB) {
        // Reverse cycle tabs
    }
}
```

## 5) Mouse Handling Snippet

```cpp
using namespace HonkordGL::Events;

void HandleMouse(const Event& ev)
{
    switch (ev.type) {
    case EventType::MOUSE_ENTER:
        // Pointer entered client area
        break;
    case EventType::MOUSE_LEAVE:
        // Pointer left client area
        break;
    case EventType::MOUSE_MOVE:
        // ev.x / ev.y
        break;
    case EventType::MOUSE_BUTTON_PRESS:
    case EventType::MOUSE_BUTTON_RELEASE:
        // ev.mouse_button (X11-style: 1 left, 2 middle, 3 right, 4/5 wheel)
        break;
    default:
        break;
    }
}
```

---

## All Event References (Current API)

The list below is taken from `include/HonkordGL/Events.h` and `include/HonkordGL/WindowApplication.h`.

### Namespace and Core Types

- `HonkordGL::Events`
- `HonkordGL::Events::Event`
- `HonkordGL::Events::EventType`
- `HonkordGL::Events::KeyCode`
- `HonkordGL::Events::ModifierFlags`

### EventType Values

- `QUIT`
- `RESIZE`
- `FOCUS`
- `MOUSE_ENTER`
- `MOUSE_LEAVE`
- `MOUSE_MOVE`
- `MOUSE_BUTTON_PRESS`
- `MOUSE_BUTTON_RELEASE`
- `KEY_PRESS`
- `KEY_RELEASE`

### ModifierFlags Values (bitmask)

- `SHIFT`
- `CTRL`
- `ALT`
- `SUPER`
- `CAPS_LOCK`
- `NUM_LOCK`

### Event Struct Fields

- `type`
- `key`
- `mouse_button`
- `x`
- `y`
- `width`
- `height`
- `focused`
- `modifiers`
- `character`

### Event Functions

- `bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept`
- `bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept`

### Related Event/Window Callbacks (ApplicationContextSettings)

- `ResizeCallback(ApplicationContextSettings*, int w, int h)`
- `PresentCallback(ApplicationContextSettings*)`
- `DestroyCallback(ApplicationContextSettings*)`

### KeyCode Reference

`KeyCode` includes:

- alphabet keys `A..Z`
- numeric keys `NUMBER_0..NUMBER_9`
- function keys `F1..F20`
- control/navigation keys (`ESCAPE`, `ENTER`, `TAB`, `SPACE`, `SHIFT`, `CTRL`, `ALT`, `SUPER`, `META`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN`, arrows, etc.)
- punctuation/symbol keys (`COMMA`, `PERIOD`, `SLASH`, `BACKSLASH`, `TILDE`, `EQUAL`, `MINUS`, `PLUS`, `PIPE`, `AMPERSAND`, ...)

For the exact complete enum values, use `include/HonkordGL/Events.h`.
