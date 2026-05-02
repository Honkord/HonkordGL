/**
 * Tetris (CPU software framebuffer only — no GPU). HonkordGL `Software2DContext` + `WindowBackend`.
 *
 * Controls: Left/Right move, Up rotate, Down soft drop, Space hard drop, P pause, R restart.
 *
 * Example build (Linux + X11/Wayland, from repo root):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -DHONKORDGL_TEXTUI_GPU=0 -c -o /tmp/TextUI_cpu.o src/TextUI.cpp
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     works/Tetris/Main.cpp works/MovingSquare/StbImageStub.cpp \
 *     src/Video.cpp src/WindowBackend.cpp src/SoftwareRenderer.cpp \
 *     /tmp/TextUI_cpu.o \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp src/X11/CursorX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o works/Tetris/Tetris \
 *     -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/Audio.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/TextUI.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Events::KeyCode;
using HonkordGL::Audio::AudioMixer;
using HonkordGL::Audio::AudioStream;
using HonkordGL::Audio::AudioTrack;
using HonkordGL::Audio::CreateAudioMixerDevice;
using HonkordGL::Audio::InitializeAudioSubsystem;
using HonkordGL::Audio::ObliterateAudioMixer;
using HonkordGL::Audio::ShutdownAudioSubsystem;
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

constexpr int kCols = 10;
constexpr int kRows = 20;
constexpr int kHudH = 40;

// Cell colors (R,G,B) per piece type 0..6 (I,O,T,S,Z,J,L)
constexpr std::uint8_t kColors[7][3] = {
    {0, 206, 209},  // I cyan
    {238, 234, 83}, // O yellow
    {160, 32, 240}, // T purple
    {50, 205, 50},  // S green
    {220, 20, 60},  // Z red
    {30, 144, 255}, // J blue
    {255, 140, 0},  // L orange
};

/** Four minos per (piece, rotation): [piece][rot][mino][x/y]. */
static const int kMino[7][4][4][2] = {
    // I
    {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
    },
    // O (all rotations identical)
    {
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    },
    // T
    {
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    // S
    {
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    // Z
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
    },
    // J
    {
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {0, 2}, {1, 2}},
    },
    // L
    {
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
    },
};

struct Piece {
    int type{0};
    int rot{0};
    int x{3};
    int y{0};
};

struct Game {
    std::array<std::array<int, kCols>, kRows> board{};
    Piece cur{};
    int nextType{0};
    int score{0};
    int lines{0};
    int level{1};
    bool over{false};
    bool paused{false};
    std::mt19937 rng{std::random_device{}()};
    std::uint64_t lastGravityTick{0};

    Game()
    {
        nextType = static_cast<int>(rng() % 7);
        spawn();
    }

    void spawn()
    {
        cur.type = nextType;
        nextType = static_cast<int>(rng() % 7);
        cur.rot = 0;
        cur.x = 3;
        cur.y = 0;
        if (collides(cur))
            over = true;
    }

    bool collides(Piece const & p) const noexcept
    {
        for (int i = 0; i < 4; ++i) {
            const int lx = kMino[p.type][p.rot][i][0];
            const int ly = kMino[p.type][p.rot][i][1];
            const int bx = p.x + lx;
            const int by = p.y + ly;
            if (bx < 0 || bx >= kCols || by >= kRows)
                return true;
            if (by >= 0 && board[by][bx] != 0)
                return true;
        }
        return false;
    }

    void lock()
    {
        for (int i = 0; i < 4; ++i) {
            const int lx = kMino[cur.type][cur.rot][i][0];
            const int ly = kMino[cur.type][cur.rot][i][1];
            const int bx = cur.x + lx;
            const int by = cur.y + ly;
            if (by >= 0 && by < kRows && bx >= 0 && bx < kCols)
                board[by][bx] = cur.type + 1;
        }
        clearLines();
        spawn();
    }

    void clearLines()
    {
        int cleared = 0;
        for (int y = kRows - 1; y >= 0;) {
            bool full = true;
            for (int x = 0; x < kCols; ++x) {
                if (board[y][x] == 0) {
                    full = false;
                    break;
                }
            }
            if (full) {
                ++cleared;
                for (int yy = y; yy > 0; --yy)
                    board[yy] = board[yy - 1];
                board[0].fill(0);
            } else {
                --y;
            }
        }
        if (cleared > 0) {
            lines += cleared;
            static const int pts[5] = {0, 100, 300, 500, 800};
            score += pts[cleared] * level;
            level = 1 + lines / 10;
        }
    }

    int gravityMs() const noexcept
    {
        const int base = 800;
        const int ms = base - (level - 1) * 55;
        return std::max(80, ms);
    }

    void tryMove(int dx, int dy)
    {
        if (over || paused)
            return;
        Piece n = cur;
        n.x += dx;
        n.y += dy;
        if (!collides(n))
            cur = n;
        else if (dy > 0)
            lock();
    }

    void tryRotate()
    {
        if (over || paused)
            return;
        Piece n = cur;
        n.rot = (n.rot + 1) & 3;
        if (!collides(n)) {
            cur = n;
            return;
        }
        // Simple wall kicks
        for (int kick : {-1, 1, -2, 2}) {
            n = cur;
            n.rot = (n.rot + 1) & 3;
            n.x += kick;
            if (!collides(n)) {
                cur = n;
                return;
            }
        }
    }

    void hardDrop()
    {
        if (over || paused)
            return;
        while (true) {
            Piece n = cur;
            n.y += 1;
            if (collides(n)) {
                lock();
                return;
            }
            cur = n;
            score += 2;
        }
    }

    void tickGravity(std::uint64_t nowTick, int tickRate)
    {
        if (over || paused)
            return;
        if (nowTick < lastGravityTick)
            lastGravityTick = nowTick;
        const int gravityMsValue = gravityMs();
        const std::uint64_t gravityTicks =
            std::max<std::uint64_t>(1u, static_cast<std::uint64_t>((gravityMsValue * tickRate + 999) / 1000));
        if (nowTick - lastGravityTick < gravityTicks)
            return;
        lastGravityTick = nowTick;
        tryMove(0, 1);
    }
};

void DrawCell(Software2DContext & fb, int x, int y, int cw, int ch, int typeIdx, bool ghost) noexcept
{
    if (typeIdx < 0 || typeIdx > 6)
        return;
    const std::uint8_t * const c = kColors[typeIdx];
    std::uint8_t r = c[0];
    std::uint8_t g = c[1];
    std::uint8_t b = c[2];
    if (ghost) {
        r = static_cast<std::uint8_t>(r / 3 + 20);
        g = static_cast<std::uint8_t>(g / 3 + 20);
        b = static_cast<std::uint8_t>(b / 3 + 24);
    }
    const int pad = std::max(1, cw / 12);
    fb.FillRect(x, y, cw, ch, 20, 22, 28, 255);
    fb.FillRect(x + pad, y + pad, cw - 2 * pad, ch - 2 * pad, r, g, b, 255);
}

void DrawGame(Software2DContext & fb, Game const & g, int fbw, int fbh) noexcept
{
    fb.Clear(18, 20, 28, 255);
    fb.FillRect(0, 0, fbw, kHudH, 32, 36, 46, 255);

    const int availH = fbh - kHudH - 24;
    const int availW = fbw - 24;
    const int cell = std::max(12, std::min(availW * 8 / (kCols * 10), availH / (kRows + 2)));
    const int bw = cell * kCols;
    const int bh = cell * kRows;
    const int ox = (fbw - bw) / 2;
    const int oy = kHudH + (fbh - kHudH - bh) / 2;

    fb.FillRect(ox - 6, oy - 6, bw + 12, bh + 12, 45, 48, 58, 255);

    for (int y = 0; y < kRows; ++y) {
        for (int x = 0; x < kCols; ++x) {
            const int t = g.board[y][x];
            if (t > 0)
                DrawCell(fb, ox + x * cell, oy + y * cell, cell, cell, t - 1, false);
        }
    }

    // Ghost
    if (!g.over && !g.paused) {
        Piece ghost = g.cur;
        while (true) {
            Piece n = ghost;
            n.y += 1;
            if (g.collides(n))
                break;
            ghost = n;
        }
        for (int i = 0; i < 4; ++i) {
            const int lx = kMino[ghost.type][ghost.rot][i][0];
            const int ly = kMino[ghost.type][ghost.rot][i][1];
            const int bx = ghost.x + lx;
            const int by = ghost.y + ly;
            if (by >= 0)
                DrawCell(fb, ox + bx * cell, oy + by * cell, cell, cell, ghost.type, true);
        }
    }

    // Current piece
    if (!g.over && !g.paused) {
        for (int i = 0; i < 4; ++i) {
            const int lx = kMino[g.cur.type][g.cur.rot][i][0];
            const int ly = kMino[g.cur.type][g.cur.rot][i][1];
            const int bx = g.cur.x + lx;
            const int by = g.cur.y + ly;
            if (by >= 0)
                DrawCell(fb, ox + bx * cell, oy + by * cell, cell, cell, g.cur.type, false);
        }
    }

    // Next preview (top right)
    const int pw = cell * 4;
    const int px0 = std::min(ox + bw + 16, fbw - pw - 8);
    const int py0 = oy;
    fb.FillRect(px0 - 4, py0 - 4, pw + 8, cell * 4 + 8, 40, 44, 54, 255);
    for (int i = 0; i < 4; ++i) {
        const int lx = kMino[g.nextType][0][i][0];
        const int ly = kMino[g.nextType][0][i][1];
        DrawCell(fb, px0 + lx * cell, py0 + ly * cell, cell, cell, g.nextType, false);
    }
}

} // namespace

int main()
{
    bool audioReady = false;
    AudioMixer * bgmMixer = nullptr;
    AudioTrack * bgmTrack = nullptr;
    AudioStream bgmStream{};
    auto shutdownAudio = [&]() {
        if (bgmMixer) {
            ObliterateAudioMixer(bgmMixer);
            bgmMixer = nullptr;
            bgmTrack = nullptr;
        }
        ShutdownAudioSubsystem();
    };
    if (InitializeAudioSubsystem(0u) == 0) {
        bgmMixer = CreateAudioMixerDevice("tetris_bgm", nullptr);
        if (bgmMixer) {
            bgmTrack = bgmMixer->AddTrack();
            if (bgmTrack) {
                static const char * const kMusicPaths[] = {
                    "works/Tetris/Party Sector.ogg",
                    "Party Sector.ogg",
                };
                int loadOk = -1;
                for (const char * p : kMusicPaths) {
                    loadOk = bgmStream.OpenFromFile(p);
                    if (loadOk == 0)
                        break;
                }
                if (loadOk == 0) {
                    bgmStream.SetLoop(true);
                    bgmTrack->SetVolume(0.45f);
                    if (bgmStream.Play(*bgmMixer, *bgmTrack) == 0 && bgmMixer->Play() == 0)
                        audioReady = true;
                }
            }
        }
    }
    // if (!audioReady) {
    //     std::cerr << "[Audio] Failed to play Party Sector.ogg (continuing without BGM).\n";
    //     shutdownAudio();
    // }

    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL — Tetris (software)";
    settings.client_rect.x = 100;
    settings.client_rect.y = 80;
    settings.client_rect.w = 520;
    settings.client_rect.z = 700;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed.\n";
        if (audioReady)
            shutdownAudio();
        return 1;
    }
    auto window = CreateWindowOnSharedBackend(backend, settings);
    if (!window) {
        std::cerr << "CreateWindowOnSharedBackend failed.\n";
        backend.Shutdown();
        if (audioReady)
            shutdownAudio();
        return 1;
    }
    window->CreateWindow();
    ApplicationContextSettings& ctx = window->RetrieveCurrentSettings();
    if (!ctx.window_handle) {
        std::cerr << "CreateWindow failed.\n";
        backend.Shutdown();
        if (audioReady)
            shutdownAudio();
        return 1;
    }

    Software2DContext frame{};
    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (!frame.Resize(fbw, fbh)) {
        std::cerr << "Software2DContext::Resize failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        if (audioReady)
            shutdownAudio();
        return 1;
    }

    if (TextUIBindSoftware2D(ctx, frame) != 0) {
        std::cerr << "TextUIBindSoftware2D failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        if (audioReady)
            shutdownAudio();
        return 1;
    }

    Game game;
    game.lastGravityTick = backend.GetTicks();

    bool running = true;
    backend.SetTargetFrameRate(60);

    char hud[256]{};
    char title[192]{};

    std::cout
        << "Tetris (CPU framebuffer). Arrows + Space, P pause, R restart, F5 reset window settings in-place.\n";

    while (running) {
        Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            if (ev.type == EventType::QUIT)
                running = false;
            if (ev.type == EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                fbw = ev.width;
                fbh = ev.height;
                (void)frame.Resize(fbw, fbh);
            }
            if (ev.type == EventType::KEY_PRESS) {
                if (ev.key == KeyCode::P) {
                    if (!game.over)
                        game.paused = !game.paused;
                }
                if (ev.key == KeyCode::R) {
                    game = Game{};
                    game.lastGravityTick = backend.GetTicks();
                }
                if (ev.key == KeyCode::F5) {
                    // Reset app settings while window is alive (no explicit close).
                    settings.window_title = "HonkordGL — Tetris (reset)";
                    settings.client_rect.x = 100;
                    settings.client_rect.y = 80;
                    settings.client_rect.w = 520;
                    settings.client_rect.z = 700;
                    window->ResetApplication(settings);
                    fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
                    fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
                    (void)frame.Resize(fbw, fbh);
                    (void)TextUIBindSoftware2D(ctx, frame);
                }
                if (!game.over && !game.paused) {
                    if (ev.key == KeyCode::LEFT)
                        game.tryMove(-1, 0);
                    if (ev.key == KeyCode::RIGHT)
                        game.tryMove(1, 0);
                    if (ev.key == KeyCode::DOWN)
                        game.tryMove(0, 1);
                    if (ev.key == KeyCode::UP)
                        game.tryRotate();
                    if (ev.key == KeyCode::SPACE)
                        game.hardDrop();
                }
            }
        }

        game.tickGravity(backend.GetTicks(), 60);

        DrawGame(frame, game, fbw, fbh);

        std::snprintf(
            hud,
            sizeof hud,
            "Score %d  Lines %d  Lv %d  %s",
            game.score,
            game.lines,
            game.level,
            game.over ? "GAME OVER — R" : (game.paused ? "PAUSED (P)" : "P pause"));
        TextUIBegin();
        TextUINextColor(1.f, 1.f, 1.f, 1.f);
        TextUITextSize(15.f);
        TextUITextRect(10.f, 6.f, static_cast<float>(fbw - 20), static_cast<float>(kHudH - 8));
        TextUIDraw(hud);
        TextUIEnd();

        std::snprintf(title, sizeof title, "HonkordGL Tetris | %d pts", game.score);
        backend.SetWindowTitle(ctx, title);

        (void)backend.PresentSoftwareFramebuffer(ctx, frame.Pixels(), frame.Width(), frame.Height(), frame.StrideBytes());
        backend.DelayFrame();
    }

    TextUIShutdown();
    window->TerminateWindow();
    backend.Shutdown();
    if (audioReady)
        shutdownAudio();
    return 0;
}