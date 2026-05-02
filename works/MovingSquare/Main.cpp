/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Minimal window + **CPU software framebuffer**: animated solid rectangle and a top HUD line
 * with (x, y) via `TextUIBindSoftware2D` (same baked font as the GPU TextUI path). Title bar also
 * shows coordinates.
 *
 * Example build (Linux + X11/Wayland, from repo root):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -DHONKORDGL_TEXTUI_GPU=0 -c -o /tmp/TextUI_cpu.o src/TextUI.cpp
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     works/MovingSquare/Main.cpp works/MovingSquare/StbImageStub.cpp \
 *     src/Video.cpp src/WindowBackend.cpp src/SoftwareRenderer.cpp \
 *     /tmp/TextUI_cpu.o \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp src/X11/CursorX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o works/MovingSquare/MovingSquare \
 *     -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl
 *
 *   cd works/MovingSquare && ./MovingSquare
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/TextUI.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::TextUIBegin;
using HonkordGL::Graphics::TextUIBindSoftware2D;
using HonkordGL::Graphics::TextUIDraw;
using HonkordGL::Graphics::TextUIEnd;
using HonkordGL::Graphics::TextUINextColor;
using HonkordGL::Graphics::TextUIShutdown;
using HonkordGL::Graphics::TextUITextRect;
using HonkordGL::Graphics::TextUITextSize;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr int kHudH = 32;

void RenderScene(Software2DContext & fb, int rx, int ry, int rw, int rh) noexcept
{
    const int w = fb.Width();
    fb.Clear(15, 18, 26, 255);
    fb.FillRect(rx, ry, rw, rh, 235, 107, 46, 255);
    fb.FillRect(0, 0, w, kHudH, 30, 34, 42, 255);
}

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL — MovingSquare (software)";
    settings.client_rect.x = 80;
    settings.client_rect.y = 80;
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

    Software2DContext frame{};
    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (!frame.Resize(fbw, fbh)) {
        std::cerr << "Software2DContext::Resize failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    if (TextUIBindSoftware2D(ctx, frame) != 0) {
        std::cerr << "TextUIBindSoftware2D failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    constexpr int kRectW = 96;
    constexpr int kRectH = 96;

    bool running = true;

    backend.SetTargetFrameRate(60);

    std::cout << "Moving square (CPU framebuffer). Close the window to exit.\n";

    char title[160]{};
    char hud[96]{};

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

        const float fW = static_cast<float>(fbw);
        const float fH = static_cast<float>(fbh);
        const double t = static_cast<double>(backend.GetTicks()) / 60.0;
        const float margin = 24.f;
        const float rxMax = std::max(margin, fW - static_cast<float>(kRectW) - margin);
        const float ryMax = std::max(margin, fH - static_cast<float>(kRectH) - margin);
        const float cx = (margin + rxMax) * 0.5f;
        const float cy = (margin + ryMax) * 0.5f;
        const float ax = std::max(8.f, (rxMax - margin) * 0.5f - static_cast<float>(kRectW) * 0.5f);
        const float ay = std::max(8.f, (ryMax - margin) * 0.5f - static_cast<float>(kRectH) * 0.5f);
        const float rx = cx + std::cos(t * 1.1) * ax;
        const float ry = cy + std::sin(t * 1.1) * ay;

        const int irx = static_cast<int>(std::floor(rx));
        const int iry = static_cast<int>(std::floor(ry));

        RenderScene(frame, irx, iry, kRectW, kRectH);

        std::snprintf(hud, sizeof hud, "x=%d y=%d", irx, iry);
        TextUIBegin();
        TextUINextColor(1.f, 1.f, 1.f, 1.f);
        TextUITextSize(14.f);
        TextUITextRect(8.f, 8.f, static_cast<float>(fbw - 16), static_cast<float>(kHudH - 8));
        TextUIDraw(hud);
        TextUIEnd();

        std::snprintf(title, sizeof title, "HonkordGL — MovingSquare | x=%d y=%d", irx, iry);
        backend.SetWindowTitle(ctx, title);

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    TextUIShutdown();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}