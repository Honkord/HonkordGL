/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Deferred sample: the application owns the window and event loop. After `CreateWindow` and
 * `DeferredRendererSampleCreate`, call `RunDeferredRendererSampleMain(context, settings)` once per frame
 * inside the loop (after draining `PollEvent`). On `RESIZE`, update `ctx.frame_buffer_width` /
 * `ctx.frame_buffer_height` so the sample can rebuild its G-buffer.
 *
 * Link `src/DeferredRendererSample.cpp`, `src/internal/DeferredRendererGpuApi.cpp`, `src/GpuShaderCompiler.cpp`,
 * `src/GpuRenderer.cpp`, `src/TextUI.cpp` (optional text UI), plus window/video/software objects (command below).
 *
 * Example build (Linux + X11/Wayland, from repo root):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     tests/DeferredRenderer/TestDeferredRenderer.cpp \
 *     src/DeferredRendererSample.cpp src/internal/DeferredRendererGpuApi.cpp \
 *     src/GpuShaderCompiler.cpp \
 *     src/Video.cpp src/WindowBackend.cpp \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/X11/GLXRendererContext.cpp src/X11/EGLRendererContextX11.cpp \
 *     src/Wayland/EGLRendererContextWayland.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/GpuRenderer.cpp src/TextUI.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o tests/DeferredRenderer/TestDeferredRenderer \
 *     -lX11 -lXrandr -lXi -lwayland-client -lwayland-egl -lEGL -lGL
 *
 *   cd tests/DeferredRenderer && ./TestDeferredRenderer
 */

#include <HonkordGL/DeferredRendererSample.h>
#include <HonkordGL/Events.h>

#include <cstdlib>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::DeferredRendererSampleContext;
using HonkordGL::Graphics::DeferredRendererSampleCreate;
using HonkordGL::Graphics::DeferredRendererSampleDestroy;
using HonkordGL::Graphics::DeferredRendererSampleSettings;
using HonkordGL::Graphics::RunDeferredRendererSampleMain;
using HonkordGL::Graphics::WindowBackend;

int main()
{
    DeferredRendererSampleSettings custom{};
    custom.window_title = "HonkordGL — Custom Deferred Sample";
    custom.client_x = 120;
    custom.client_y = 60;
    custom.client_w = 1280;
    custom.client_h = 720;
    custom.target_fps = 60;
    custom.mesh_albedo[0] = 0.25f;
    custom.mesh_albedo[1] = 0.75f;
    custom.mesh_albedo[2] = 0.45f;
    custom.light_dir_world[0] = 0.3f;
    custom.light_dir_world[1] = 0.85f;
    custom.light_dir_world[2] = 0.45f;
    custom.light_color[0] = 0.95f;
    custom.light_color[1] = 0.92f;
    custom.light_color[2] = 1.0f;
    custom.background_rgb[0] = 0.01f;
    custom.background_rgb[1] = 0.015f;
    custom.background_rgb[2] = 0.03f;

    WindowBackend backend;
    ApplicationContextSettings ctx{};
    static constexpr char kDefaultTitle[] = "HonkordGL — DeferredRenderer";
    ctx.window_title = custom.window_title ? custom.window_title : kDefaultTitle;
    ctx.client_rect.x = custom.client_x;
    ctx.client_rect.y = custom.client_y;
    ctx.client_rect.w = custom.client_w > 0 ? custom.client_w : 960;
    ctx.client_rect.z = custom.client_h > 0 ? custom.client_h : 600;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed.\n";
        return 1;
    }
    if (!backend.CreateWindow(ctx)) {
        std::cerr << "CreateWindow failed.\n";
        backend.CloseDisplay();
        return 1;
    }

    DeferredRendererSampleContext * sample = nullptr;
    if (DeferredRendererSampleCreate(ctx, backend, custom, &sample) != 0) {
        std::cerr << "DeferredRendererSampleCreate failed.\n";
        backend.DestroyWindow(ctx);
        backend.CloseDisplay();
        return 1;
    }

    std::cout << "DeferredRenderer: G-buffer + directional light. Close window to exit.\n";

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

        RunDeferredRendererSampleMain(sample, custom);
    }

    DeferredRendererSampleDestroy(sample);
    backend.DestroyWindow(ctx);
    backend.CloseDisplay();
    return 0;
}