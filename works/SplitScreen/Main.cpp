/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Split-screen camera demo game (software framebuffer):
 * - Two players move in a shared world
 * - Each half of the window uses its own Camera2D view
 * - Uses Camera2D::BuildSplitScreen to configure horizontal slices
 */

#include <HonkordGL/Camera.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::Camera2D;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::Rectf;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr float kWorldW = 2800.f;
constexpr float kWorldH = 1600.f;
constexpr int kPlayerSize = 24;

struct Player {
    float x{};
    float y{};
    float vx{};
    float vy{};
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    int score{};
};

struct Orb {
    float x{};
    float y{};
    float radius{};
    bool alive{true};
};

struct InputState {
    bool w{};
    bool a{};
    bool s{};
    bool d{};
    bool up{};
    bool left{};
    bool down{};
    bool right{};
};

float ClampF(float v, float lo, float hi) noexcept
{
    return std::max(lo, std::min(v, hi));
}

void DrawCircle(Software2DContext & fb, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
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

void DrawWorldGrid(Software2DContext & fb, Rectf const & outRect, Camera2D const & cam) noexcept
{
    const float zoom = std::max(0.0001f, cam.Zoom());
    const float worldViewW = outRect.w / zoom;
    const float worldViewH = outRect.z / zoom;
    const float worldLeft = cam.FocusX() - worldViewW * 0.5f;
    const float worldTop = cam.FocusY() - worldViewH * 0.5f;

    const int x0 = static_cast<int>(outRect.x);
    const int y0 = static_cast<int>(outRect.y);
    const int w = static_cast<int>(outRect.w);
    const int h = static_cast<int>(outRect.z);

    fb.FillRect(x0, y0, w, h, 15, 18, 28, 255);

    const float major = 200.f;
    const float minor = 50.f;
    const int xStart = static_cast<int>(std::floor(worldLeft / minor)) - 1;
    const int xEnd = static_cast<int>(std::ceil((worldLeft + worldViewW) / minor)) + 1;
    for (int gx = xStart; gx <= xEnd; ++gx) {
        const float wx = static_cast<float>(gx) * minor;
        const float sx = outRect.x + (wx - worldLeft) * zoom;
        const int px = static_cast<int>(std::floor(sx));
        const bool isMajor = std::fmod(std::abs(wx), major) < 0.01f;
        const std::uint8_t c = static_cast<std::uint8_t>(isMajor ? 66 : 36);
        fb.FillRect(px, y0, 1, h, c, c, static_cast<std::uint8_t>(c + 12), 255);
    }

    const int yStart = static_cast<int>(std::floor(worldTop / minor)) - 1;
    const int yEnd = static_cast<int>(std::ceil((worldTop + worldViewH) / minor)) + 1;
    for (int gy = yStart; gy <= yEnd; ++gy) {
        const float wy = static_cast<float>(gy) * minor;
        const float sy = outRect.y + (wy - worldTop) * zoom;
        const int py = static_cast<int>(std::floor(sy));
        const bool isMajor = std::fmod(std::abs(wy), major) < 0.01f;
        const std::uint8_t c = static_cast<std::uint8_t>(isMajor ? 66 : 36);
        fb.FillRect(x0, py, w, 1, c, c, static_cast<std::uint8_t>(c + 12), 255);
    }
}

void DrawEntity(
    Software2DContext & fb, Rectf const & outRect, Camera2D const & cam, float wx, float wy, int halfSize, std::uint8_t r, std::uint8_t g,
    std::uint8_t b) noexcept
{
    const float zoom = std::max(0.0001f, cam.Zoom());
    const float worldViewW = outRect.w / zoom;
    const float worldViewH = outRect.z / zoom;
    const float worldLeft = cam.FocusX() - worldViewW * 0.5f;
    const float worldTop = cam.FocusY() - worldViewH * 0.5f;
    const float sx = outRect.x + (wx - worldLeft) * zoom;
    const float sy = outRect.y + (wy - worldTop) * zoom;
    const int size = std::max(2, static_cast<int>(std::round(static_cast<float>(halfSize * 2) * zoom)));
    const int px = static_cast<int>(std::round(sx - static_cast<float>(size) * 0.5f));
    const int py = static_cast<int>(std::round(sy - static_cast<float>(size) * 0.5f));
    fb.FillRect(px, py, size, size, r, g, b, 255);
}

void DrawOrb(Software2DContext & fb, Rectf const & outRect, Camera2D const & cam, Orb const & orb) noexcept
{
    const float zoom = std::max(0.0001f, cam.Zoom());
    const float worldViewW = outRect.w / zoom;
    const float worldViewH = outRect.z / zoom;
    const float worldLeft = cam.FocusX() - worldViewW * 0.5f;
    const float worldTop = cam.FocusY() - worldViewH * 0.5f;
    const float sx = outRect.x + (orb.x - worldLeft) * zoom;
    const float sy = outRect.y + (orb.y - worldTop) * zoom;
    const int radiusPx = std::max(2, static_cast<int>(std::round(orb.radius * zoom)));
    DrawCircle(fb, static_cast<int>(std::round(sx)), static_cast<int>(std::round(sy)), radiusPx, 250, 206, 72);
}

void DrawViewBorder(Software2DContext & fb, Rectf const & outRect, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    const int x = static_cast<int>(outRect.x);
    const int y = static_cast<int>(outRect.y);
    const int w = static_cast<int>(outRect.w);
    const int h = static_cast<int>(outRect.z);
    fb.FillRect(x, y, w, 2, r, g, b, 255);
    fb.FillRect(x, y + h - 2, w, 2, r, g, b, 255);
    fb.FillRect(x, y, 2, h, r, g, b, 255);
    fb.FillRect(x + w - 2, y, 2, h, r, g, b, 255);
}

void DrawHud(Software2DContext & fb, Player const & p1, Player const & p2, int fbw) noexcept
{
    fb.FillRect(0, 0, fbw, 26, 20, 24, 34, 255);
    const int maxScoreBar = std::max(8, fbw / 8);
    const int p1Bar = std::min(maxScoreBar, p1.score * 18);
    const int p2Bar = std::min(maxScoreBar, p2.score * 18);
    fb.FillRect(8, 6, p1Bar, 14, p1.r, p1.g, p1.b, 255);
    fb.FillRect(fbw - 8 - p2Bar, 6, p2Bar, 14, p2.r, p2.g, p2.b, 255);
    fb.FillRect((fbw / 2) - 1, 0, 2, 26, 50, 56, 74, 255);
}

void HandleEvents(ApplicationContextSettings & ctx, bool & running, int & fbw, int & fbh, InputState & in)
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
                in.w = down;
                break;
            case KeyCode::A:
                in.a = down;
                break;
            case KeyCode::S:
                in.s = down;
                break;
            case KeyCode::D:
                in.d = down;
                break;
            case KeyCode::UP:
                in.up = down;
                break;
            case KeyCode::LEFT:
                in.left = down;
                break;
            case KeyCode::DOWN:
                in.down = down;
                break;
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

void UpdatePlayer(Player & p, bool up, bool down, bool left, bool right, float dt) noexcept
{
    constexpr float accel = 950.f;
    constexpr float maxSpeed = 360.f;
    constexpr float damping = 0.90f;

    float ax = 0.f;
    float ay = 0.f;
    if (left)
        ax -= accel;
    if (right)
        ax += accel;
    if (up)
        ay -= accel;
    if (down)
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

void CollectOrbs(Player & p1, Player & p2, std::vector<Orb> & orbs, std::uint64_t ticks)
{
    auto collides = [](Player const & p, Orb const & o) {
        const float dx = p.x - o.x;
        const float dy = p.y - o.y;
        const float rr = static_cast<float>(kPlayerSize) + o.radius;
        return (dx * dx + dy * dy) <= rr * rr;
    };

    for (Orb & o : orbs) {
        if (!o.alive)
            continue;
        if (collides(p1, o)) {
            o.alive = false;
            ++p1.score;
        } else if (collides(p2, o)) {
            o.alive = false;
            ++p2.score;
        }
    }

    int aliveCount = 0;
    for (Orb const & o : orbs) {
        if (o.alive)
            ++aliveCount;
    }
    if (aliveCount >= 18)
        return;

    const int needed = 18 - aliveCount;
    for (int i = 0; i < needed; ++i) {
        const float t = static_cast<float>((ticks + static_cast<std::uint64_t>(i) * 123u) % 4096u);
        Orb o{};
        o.x = 80.f + std::fmod(t * 11.3f, kWorldW - 160.f);
        o.y = 80.f + std::fmod(t * 7.1f + 150.f, kWorldH - 160.f);
        o.radius = 9.f + std::fmod(t, 5.f);
        o.alive = true;
        orbs.push_back(o);
    }
}

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL - SplitScreen";
    settings.client_rect.x = 70;
    settings.client_rect.y = 70;
    settings.client_rect.w = 1200;
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

    Camera2D base{};
    base.EncapsulateFramebuffer(ctx);
    base.SetZoom(1.05f);
    std::vector<Camera2D> cameras = Camera2D::BuildSplitScreen(2u, base);

    Player p1{320.f, 300.f, 0.f, 0.f, 90, 170, 255, 0};
    Player p2{kWorldW - 320.f, kWorldH - 300.f, 0.f, 0.f, 255, 120, 100, 0};
    InputState input{};
    std::vector<Orb> orbs{};
    CollectOrbs(p1, p2, orbs, 1);

    bool running = true;
    backend.SetTargetFrameRate(60);
    std::uint64_t prevTick = backend.GetTicks();
    char title[180]{};

    while (running) {
        HandleEvents(ctx, running, fbw, fbh, input);
        if (frame.Width() != fbw || frame.Height() != fbh)
            (void)frame.Resize(fbw, fbh);

        const std::uint64_t nowTick = backend.GetTicks();
        const std::uint64_t deltaTick = nowTick >= prevTick ? nowTick - prevTick : 0;
        const float dt = std::min(0.05f, static_cast<float>(deltaTick) / 60.f);
        prevTick = nowTick;

        UpdatePlayer(p1, input.w, input.s, input.a, input.d, dt);
        UpdatePlayer(p2, input.up, input.down, input.left, input.right, dt);
        CollectOrbs(p1, p2, orbs, nowTick);

        cameras[0].EncapsulateFramebuffer(ctx);
        cameras[1].EncapsulateFramebuffer(ctx);
        cameras[0].SetFocus(p1.x, p1.y);
        cameras[1].SetFocus(p2.x, p2.y);

        frame.Clear(9, 11, 16, 255);
        for (std::size_t i = 0; i < cameras.size(); ++i) {
            Rectf outRect = ComputeOutputRect(cameras[i], fbw, fbh);
            cameras[i].MapViewportToRect(outRect);
            DrawWorldGrid(frame, outRect, cameras[i]);
            for (Orb const & o : orbs) {
                if (o.alive)
                    DrawOrb(frame, outRect, cameras[i], o);
            }
            DrawEntity(frame, outRect, cameras[i], p1.x, p1.y, kPlayerSize, p1.r, p1.g, p1.b);
            DrawEntity(frame, outRect, cameras[i], p2.x, p2.y, kPlayerSize, p2.r, p2.g, p2.b);
            if (i == 0)
                DrawViewBorder(frame, outRect, p1.r, p1.g, p1.b);
            else
                DrawViewBorder(frame, outRect, p2.r, p2.g, p2.b);
        }
        DrawHud(frame, p1, p2, fbw);

        std::snprintf(title, sizeof title, "HonkordGL - SplitScreen | P1:%d  P2:%d  (WASD + Arrows, ESC to quit)", p1.score, p2.score);
        backend.SetWindowTitle(ctx, title);

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}