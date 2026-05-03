/**
 * Dear ImGui sample using HonkordGL window events + `GpuRenderer` (OpenGL family).
 *
 * Run from repo root: `make works/ImGuiDemo/ImGuiDemo` then `works/ImGuiDemo/ImGuiDemo`.
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include "imgui.h"
#include "imgui_impl_honkordgl.h"

#include <algorithm>
#include <cmath>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::WindowBackend;

namespace {

void ApplyHonkordDemoStyle()
{
    ImGuiStyle & style = ImGui::GetStyle();
    style.WindowRounding = 10.f;
    style.ChildRounding = 8.f;
    style.FrameRounding = 8.f;
    style.PopupRounding = 8.f;
    style.ScrollbarRounding = 8.f;
    style.GrabRounding = 6.f;
    style.TabRounding = 6.f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowPadding = ImVec2(16.f, 14.f);
    style.ItemSpacing = ImVec2(10.f, 8.f);
    style.FramePadding = ImVec2(10.f, 6.f);

    ImVec4 * const c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.14f, 0.97f);
    c[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.08f, 0.11f, 1.f);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.22f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.22f, 0.30f, 1.f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.28f, 0.38f, 1.f);
    c[ImGuiCol_Button] = ImVec4(0.23f, 0.38f, 0.72f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.50f, 0.88f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.30f, 0.58f, 1.f);
    c[ImGuiCol_Header] = ImVec4(0.22f, 0.35f, 0.58f, 0.55f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.42f, 0.68f, 0.80f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.40f, 0.65f, 1.f);
    c[ImGuiCol_CheckMark] = ImVec4(0.55f, 0.78f, 1.f, 1.f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.45f, 0.65f, 1.f, 1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.82f, 1.f, 1.f);
    c[ImGuiCol_Separator] = ImVec4(0.35f, 0.40f, 0.52f, 0.55f);
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.96f, 1.f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.58f, 1.f);
    c[ImGuiCol_Border] = ImVec4(0.28f, 0.32f, 0.42f, 0.65f);
}

/** Custom showcase window (replaces `ShowDemoWindow` for a smaller, stylized sample). */
void DrawHonkordCustomDemo(bool * p_open, float * clear_phase, bool * animate_clear)
{
    ImGuiIO & io = ImGui::GetIO();
    ImGuiViewport const * const vp = ImGui::GetMainViewport();
    ImVec2 const center = vp->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.f, 560.f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("HonkordGL · Custom demo", p_open))
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Sample widgets using the HonkordGL ImGui backend.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("FPS (approx): %.1f", io.Framerate);
        ImGui::Text("Frame time:    %.3f ms", static_cast<double>(io.DeltaTime) * 1000.0);
    }

    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Animate window background", animate_clear);
        if (*animate_clear && ImGui::IsItemHovered())
            ImGui::SetTooltip("Modulates the GPU clear color each frame.");
        ImGui::Spacing();
        static float accent_rgb[3] = {0.23f, 0.38f, 0.72f};
        if (ImGui::ColorEdit3("Accent (buttons)", accent_rgb)) {
            ImGuiStyle & st = ImGui::GetStyle();
            ImVec4 const base(accent_rgb[0], accent_rgb[1], accent_rgb[2], 1.f);
            st.Colors[ImGuiCol_Button] = base;
            st.Colors[ImGuiCol_ButtonHovered] = ImVec4(
                std::min(accent_rgb[0] * 1.25f, 1.f),
                std::min(accent_rgb[1] * 1.22f, 1.f),
                std::min(accent_rgb[2] * 1.18f, 1.f),
                1.f);
            st.Colors[ImGuiCol_ButtonActive] = ImVec4(
                accent_rgb[0] * 0.75f,
                accent_rgb[1] * 0.78f,
                accent_rgb[2] * 0.82f,
                1.f);
        }
    }

    if (ImGui::CollapsingHeader("Widgets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static float blend = 0.42f;
        ImGui::SliderFloat("Slider", &blend, 0.f, 1.f, "%.2f");
        ImGui::ProgressBar(blend, ImVec2(-FLT_MIN, 0.f));

        static bool show_lines = true;
        ImGui::Checkbox("Show bullet lines", &show_lines);
        if (show_lines)
        {
            ImGui::BulletText("GpuRenderer + indexed ImDrawList geometry");
            ImGui::BulletText("ImTextureData font atlas uploads");
            ImGui::BulletText("HonkordGL Events → ImGui IO");
        }

        static char text_buf[160] = "Hello from HonkordGL";
        ImGui::InputText("Input", text_buf, sizeof(text_buf));

        static int combo = 0;
        char const * const flavors[] = {"Linux GLX / EGL", "Wayland EGL", "Windows WGL"};
        ImGui::Combo("Platform hint", &combo, flavors, IM_ARRAYSIZE(flavors));

        ImGui::Separator();
        static int clicks = 0;
        if (ImGui::Button("Click"))
            ++clicks;
        ImGui::SameLine();
        ImGui::Text("count = %d", clicks);
    }

    if (ImGui::CollapsingHeader("Plot (simple)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float samples[32]{};
        float const phase = *clear_phase * 3.f;
        for (int i = 0; i < 32; ++i)
            samples[i] = 0.5f + 0.45f * std::sin(phase + static_cast<float>(i) * 0.35f);
        ImGui::PlotLines(
            "##wave",
            samples,
            32,
            0,
            nullptr,
            -1.f,
            1.f,
            ImVec2(-FLT_MIN, 96.f));
    }

    ImGui::End();

    if (*animate_clear)
        *clear_phase += io.DeltaTime * 0.55f;
}

} // namespace

int main()
{
    WindowBackend backend{};
    ApplicationContextSettings ctx{};
    ctx.window_title = "HonkordGL — Custom ImGui demo";
    ctx.client_rect.x = 120;
    ctx.client_rect.y = 120;
    ctx.client_rect.w = 1280;
    ctx.client_rect.z = 720;

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
    const int attachRc = gpu.CreateBestAvailableShaderBackend();
    if (attachRc != static_cast<int>(RendererContextResult::OK)) {
        std::cerr << "GpuRenderer::CreateBestAvailableShaderBackend failed: " << attachRc << '\n';
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    gpu.MakeCurrent();
    gpu.SetDefaultViewport();
    std::cout << "GPU backend: " << gpu.ActiveBackendLabel() << '\n';

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyHonkordDemoStyle();

    if (!ImGui_ImplHonkordGL_Init(&ctx, &gpu)) {
        std::cerr << "ImGui_ImplHonkordGL_Init failed (need OpenGL/EGL shader backend with RGBA texture upload).\n";
        ImGui::DestroyContext();
        gpu.Destroy();
        backend.DestroyWindow(ctx);
        backend.Shutdown();
        return 1;
    }

    bool running = true;
    bool show_custom = true;
    bool animate_clear = false;
    float clear_phase = 0.f;

    while (running) {
        HonkordGL::Events::Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            ImGui_ImplHonkordGL_ProcessEvent(&ctx, ev);
            if (ev.type == HonkordGL::Events::EventType::QUIT)
                running = false;
            if (ev.type == HonkordGL::Events::EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                gpu.MakeCurrent();
                gpu.SetDefaultViewport();
            }
        }

        ImGui_ImplHonkordGL_NewFrame(&ctx);
        ImGui::NewFrame();

        if (show_custom)
            DrawHonkordCustomDemo(&show_custom, &clear_phase, &animate_clear);

        ImGui::Render();

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();

        float cr = 0.11f;
        float cg = 0.13f;
        float cb = 0.18f;
        if (animate_clear) {
            float const t = 0.5f + 0.5f * std::sin(clear_phase);
            cr = 0.06f + 0.07f * t;
            cg = 0.08f + 0.11f * (1.f - t);
            cb = 0.14f + 0.12f * t;
        }
        gpu.ClearColorBuffer(cr, cg, cb, 1.f);

        ImDrawData * draw_data = ImGui::GetDrawData();
        if (draw_data)
            ImGui_ImplHonkordGL_RenderDrawData(draw_data, &gpu);

        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    ImGui_ImplHonkordGL_Shutdown();
    ImGui::DestroyContext();

    gpu.Destroy();
    backend.DestroyWindow(ctx);
    backend.Shutdown();
    return 0;
}
