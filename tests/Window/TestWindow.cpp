/**
    * @author Honkord <https://github.com>

    Copyright (C) 2026 Honkord
    (See LICENSE.md for what this file is licensed under)

    Minimal test: open a window whose primary job is to deliver input and lifecycle
    events; a separate render pass each frame presents a CPU `Software2DContext`
    (checkerboard) via `WindowBackend::PresentSoftwareFramebuffer(...)`.
    RandR / monitor polling is enabled automatically inside `WindowBackend::CreateWindow`.
    There is no `#if` platform gate in this file; link the appropriate `WindowBackend` / `Events`
    objects for your OS when building the test.

    Example build (Linux + X11/Wayland, from repo root — no GPU/GL objects required for this test):
      gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
      g++ -std=c++17 -I include -I src/Wayland \
        tests/Window/TestWindow.cpp \
        src/Video.cpp \
        src/WindowBackend.cpp \
        src/X11/WindowBackend.cpp \
        src/X11/EventsX11.cpp \
        src/Wayland/EventsWayland.cpp \
        src/Wayland/WindowBackend.cpp \
        src/Wayland/PlatformSession.cpp \
        src/Wayland/IpcWayland.cpp \
        src/X11/RandrMonitorPoll.cpp \
        src/X11/IpcHelperWindow.cpp \
        src/X11/MonitorsX11.cpp \
        src/DRM/EventsDRM.cpp \
        src/Software2DContext.cpp \
        src/SoftwareBlitCollector.cpp \
        /tmp/xdg-shell.o \
        -o tests/Window/TestWindow \
        -lX11 -lXrandr -lXi -lwayland-client -lwayland-cursor -lwayland-egl

    Example build (macOS + Cocoa):
      clang++ -std=c++17 -ObjC++ -fobjc-arc -I include \
        tests/Window/TestWindow.cpp \
        src/Video.cpp \
        src/WindowBackend.cpp \
        src/Cocoa/WindowBackend.mm \
        src/Cocoa/EventsCocoa.mm \
        src/Cocoa/DisplayMonitorPoll.mm \
        src/Cocoa/CocoaIpcWindow.mm \
        src/Cocoa/MonitorsCocoa.mm \
        src/DRM/EventsDRM.cpp \
        src/Software2DContext.cpp \
        src/SoftwareBlitCollector.cpp \
        -o tests/Window/TestWindow \
        -framework Cocoa -framework Carbon

    Example build (Windows, MSVC, from repo root):
      cl /std:c++17 /EHsc /I include /I src\\Win32 ^
        tests\\Window\\TestWindow.cpp src\\Video.cpp src\\WindowBackend.cpp ^
        src\\Win32\\WindowBackend.cpp src\\Win32\\EventsWin32.cpp ^
        src\\DRM\\EventsDRM.cpp ^
        src\\Software2DContext.cpp ^
        src\\SoftwareBlitCollector.cpp ^
        /Fe:tests\\Window\\TestWindow.exe user32.lib gdi32.lib
*/

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/Software2DContext.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::Recti;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::WindowBackend;
using HonkordGL::Graphics::WindowBackendKind;

static const char * EventTypeName(EventType t) noexcept
{
    switch (t) {
    case EventType::QUIT:
        return "QUIT";
    case EventType::RESIZE:
        return "RESIZE";
    case EventType::FOCUS:
        return "FOCUS";
    case EventType::MOUSE_ENTER:
        return "MOUSE_ENTER";
    case EventType::MOUSE_LEAVE:
        return "MOUSE_LEAVE";
    case EventType::MOUSE_MOVE:
        return "MOUSE_MOVE";
    case EventType::MOUSE_BUTTON_PRESS:
        return "MOUSE_BUTTON_PRESS";
    case EventType::MOUSE_BUTTON_RELEASE:
        return "MOUSE_BUTTON_RELEASE";
    case EventType::KEY_PRESS:
        return "KEY_PRESS";
    case EventType::KEY_RELEASE:
        return "KEY_RELEASE";
    default:
        return "?";
    }
}
static void FillCheckerboard(Software2DContext& surf, int sw, int sh) noexcept
{
    constexpr int cell = 16;
    if (!surf.Resize(sw, sh))
        return;
    std::uint8_t * const px = surf.Pixels();
    if (!px)
        return;
    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            const bool c = (((x / cell) + (y / cell)) & 1) == 0;
            std::uint8_t * const p = &px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(sw)
                                         + static_cast<std::size_t>(x))
                                        * 4u];
            p[0] = c ? static_cast<std::uint8_t>(220) : static_cast<std::uint8_t>(40);
            p[1] = c ? static_cast<std::uint8_t>(90) : static_cast<std::uint8_t>(160);
            p[2] = c ? static_cast<std::uint8_t>(40) : static_cast<std::uint8_t>(220);
            p[3] = 255;
        }
    }
}

/**
 * Drains the platform event queue: logging, quit/resize handling, monitor hotplug notice.
 * Does not present pixels; resize updates `ctx` framebuffer size and rebuilds `frame`.
 */
static void HandleWindowEvents(ApplicationContextSettings& ctx, WindowBackend& backend, bool& running, Software2DContext& frame, int& fbw,
    int& fbh)
{
    Event ev{};
    while (HonkordGL::Events::PollEvent(ctx, ev)) {
        std::cout << EventTypeName(ev.type);
        if (ev.type == EventType::RESIZE)
            std::cout << " " << ev.width << "x" << ev.height;
        if (ev.type == EventType::MOUSE_MOVE || ev.type == EventType::MOUSE_BUTTON_PRESS
            || ev.type == EventType::MOUSE_BUTTON_RELEASE)
            std::cout << " @" << ev.x << "," << ev.y << " btn=" << ev.mouse_button;
        if (ev.type == EventType::KEY_PRESS || ev.type == EventType::KEY_RELEASE)
            std::cout << " char=" << ev.character;
        std::cout << '\n';

        if (ev.type == EventType::QUIT)
            running = false;
        if (ev.type == EventType::RESIZE) {
            ctx.frame_buffer_width = ev.width;
            ctx.frame_buffer_height = ev.height;
            fbw = ev.width;
            fbh = ev.height;
            FillCheckerboard(frame, fbw, fbh);
        }
    }

    if (backend.PollMonitorsChanged())
        std::cout << "[Display] monitor layout may have changed\n";
}

/** Single render step: blit the CPU framebuffer to the window (no event I/O). */
static void RenderFrame(WindowBackend& backend, ApplicationContextSettings& ctx, const Software2DContext& frame)
{
    (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
}

int main()
{
    WindowBackend backend;
    ApplicationContextSettings ctx{};
    ctx.window_title = "HonkordGL — TestWindow";
    ctx.client_rect.x = 120;
    ctx.client_rect.y = 120;
    ctx.client_rect.w = 800;
    ctx.client_rect.z = 600;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed (no display driver succeeded).\n";
        return 1;
    }

    if (!backend.CreateWindow(ctx)) {
        std::cerr << "CreateWindow failed\n";
        backend.CloseDisplay();
        return 1;
    }

    Software2DContext frame{};
    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    FillCheckerboard(frame, fbw, fbh);

    std::cout << "Window open (events only in HandleWindowEvents; RenderFrame presents each tick).\n";
    std::cout << "Frame pacing: WindowBackend::DelayFrame() after each present (60 FPS target).\n";
    std::cout << "Close the window or press keys; Ctrl+C to abort.\n";

    bool running = true;
    backend.SetTargetFrameRate(60);

    RenderFrame(backend, ctx, frame);

    while (running) {
        HandleWindowEvents(ctx, backend, running, frame, fbw, fbh);
        RenderFrame(backend, ctx, frame);
        backend.DelayFrame();
    }

    backend.DestroyWindow(ctx);
    backend.CloseDisplay();
    return 0;
}