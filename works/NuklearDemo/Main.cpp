/**
 * Nuklear sample using HonkordGL window events + OpenGL-family `GpuRenderer`.
 *
 * Build: `make works/NuklearDemo/NuklearDemo`
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include "nuklear_honkord_defs.h"
#include "nuklear.h"

#include "nuklear_impl_honkord_input.h"
#include "nuklear_impl_honkordgl.h"

#include <cmath>
#include <iostream>
#include <string>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::WindowBackend;

int main()
{
    WindowBackend backend{};
    ApplicationContextSettings ctx{};
    ctx.window_title = "HonkordGL — Nuklear (GPU)";
    ctx.client_rect.x = 120;
    ctx.client_rect.y = 120;
    ctx.client_rect.w = 960;
    ctx.client_rect.z = 540;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "WindowBackend::Initialize failed.\n";
        return 1;
    }

    if (!backend.CreateWindow(ctx)) {
        std::cerr << "CreateWindow failed.\n";
        backend.Shutdown();
        return 1;
    }

    GpuRenderer gpu(ctx);
    if (gpu.CreateBestAvailableShaderBackend() != static_cast<int>(RendererContextResult::OK)) {
        std::cerr << "GpuRenderer::CreateBestAvailableShaderBackend failed.\n";
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    gpu.MakeCurrent();
    gpu.SetDefaultViewport();
    std::cout << "GPU backend: " << gpu.ActiveBackendLabel() << '\n';

    nk_context nk{};
    if (!Nuklear_ImplHonkordGL_Init(&ctx, &gpu, &nk, 15.f)) {
        std::cerr << "Nuklear_ImplHonkordGL_Init failed (need OpenGL/EGL with RGBA texture upload).\n";
        gpu.Destroy();
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    backend.SetTargetFrameRate(60);

    enum { EASY = 0, HARD = 1 };
    static int op = EASY;
    static float value = 22.f;
    static int cnt = 0;
    struct nk_colorf bg{};
    bg.r = 0.10f;
    bg.g = 0.12f;
    bg.b = 0.17f;
    bg.a = 1.f;

    bool running = true;
    while (running) {
        HonkordGL::Events::Event ev{};
        Nuklear_ImplHonkord_InputBegin(&nk);
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            Nuklear_ImplHonkord_ProcessEvent(&ctx, &nk, ev);
            if (ev.type == HonkordGL::Events::EventType::QUIT)
                running = false;
            if (ev.type == HonkordGL::Events::EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                gpu.MakeCurrent();
                gpu.SetDefaultViewport();
            }
        }
        Nuklear_ImplHonkord_InputEnd(&nk);

        nk_clear(&nk);

        if (nk_begin(&nk, "HonkordGL · Nuklear", nk_rect(50, 50, 320, 420),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE
                    | NK_WINDOW_TITLE))
        {
            nk_layout_row_static(&nk, 28, 120, 2);
            if (nk_button_label(&nk, "button"))
                ++cnt;
            nk_label(&nk, "counter:", NK_TEXT_LEFT);
            nk_label(&nk, std::to_string(cnt).c_str(), NK_TEXT_LEFT);

            nk_layout_row_dynamic(&nk, 28, 2);
            if (nk_option_label(&nk, "easy", op == EASY))
                op = EASY;
            if (nk_option_label(&nk, "hard", op == HARD))
                op = HARD;

            nk_layout_row_dynamic(&nk, 28, 1);
            nk_labelf(&nk, NK_TEXT_LEFT, "slider: %.2f", value);
            nk_slider_float(&nk, 0.f, &value, 100.f, 1.f);

            nk_layout_row_dynamic(&nk, 120, 1);
            nk_chart_begin(&nk, NK_CHART_LINES, 32, 0.f, 100.f);
            static float t = 0.f;
            t += 0.05f;
            for (int i = 0; i < 32; ++i)
                nk_chart_push(&nk, 50.f + 45.f * std::sin(t + static_cast<float>(i) * 0.22f));
            nk_chart_end(&nk);

            nk_layout_row_dynamic(&nk, 28, 1);
            nk_property_float(&nk, "BG R:", 0.f, &bg.r, 1.f, 0.02f, 0.02f);
            nk_layout_row_dynamic(&nk, 28, 1);
            nk_property_float(&nk, "BG G:", 0.f, &bg.g, 1.f, 0.02f, 0.02f);
            nk_layout_row_dynamic(&nk, 28, 1);
            nk_property_float(&nk, "BG B:", 0.f, &bg.b, 1.f, 0.02f, 0.02f);
        }
        nk_end(&nk);

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.ClearColorBuffer(bg.r, bg.g, bg.b, 1.f);

        Nuklear_ImplHonkordGL_Render(&ctx, &nk, &gpu);

        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    Nuklear_ImplHonkordGL_Shutdown(&nk);
    gpu.Destroy();
    backend.DestroyWindow(ctx);
    backend.Shutdown();
    return 0;
}
