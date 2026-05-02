/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Minimal Asteroids-style game: green triangular ship, red circular asteroids.
 *
 * Example build (Linux + X11/Wayland, from repo root):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     works/AsteroidGame/Main.cpp \
 *     src/Video.cpp src/SoftwareRenderer.cpp \
 *     src/WindowBackend.cpp \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp src/X11/CursorX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/X11/GLXRendererContext.cpp src/X11/EGLRendererContextX11.cpp \
 *     src/Wayland/EGLRendererContextWayland.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/GpuRenderer.cpp src/GpuShaderCompiler.cpp src/Sprite.cpp \
 *     works/AsteroidGame/VulkanNoop.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o works/AsteroidGame/AsteroidGame \
 *     -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl -lEGL -lGL
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/Sprite.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::NativePlatform;
using HonkordGL::Graphics::Sprite;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr float kPi = 3.14159265f;
constexpr float kShipNoseLen = 20.f;
constexpr float kShipHalfBase = 14.f;
constexpr float kShipBaseY = 12.f;
/** Keep ship hull inside the client rect (approx. max extent from center). */
constexpr float kShipMargin = 24.f;

struct Asteroid {
    float x{};
    float y{};
    float vx{};
    float vy{};
    float radius{};
};

struct Bullet {
    float x{};
    float y{};
    float vx{};
    float vy{};
    bool alive{true};
};

void clampShipToBounds(float & x, float & y, float & vx, float & vy, float fw, float fh) noexcept
{
    if (fw <= kShipMargin * 2.f || fh <= kShipMargin * 2.f)
        return;
    const float xMin = kShipMargin;
    const float xMax = fw - kShipMargin;
    const float yMin = kShipMargin;
    const float yMax = fh - kShipMargin;
    if (x < xMin) {
        x = xMin;
        if (vx < 0.f)
            vx = 0.f;
    } else if (x > xMax) {
        x = xMax;
        if (vx > 0.f)
            vx = 0.f;
    }
    if (y < yMin) {
        y = yMin;
        if (vy < 0.f)
            vy = 0.f;
    } else if (y > yMax) {
        y = yMax;
        if (vy > 0.f)
            vy = 0.f;
    }
}

void spawnWave(std::vector<Asteroid> & out, float fw, float fh, int count, std::mt19937 & rng)
{
    const float pad = std::min(48.f, std::min(fw, fh) * 0.22f);
    const float xLo = pad;
    const float xHi = std::max(pad + 1.f, fw - pad);
    const float yLo = pad;
    const float yHi = std::max(pad + 1.f, fh - pad);
    std::uniform_real_distribution<float> ux(xLo, xHi);
    std::uniform_real_distribution<float> uy(yLo, yHi);
    std::uniform_real_distribution<float> uv(-90.f, 90.f);
    std::uniform_real_distribution<float> ur(18.f, 38.f);
    out.clear();
    for (int i = 0; i < count; ++i) {
        Asteroid a{};
        a.x = ux(rng);
        a.y = uy(rng);
        const float sp = 40.f + static_cast<float>(i % 5) * 15.f;
        const float ang = uv(rng) * (kPi / 180.f);
        a.vx = std::cos(ang) * sp;
        a.vy = std::sin(ang) * sp;
        a.radius = ur(rng);
        out.push_back(a);
    }
}

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL — AsteroidGame";
    settings.client_rect.x = 80;
    settings.client_rect.y = 80;
    settings.client_rect.w = 1024;
    settings.client_rect.z = 768;

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

    backend.SetTargetFrameRate(60);

    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM) {
        std::cerr << "AsteroidGame requires OpenGL (not DRM-only).\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    GpuRenderer gpu(ctx);
    if (gpu.CreateBestAvailableShaderBackend() != 0) {
        std::cerr << "CreateBestAvailableShaderBackend failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    Sprite canvas{};
    {
        std::uint8_t px[16] = {
            255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        };
        if (!canvas.CreateFromRGBA(ctx, 2, 2, px, 0)) {
            std::cerr << "CreateFromRGBA failed.\n";
            gpu.Destroy();
            window->TerminateWindow();
            backend.Shutdown();
            return 1;
        }
    }

    gpu.MakeCurrent();
    gpu.SetDefaultViewport();

    std::mt19937 rng{std::random_device{}()};

    float fw = static_cast<float>(ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w);
    float fh = static_cast<float>(ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z);

    float shipX = fw * 0.5f;
    float shipY = fh * 0.5f;
    float shipVx = 0.f;
    float shipVy = 0.f;
    float shipAng = -kPi * 0.5f;

    std::vector<Asteroid> asteroids;
    std::vector<Bullet> bullets;
    int wave = 1;
    int score = 0;
    spawnWave(asteroids, fw, fh, 4 + wave, rng);

    bool keyLeft = false;
    bool keyRight = false;
    bool keyThrust = false;
    bool keyFire = false;
    bool fireHeld = false;

    bool running = true;
    std::uint64_t prevTick = backend.GetTicks();

    while (running) {
        HonkordGL::Events::Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            using HonkordGL::Events::EventType;
            using K = HonkordGL::Events::KeyCode;
            if (ev.type == EventType::QUIT)
                running = false;
            if (ev.type == EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                fw = static_cast<float>(ev.width);
                fh = static_cast<float>(ev.height);
                gpu.MakeCurrent();
                gpu.SetDefaultViewport();
                clampShipToBounds(shipX, shipY, shipVx, shipVy, fw, fh);
            }
            if (ev.type == EventType::KEY_PRESS) {
                if (ev.key == K::LEFT)
                    keyLeft = true;
                if (ev.key == K::RIGHT)
                    keyRight = true;
                if (ev.key == K::UP)
                    keyThrust = true;
                if (ev.key == K::SPACE)
                    keyFire = true;
            }
            if (ev.type == EventType::KEY_RELEASE) {
                if (ev.key == K::LEFT)
                    keyLeft = false;
                if (ev.key == K::RIGHT)
                    keyRight = false;
                if (ev.key == K::UP)
                    keyThrust = false;
                if (ev.key == K::SPACE)
                    keyFire = false;
            }
        }

        const std::uint64_t nowTick = backend.GetTicks();
        const std::uint64_t tickDelta = nowTick >= prevTick ? (nowTick - prevTick) : 0;
        const float dt = std::min(0.05f, static_cast<float>(tickDelta) / 60.f);
        prevTick = nowTick;

        const float turn = 3.2f * dt;
        if (keyLeft)
            shipAng -= turn;
        if (keyRight)
            shipAng += turn;

        const float thrust = 320.f * dt;
        if (keyThrust) {
            shipVx += std::cos(shipAng) * thrust;
            shipVy += std::sin(shipAng) * thrust;
        }
        const float drag = std::pow(0.985f, dt * 60.f);
        shipVx *= drag;
        shipVy *= drag;

        shipX += shipVx * dt;
        shipY += shipVy * dt;
        clampShipToBounds(shipX, shipY, shipVx, shipVy, fw, fh);

        if (keyFire && !fireHeld) {
            fireHeld = true;
            constexpr float bulletSpeed = 520.f;
            Bullet b{};
            b.x = shipX + std::cos(shipAng) * (kShipNoseLen + 4.f);
            b.y = shipY + std::sin(shipAng) * (kShipNoseLen + 4.f);
            b.vx = std::cos(shipAng) * bulletSpeed + shipVx * 0.3f;
            b.vy = std::sin(shipAng) * bulletSpeed + shipVy * 0.3f;
            bullets.push_back(b);
        }
        if (!keyFire)
            fireHeld = false;

        for (Bullet & b : bullets) {
            if (!b.alive)
                continue;
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            if (b.x < 0.f || b.x > fw || b.y < 0.f || b.y > fh)
                b.alive = false;
        }

        for (Asteroid & a : asteroids) {
            a.x += a.vx * dt;
            a.y += a.vy * dt;
            const float r = a.radius;
            if (a.x < r) {
                a.x = r;
                a.vx = -a.vx;
            } else if (a.x > fw - r) {
                a.x = fw - r;
                a.vx = -a.vx;
            }
            if (a.y < r) {
                a.y = r;
                a.vy = -a.vy;
            } else if (a.y > fh - r) {
                a.y = fh - r;
                a.vy = -a.vy;
            }
        }

        for (Bullet & b : bullets) {
            if (!b.alive)
                continue;
            for (std::size_t ai = 0; ai < asteroids.size(); ++ai) {
                Asteroid & a = asteroids[ai];
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                if (dx * dx + dy * dy <= (a.radius + 3.f) * (a.radius + 3.f)) {
                    b.alive = false;
                    score += 10;
                    const float nr = a.radius * 0.55f;
                    if (nr > 10.f) {
                        std::uniform_real_distribution<float> uang(0.f, kPi * 2.f);
                        for (int k = 0; k < 2; ++k) {
                            Asteroid child{};
                            child.x = a.x;
                            child.y = a.y;
                            const float ang = uang(rng);
                            const float sp = 70.f + static_cast<float>(k) * 20.f;
                            child.vx = std::cos(ang) * sp;
                            child.vy = std::sin(ang) * sp;
                            child.radius = nr;
                            asteroids.push_back(child);
                        }
                    }
                    asteroids[ai] = asteroids.back();
                    asteroids.pop_back();
                    break;
                }
            }
        }

        bullets.erase(
            std::remove_if(
                bullets.begin(),
                bullets.end(),
                [](Bullet const & x) { return !x.alive; }),
            bullets.end());

        for (Asteroid const & a : asteroids) {
            const float dx = shipX - a.x;
            const float dy = shipY - a.y;
            if (dx * dx + dy * dy <= (a.radius + 10.f) * (a.radius + 10.f)) {
                score = std::max(0, score - 50);
                shipVx = 0.f;
                shipVy = 0.f;
                shipX = fw * 0.5f;
                shipY = fh * 0.5f;
                clampShipToBounds(shipX, shipY, shipVx, shipVy, fw, fh);
            }
        }

        if (asteroids.empty()) {
            ++wave;
            spawnWave(asteroids, fw, fh, 4 + wave * 2, rng);
        }

        {
            std::string title = "HonkordGL — Asteroids | score " + std::to_string(score) + " | wave " + std::to_string(wave);
            backend.SetWindowTitle(ctx, title.c_str());
        }

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.ClearColorBuffer(0.02f, 0.025f, 0.06f, 1.f);

        const float c = std::cos(shipAng);
        const float s = std::sin(shipAng);
        auto const xf = [&](float lx, float ly) {
            const float rx = lx * c - ly * s;
            const float ry = lx * s + ly * c;
            return std::pair<float, float>(shipX + rx, shipY + ry);
        };
        const auto p0 = xf(0.f, -kShipNoseLen);
        const auto p1 = xf(-kShipHalfBase, kShipBaseY);
        const auto p2 = xf(kShipHalfBase, kShipBaseY);

        canvas.BeginShape();
        for (Asteroid const & a : asteroids) {
            canvas.Circle(a.x, a.y, a.radius, 0.92f, 0.18f, 0.14f, 1.f, 0.35f, 0.05f, 0.05f, 1.f, 2.5f);
        }
        for (Bullet const & b : bullets) {
            canvas.Circle(b.x, b.y, 3.f, 1.f, 0.95f, 0.35f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f);
        }
        canvas.TriangleFill(p0.first, p0.second, p1.first, p1.second, p2.first, p2.second, 0.15f, 0.85f, 0.35f, 1.f);
        canvas.EndShape(ctx);

        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    gpu.Destroy();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}
