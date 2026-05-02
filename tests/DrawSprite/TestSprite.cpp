/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Renders `../../Honkord__APILogo.png` (relative to the current working directory) with `Sprite::CreateFromImageFile`.
 * Run from `tests/DrawSprite/` so the path resolves to the repo-root logo, or set cwd accordingly.
 *
 * Example build (Linux + X11/Wayland, from repo root — same object list as tests/Window/TestWindow.cpp):
 *   g++ -std=c++17 -I include -I src/Wayland \
 *     tests/DrawSprite/TestSprite.cpp \
 *     src/Video.cpp src/WindowBackend.cpp \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/X11/GLXRendererContext.cpp src/X11/EGLRendererContextX11.cpp \
 *     src/Wayland/EGLRendererContextWayland.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/GpuRenderer.cpp src/Sprite.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o tests/DrawSprite/TestSprite \
 *     -lX11 -lXrandr -lXi -lwayland-client -lwayland-egl -lEGL -lGL
 *
 *   cd tests/DrawSprite && ./TestSprite
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/Sprite.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::Sprite;
using HonkordGL::Graphics::WindowBackend;

namespace {

bool LoadLogo(Sprite& sprite, ApplicationContextSettings& ctx)
{
    static const char * const paths[] = {
        "../../Honkord__APILogo.png",
        "Honkord__APILogo.png",
    };
    for (const char * const p : paths) {
        if (sprite.CreateFromImageFile(ctx, p)) {
            std::cout << "Loaded image: " << p << " (" << sprite.TextureWidth() << "x" << sprite.TextureHeight()
                      << ")\n";
            return true;
        }
    }
    std::cerr << "CreateFromImageFile failed for ../../Honkord__APILogo.png (and fallback). "
               << "Place Honkord__APILogo.png at the repo root or run from tests/DrawSprite/.\n";
    return false;
}

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings ctx{};
    ctx.window_title = "HonkordGL — TestSprite";
    ctx.client_rect.x = 120;
    ctx.client_rect.y = 120;
    ctx.client_rect.w = 900;
    ctx.client_rect.z = 700;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed.\n";
        return 1;
    }

    if (!backend.CreateWindow(ctx)) {
        std::cerr << "CreateWindow failed\n";
        backend.Shutdown();
        return 1;
    }

    constexpr int kTargetFps = 60;
    backend.SetTargetFrameRate(kTargetFps);

    GpuRenderer gpu(ctx);
    Sprite sprite{};
    const int attachRc = gpu.CreateBestAvailableShaderBackend();
    if (attachRc != 0) {
        std::cerr << "GpuRenderer::CreateBestAvailableShaderBackend failed: " << attachRc;
        if (attachRc == static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND))
            std::cerr << " (UNSUPPORTED_BACKEND)";
        std::cerr << '\n';
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    gpu.MakeCurrent();
    gpu.SetDefaultViewport();
    std::cout << "GPU backend: " << gpu.ActiveBackendLabel() << '\n';

    if (!LoadLogo(sprite, ctx)) {
        gpu.Destroy();
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    {
        const float tw = static_cast<float>(sprite.TextureWidth());
        const float th = static_cast<float>(sprite.TextureHeight());
        const float fbw = static_cast<float>(ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w);
        const float fbh = static_cast<float>(ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z);
        const float x = std::floor((fbw - tw) * 0.5f);
        const float y = std::floor((fbh - th) * 0.5f);
        sprite.SetPosition(x, y);
    }

    std::cout << "Close the window to exit.\n";

    bool running = true;
    while (running) {
        HonkordGL::Events::Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            if (ev.type == HonkordGL::Events::EventType::QUIT)
                running = false;
            if (ev.type == HonkordGL::Events::EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                gpu.MakeCurrent();
                gpu.SetDefaultViewport();
                const float tw = static_cast<float>(sprite.TextureWidth());
                const float th = static_cast<float>(sprite.TextureHeight());
                const float fbw = static_cast<float>(ctx.frame_buffer_width);
                const float fbh = static_cast<float>(ctx.frame_buffer_height);
                sprite.SetPosition(std::floor((fbw - tw) * 0.5f), std::floor((fbh - th) * 0.5f));
            }
        }

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.ClearColorBuffer(0.08f, 0.1f, 0.14f, 1.f);
        if (sprite.IsValid())
            sprite.Draw(ctx);
        gpu.SwapBuffers();

        backend.DelayFrame();
    }

    gpu.Destroy();
    backend.DestroyWindow(ctx);
    backend.Shutdown();
    return 0;
}