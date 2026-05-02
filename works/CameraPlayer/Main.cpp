/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Single-window camera player demo (software framebuffer):
 * - One player moving in a larger world
 * - Camera2D follows the player
 * - Debug info rendered in-frame (TextUI), not in window title
 */

#include <HonkordGL/Camera.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/TextUI.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::Camera2D;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::Rectf;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::TextUIBegin;
using HonkordGL::Graphics::TextUIBindSoftware2D;
using HonkordGL::Graphics::TextUIDraw;
using HonkordGL::Graphics::TextUIEnd;
using HonkordGL::Graphics::TextUINextColor;
using HonkordGL::Graphics::TextUIShutdown;
using HonkordGL::Graphics::TextUITextRect;
using HonkordGL::Graphics::TextUITextSize;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr float kWorldW = 2600.f;
constexpr float kWorldH = 1600.f;
constexpr int kPlayerSize = 24;
constexpr int kHudH = 72;

struct Player {
    float x{220.f};
    float y{180.f};
    float vx{0.f};
    float vy{0.f};
};

struct InputState {
    bool up{};
    bool down{};
    bool left{};
    bool right{};
};

float ClampF(float v, float lo, float hi) noexcept
{
    return std::max(lo, std::min(v, hi));
}

Rectf ComputeOutputRect(Camera2D const & cam, int fbw, int fbh) noexcept
{
    const Rectf v = cam.ViewportRegionNormalized();
    return Rectf{
        v.x * static_cast<float>(fbw),
        v.y * static_cast<float>(fbh),
        v.w * static_cast<float>(fbw),
        v.z * static_cast<float>(fbh),
    };
}

void WorldToScreen(Camera2D const & cam, Rectf const & outRect, float wx, float wy, float & sx, float & sy) noexcept
{
    const float zoom = std::max(0.0001f, cam.Zoom());
    const float viewW = outRect.w / zoom;
    const float viewH = outRect.z / zoom;
    const float worldLeft = cam.FocusX() - viewW * 0.5f;
    const float worldTop = cam.FocusY() - viewH * 0.5f;
    sx = outRect.x + (wx - worldLeft) * zoom;
    sy = outRect.y + (wy - worldTop) * zoom;
}

void DrawWorld(Software2DContext & fb, Camera2D const & cam, Rectf const & outRect) noexcept
{
    const int x0 = static_cast<int>(outRect.x);
    const int y0 = static_cast<int>(outRect.y);
    const int w = static_cast<int>(outRect.w);
    const int h = static_cast<int>(outRect.z);
    fb.FillRect(x0, y0, w, h, 14, 18, 26, 255);

    const float zoom = std::max(0.0001f, cam.Zoom());
    const float viewW = outRect.w / zoom;
    const float viewH = outRect.z / zoom;
    const float worldLeft = cam.FocusX() - viewW * 0.5f;
    const float worldTop = cam.FocusY() - viewH * 0.5f;

    const float minor = 50.f;
    const float major = 250.f;

    const int xStart = static_cast<int>(std::floor(worldLeft / minor)) - 1;
    const int xEnd = static_cast<int>(std::ceil((worldLeft + viewW) / minor)) + 1;
    for (int i = xStart; i <= xEnd; ++i) {
        const float wx = static_cast<float>(i) * minor;
        const float sx = outRect.x + (wx - worldLeft) * zoom;
        const bool isMajor = std::fmod(std::abs(wx), major) < 0.01f;
        const unsigned char c = static_cast<unsigned char>(isMajor ? 70 : 34);
        fb.FillRect(static_cast<int>(sx), y0, 1, h, c, c, static_cast<unsigned char>(c + 10), 255);
    }

    const int yStart = static_cast<int>(std::floor(worldTop / minor)) - 1;
    const int yEnd = static_cast<int>(std::ceil((worldTop + viewH) / minor)) + 1;
    for (int i = yStart; i <= yEnd; ++i) {
        const float wy = static_cast<float>(i) * minor;
        const float sy = outRect.y + (wy - worldTop) * zoom;
        const bool isMajor = std::fmod(std::abs(wy), major) < 0.01f;
        const unsigned char c = static_cast<unsigned char>(isMajor ? 70 : 34);
        fb.FillRect(x0, static_cast<int>(sy), w, 1, c, c, static_cast<unsigned char>(c + 10), 255);
    }
}

void DrawPlayer(Software2DContext & fb, Camera2D const & cam, Rectf const & outRect, Player const & p) noexcept
{
    float sx = 0.f;
    float sy = 0.f;
    WorldToScreen(cam, outRect, p.x, p.y, sx, sy);
    const int size = std::max(6, static_cast<int>(kPlayerSize * cam.Zoom()));
    fb.FillRect(static_cast<int>(sx) - size / 2, static_cast<int>(sy) - size / 2, size, size, 88, 185, 255, 255);
}

void DrawHud(Software2DContext & fb, int fbw) noexcept
{
    fb.FillRect(0, 0, fbw, kHudH, 22, 26, 36, 255);
}

void HandleEvents(ApplicationContextSettings & ctx, bool & running, int & fbw, int & fbh, InputState & in) noexcept
{
    using HonkordGL::Events::EventType;
    using HonkordGL::Events::KeyCode;

    HonkordGL::Events::Event ev{};
    while (HonkordGL::Events::PollEvent(ctx, ev)) {
        if (ev.type == EventType::QUIT)
            running = false;
        if (ev.type == EventType::RESIZE) {
            ctx.frame_buffer_width = ev.width;
            ctx.frame_buffer_height = ev.height;
            fbw = ev.width;
            fbh = ev.height;
        }
        if (ev.type == EventType::KEY_PRESS || ev.type == EventType::KEY_RELEASE) {
            const bool down = ev.type == EventType::KEY_PRESS;
            switch (ev.key) {
            case KeyCode::W:
            case KeyCode::UP:
                in.up = down;
                break;
            case KeyCode::S:
            case KeyCode::DOWN:
                in.down = down;
                break;
            case KeyCode::A:
            case KeyCode::LEFT:
                in.left = down;
                break;
            case KeyCode::D:
            case KeyCode::RIGHT:
                in.right = down;
                break;
            case KeyCode::ESCAPE:
                if (down)
                    running = false;
                break;
            default:
                break;
            }
        }
    }
}

void UpdatePlayer(Player & p, InputState const & in, float dt) noexcept
{
    constexpr float accel = 980.f;
    constexpr float maxSpeed = 400.f;
    constexpr float damping = 0.90f;

    float ax = 0.f;
    float ay = 0.f;
    if (in.left)
        ax -= accel;
    if (in.right)
        ax += accel;
    if (in.up)
        ay -= accel;
    if (in.down)
        ay += accel;

    p.vx += ax * dt;
    p.vy += ay * dt;
    p.vx *= std::pow(damping, dt * 60.f);
    p.vy *= std::pow(damping, dt * 60.f);
    p.vx = ClampF(p.vx, -maxSpeed, maxSpeed);
    p.vy = ClampF(p.vy, -maxSpeed, maxSpeed);
    p.x = ClampF(p.x + p.vx * dt, static_cast<float>(kPlayerSize), kWorldW - static_cast<float>(kPlayerSize));
    p.y = ClampF(p.y + p.vy * dt, static_cast<float>(kPlayerSize), kWorldH - static_cast<float>(kPlayerSize));
}

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL - CameraPlayer";
    settings.client_rect.x = 90;
    settings.client_rect.y = 90;
    settings.client_rect.w = 1100;
    settings.client_rect.z = 700;

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
    ApplicationContextSettings & ctx = window->RetrieveCurrentSettings();
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
    if (TextUIBindSoftware2D(ctx, frame) != 0) {
        std::cerr << "TextUIBindSoftware2D failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    Camera2D camera{};
    camera.EncapsulateFramebuffer(ctx);
    camera.SetZoom(1.15f);
    camera.SetViewportRegionNormalized(0.f, 0.f, 1.f, 1.f);

    Player player{};
    InputState input{};
    bool running = true;
    backend.SetTargetFrameRate(60);
    std::uint64_t prevTick = backend.GetTicks();
    char debug[256]{};

    while (running) {
        HandleEvents(ctx, running, fbw, fbh, input);
        if (frame.Width() != fbw || frame.Height() != fbh) {
            (void)frame.Resize(fbw, fbh);
            camera.EncapsulateFramebuffer(ctx);
        }

        const std::uint64_t now = backend.GetTicks();
        const std::uint64_t ticks = now >= prevTick ? now - prevTick : 0;
        const float dt = std::min(0.05f, static_cast<float>(ticks) / 60.f);
        prevTick = now;

        UpdatePlayer(player, input, dt);
        camera.SetFocus(player.x, player.y);

        frame.Clear(10, 12, 18, 255);
        const Rectf outRect = ComputeOutputRect(camera, fbw, fbh);
        camera.MapViewportToRect(outRect);
        DrawWorld(frame, camera, outRect);
        DrawPlayer(frame, camera, outRect, player);
        DrawHud(frame, fbw);

        std::snprintf(
            debug,
            sizeof debug,
            "DEBUG  pos(%.1f, %.1f) vel(%.1f, %.1f) zoom %.2f  world %.0fx%.0f",
            player.x,
            player.y,
            player.vx,
            player.vy,
            camera.Zoom(),
            kWorldW,
            kWorldH);

        TextUIBegin();
        TextUINextColor(1.f, 1.f, 1.f, 1.f);
        TextUITextSize(15.f);
        TextUITextRect(10.f, 10.f, static_cast<float>(fbw - 20), static_cast<float>(kHudH - 12));
        TextUIDraw(debug);
        TextUIEnd();

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    TextUIShutdown();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}