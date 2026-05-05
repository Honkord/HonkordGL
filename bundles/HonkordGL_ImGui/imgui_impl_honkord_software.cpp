/**
 * Dear ImGui — HonkordGL CPU `SoftwareRenderer` / `Software2DContext` backend (no GPU).
 */

#include "imgui_impl_honkord_software.h"

#ifndef IMGUI_DISABLE

#include <HonkordGL/SoftwareRenderer.h>
#include <HonkordGL/WindowApplication.h>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::Software2DContext;
using HonkordGL::Graphics::SoftwareRenderer;

namespace {

struct CpuTexture {
    int w{};
    int h{};
    std::vector<unsigned char> rgba;
};

struct ImGui_ImplHonkordSoftware_Data {
    ApplicationContextSettings * App{nullptr};
    SoftwareRenderer * Renderer{nullptr};
    std::unordered_map<ImTextureID, CpuTexture *> Textures{};
    std::chrono::steady_clock::time_point LastFrame{};
    bool TimeValid{false};
};

static ImGui_ImplHonkordSoftware_Data * GetBackendData()
{
    return ImGui::GetCurrentContext()
        ? static_cast<ImGui_ImplHonkordSoftware_Data *>(ImGui::GetIO().BackendRendererUserData)
        : nullptr;
}

static ImGuiKey KeyCodeToImGui(HonkordGL::Events::KeyCode key) noexcept
{
    using KC = HonkordGL::Events::KeyCode;
    switch (key) {
    case KC::ESCAPE:
        return ImGuiKey_Escape;
    case KC::ENTER:
        return ImGuiKey_Enter;
    case KC::TAB:
        return ImGuiKey_Tab;
    case KC::SPACE:
        return ImGuiKey_Space;
    case KC::BACKQUOTE:
        return ImGuiKey_GraveAccent;
    case KC::SHIFT:
        return ImGuiKey_LeftShift;
    case KC::CTRL:
        return ImGuiKey_LeftCtrl;
    case KC::ALT:
        return ImGuiKey_LeftAlt;
    case KC::SUPER:
    case KC::META:
        return ImGuiKey_LeftSuper;
    case KC::CAPS_LOCK:
        return ImGuiKey_CapsLock;
    case KC::NUM_LOCK:
        return ImGuiKey_NumLock;
    case KC::PRINT_SCREEN:
        return ImGuiKey_PrintScreen;
    case KC::SCROLL_LOCK:
        return ImGuiKey_ScrollLock;
    case KC::PAUSE:
        return ImGuiKey_Pause;
    case KC::INSERT:
        return ImGuiKey_Insert;
    case KC::DELETE:
        return ImGuiKey_Delete;
    case KC::HOME:
        return ImGuiKey_Home;
    case KC::END:
        return ImGuiKey_End;
    case KC::PAGE_UP:
        return ImGuiKey_PageUp;
    case KC::PAGE_DOWN:
        return ImGuiKey_PageDown;
    case KC::UP:
        return ImGuiKey_UpArrow;
    case KC::DOWN:
        return ImGuiKey_DownArrow;
    case KC::LEFT:
        return ImGuiKey_LeftArrow;
    case KC::RIGHT:
        return ImGuiKey_RightArrow;
    case KC::BRACKET_LEFT:
        return ImGuiKey_LeftBracket;
    case KC::BRACKET_RIGHT:
        return ImGuiKey_RightBracket;
    case KC::SEMICOLON:
        return ImGuiKey_Semicolon;
    case KC::COMMA:
        return ImGuiKey_Comma;
    case KC::PERIOD:
        return ImGuiKey_Period;
    case KC::SLASH:
        return ImGuiKey_Slash;
    case KC::BACKSLASH:
        return ImGuiKey_Backslash;
    case KC::TILDE:
        return ImGuiKey_None;
    case KC::EQUAL:
        return ImGuiKey_Equal;
    case KC::MINUS:
        return ImGuiKey_Minus;
    default:
        break;
    }

    if (key >= KC::A && key <= KC::Z)
        return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + (static_cast<int>(key) - static_cast<int>(KC::A)));
    if (key >= KC::NUMBER_0 && key <= KC::NUMBER_9)
        return static_cast<ImGuiKey>(
            static_cast<int>(ImGuiKey_0) + (static_cast<int>(key) - static_cast<int>(KC::NUMBER_0)));
    if (key >= KC::F1 && key <= KC::F12)
        return static_cast<ImGuiKey>(
            static_cast<int>(ImGuiKey_F1) + (static_cast<int>(key) - static_cast<int>(KC::F1)));

    return ImGuiKey_None;
}

static int NativeMouseButtonToImGui(unsigned int btn) noexcept
{
    switch (btn) {
    case 1:
        return 0;
    case 2:
        return 2;
    case 3:
        return 1;
    default:
        return -1;
    }
}

static bool ExpandAlpha8ToRgba(ImTextureData * tex, std::vector<unsigned char> * out_rgba)
{
    if (!tex || tex->Format != ImTextureFormat_Alpha8 || !tex->Pixels)
        return false;
    int const n = tex->Width * tex->Height;
    out_rgba->resize(static_cast<std::size_t>(n) * 4u);
    unsigned char const * const src = tex->Pixels;
    unsigned char * const dst = out_rgba->data();
    for (int i = 0; i < n; ++i) {
        dst[i * 4 + 0] = 255;
        dst[i * 4 + 1] = 255;
        dst[i * 4 + 2] = 255;
        dst[i * 4 + 3] = src[i];
    }
    return true;
}

static float Orient2d(ImVec2 const & a, ImVec2 const & b, ImVec2 const & c) noexcept
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void SampleRgbaLinear(CpuTexture const & tex, float u, float v, float * out_rgba) noexcept
{
    if (tex.w <= 0 || tex.h <= 0 || tex.rgba.empty()) {
        out_rgba[0] = out_rgba[1] = out_rgba[2] = 1.f;
        out_rgba[3] = 1.f;
        return;
    }
    u = std::clamp(u, 0.f, 1.f);
    v = std::clamp(v, 0.f, 1.f);
    float const fx = u * static_cast<float>(tex.w - 1 > 0 ? tex.w - 1 : 0);
    float const fy = v * static_cast<float>(tex.h - 1 > 0 ? tex.h - 1 : 0);
    int const x0 = static_cast<int>(fx);
    int const y0 = static_cast<int>(fy);
    int const x1 = std::min(x0 + 1, tex.w - 1);
    int const y1 = std::min(y0 + 1, tex.h - 1);
    float const tx = fx - static_cast<float>(x0);
    float const ty = fy - static_cast<float>(y0);

    auto px_at = [&](int x, int y) -> unsigned char const * {
        return &tex.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(tex.w)
                             + static_cast<std::size_t>(x))
                         * 4u];
    };

    unsigned char const * p00 = px_at(x0, y0);
    unsigned char const * p10 = px_at(x1, y0);
    unsigned char const * p01 = px_at(x0, y1);
    unsigned char const * p11 = px_at(x1, y1);

    auto lerp = [](float t, float a, float b) noexcept { return a + (b - a) * t; };
    for (int c = 0; c < 4; ++c) {
        float const a = lerp(tx, static_cast<float>(p00[c]), static_cast<float>(p10[c])) / 255.f;
        float const b = lerp(tx, static_cast<float>(p01[c]), static_cast<float>(p11[c])) / 255.f;
        out_rgba[c] = lerp(ty, a, b);
    }
}

static void BlendStraightOver(std::uint8_t * dst, float Sr, float Sg, float Sb, float Sa) noexcept
{
    float const Dr = static_cast<float>(dst[0]) / 255.f;
    float const Dg = static_cast<float>(dst[1]) / 255.f;
    float const Db = static_cast<float>(dst[2]) / 255.f;
    float const Da = static_cast<float>(dst[3]) / 255.f;

    float const Co_r = Sr * Sa + Dr * Da * (1.f - Sa);
    float const Co_g = Sg * Sa + Dg * Da * (1.f - Sa);
    float const Co_b = Sb * Sa + Db * Da * (1.f - Sa);
    float const Ao = Sa + Da * (1.f - Sa);
    if (Ao < 1.f / 512.f) {
        dst[0] = dst[1] = dst[2] = dst[3] = 0;
        return;
    }
    float const inv = 1.f / Ao;
    dst[0] = static_cast<std::uint8_t>(std::clamp(Co_r * inv * 255.f, 0.f, 255.f));
    dst[1] = static_cast<std::uint8_t>(std::clamp(Co_g * inv * 255.f, 0.f, 255.f));
    dst[2] = static_cast<std::uint8_t>(std::clamp(Co_b * inv * 255.f, 0.f, 255.f));
    dst[3] = static_cast<std::uint8_t>(std::clamp(Ao * 255.f, 0.f, 255.f));
}

static void RasterizeTriangle(
    Software2DContext & surf,
    CpuTexture const * tex_or_null,
    ImDrawVert const & v0,
    ImDrawVert const & v1,
    ImDrawVert const & v2,
    float clip_x0,
    float clip_y0,
    float clip_x1,
    float clip_y1) noexcept
{
    ImVec2 const p0 = v0.pos;
    ImVec2 const p1 = v1.pos;
    ImVec2 const p2 = v2.pos;

    float const area = Orient2d(p0, p1, p2);
    if (std::fabs(area) < 1e-16f)
        return;

    ImVec4 const c0 = ImGui::ColorConvertU32ToFloat4(v0.col);
    ImVec4 const c1 = ImGui::ColorConvertU32ToFloat4(v1.col);
    ImVec4 const c2 = ImGui::ColorConvertU32ToFloat4(v2.col);

    float const min_x_f = std::min({p0.x, p1.x, p2.x});
    float const max_x_f = std::max({p0.x, p1.x, p2.x});
    float const min_y_f = std::min({p0.y, p1.y, p2.y});
    float const max_y_f = std::max({p0.y, p1.y, p2.y});

    int const surf_w = surf.Width();
    int const surf_h = surf.Height();
    if (surf_w <= 0 || surf_h <= 0)
        return;

    int bx0 = static_cast<int>(std::floor(min_x_f));
    int by0 = static_cast<int>(std::floor(min_y_f));
    int bx1 = static_cast<int>(std::ceil(max_x_f)) - 1;
    int by1 = static_cast<int>(std::ceil(max_y_f)) - 1;

    bx0 = std::max(bx0, static_cast<int>(std::floor(clip_x0)));
    by0 = std::max(by0, static_cast<int>(std::floor(clip_y0)));
    bx1 = std::min(bx1, static_cast<int>(std::ceil(clip_x1)) - 1);
    by1 = std::min(by1, static_cast<int>(std::ceil(clip_y1)) - 1);

    bx0 = std::max(0, bx0);
    by0 = std::max(0, by0);
    bx1 = std::min(surf_w - 1, bx1);
    by1 = std::min(surf_h - 1, by1);

    if (bx0 > bx1 || by0 > by1)
        return;

    std::uint8_t * const base = surf.Pixels();
    int const stride = surf.StrideBytes();
    if (!base || stride <= 0)
        return;

    float tex_sample[4]{};

    for (int iy = by0; iy <= by1; ++iy) {
        for (int ix = bx0; ix <= bx1; ++ix) {
            ImVec2 const p(static_cast<float>(ix) + 0.5f, static_cast<float>(iy) + 0.5f);

            float const w0 = Orient2d(p1, p2, p) / area;
            float const w1 = Orient2d(p2, p0, p) / area;
            float const w2 = Orient2d(p0, p1, p) / area;
            constexpr float eps = -1e-5f;
            if (w0 < eps || w1 < eps || w2 < eps)
                continue;

            float const u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
            float const v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;

            float const Pr = w0 * c0.x + w1 * c1.x + w2 * c2.x;
            float const Pg = w0 * c0.y + w1 * c1.y + w2 * c2.y;
            float const Pb = w0 * c0.z + w1 * c1.z + w2 * c2.z;
            float const Pa = w0 * c0.w + w1 * c1.w + w2 * c2.w;

            float Br = Pr;
            float Bg = Pg;
            float Bb = Pb;
            float Ba = Pa;

            if (tex_or_null && tex_or_null->w > 0 && tex_or_null->h > 0) {
                SampleRgbaLinear(*tex_or_null, u, v, tex_sample);
                Br *= tex_sample[0];
                Bg *= tex_sample[1];
                Bb *= tex_sample[2];
                Ba *= tex_sample[3];
            }

            if (Ba < 1.f / 512.f)
                continue;

            if (p.x < clip_x0 || p.x >= clip_x1 || p.y < clip_y0 || p.y >= clip_y1)
                continue;

            std::uint8_t * const d = base + static_cast<std::size_t>(iy) * static_cast<std::size_t>(stride)
                + static_cast<std::size_t>(ix) * 4u;
            BlendStraightOver(d, Br, Bg, Bb, Ba);
        }
    }
}

} // namespace

bool ImGui_ImplHonkordSoftware_Init(ApplicationContextSettings * app, SoftwareRenderer * renderer)
{
    IMGUI_CHECKVERSION();
    ImGuiIO & io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized?");
    IM_ASSERT(app != nullptr && renderer != nullptr);

    auto * const bd = IM_NEW(ImGui_ImplHonkordSoftware_Data)();
    io.BackendRendererUserData = bd;
    bd->App = app;
    bd->Renderer = renderer;

    io.BackendRendererName = "HonkordGL (Software CPU)";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;

    return true;
}

void ImGui_ImplHonkordSoftware_Shutdown()
{
    ImGui_ImplHonkordSoftware_Data * const bd = GetBackendData();
    if (!bd)
        return;

    for (auto & kv : bd->Textures) {
        delete kv.second;
        kv.second = nullptr;
    }
    bd->Textures.clear();

    ImGuiIO & io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);

    IM_DELETE(bd);
}

void ImGui_ImplHonkordSoftware_NewFrame(ApplicationContextSettings * app)
{
    ImGui_ImplHonkordSoftware_Data * const bd = GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplHonkordSoftware_Init?");

    ImGuiIO & io = ImGui::GetIO();
    if (!app) {
        io.DeltaTime = 1.0f / 60.0f;
        return;
    }

    int const fbw = app->frame_buffer_width > 0 ? app->frame_buffer_width : app->client_rect.w;
    int const fbh = app->frame_buffer_height > 0 ? app->frame_buffer_height : app->client_rect.z;
    io.DisplaySize = ImVec2(static_cast<float>(fbw), static_cast<float>(fbh));

    float const sx = app->dpi_x > 0 ? static_cast<float>(app->dpi_x) / 96.f : 1.f;
    float const sy = app->dpi_y > 0 ? static_cast<float>(app->dpi_y) / 96.f : sx;
    io.DisplayFramebufferScale = ImVec2(sx, sy);

    auto const now = std::chrono::steady_clock::now();
    if (!bd->TimeValid) {
        bd->TimeValid = true;
        bd->LastFrame = now;
        io.DeltaTime = 1.0f / 60.0f;
    } else {
        float const dt = std::chrono::duration<float>(now - bd->LastFrame).count();
        bd->LastFrame = now;
        io.DeltaTime = dt > 1e-6f ? dt : 1.0f / 60.0f;
    }
}

bool ImGui_ImplHonkordSoftware_ProcessEvent(ApplicationContextSettings * app, HonkordGL::Events::Event const & ev)
{
    ImGui_ImplHonkordSoftware_Data * const bd = GetBackendData();
    if (!bd || app != bd->App)
        return false;

    ImGuiIO & io = ImGui::GetIO();
    using ET = HonkordGL::Events::EventType;

    switch (ev.type) {
    case ET::QUIT:
        return false;
    case ET::RESIZE:
        return false;
    case ET::FOCUS:
        io.AddFocusEvent(ev.focused != 0);
        return false;
    case ET::MOUSE_ENTER:
        io.AddMousePosEvent(static_cast<float>(ev.x), static_cast<float>(ev.y));
        return false;
    case ET::MOUSE_LEAVE:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        return false;
    case ET::MOUSE_MOVE:
        io.AddMousePosEvent(static_cast<float>(ev.x), static_cast<float>(ev.y));
        return false;
    case ET::MOUSE_BUTTON_PRESS: {
        if (ev.mouse_button == 4u) {
            io.AddMouseWheelEvent(0.f, 1.f);
            return false;
        }
        if (ev.mouse_button == 5u) {
            io.AddMouseWheelEvent(0.f, -1.f);
            return false;
        }
        int const mb = NativeMouseButtonToImGui(ev.mouse_button);
        if (mb >= 0)
            io.AddMouseButtonEvent(mb, true);
        return false;
    }
    case ET::MOUSE_BUTTON_RELEASE: {
        if (ev.mouse_button == 4u || ev.mouse_button == 5u)
            return false;
        int const mb = NativeMouseButtonToImGui(ev.mouse_button);
        if (mb >= 0)
            io.AddMouseButtonEvent(mb, false);
        return false;
    }
    case ET::KEY_PRESS: {
        ImGuiKey const ik = KeyCodeToImGui(ev.key);
        if (ik != ImGuiKey_None)
            io.AddKeyEvent(ik, true);
        if (ev.character != 0u && !io.KeyCtrl && !io.KeySuper)
            io.AddInputCharacter(ev.character);
        return false;
    }
    case ET::KEY_RELEASE: {
        ImGuiKey const ik = KeyCodeToImGui(ev.key);
        if (ik != ImGuiKey_None)
            io.AddKeyEvent(ik, false);
        return false;
    }
    default:
        return false;
    }
}

void ImGui_ImplHonkordSoftware_UpdateTexture(ImTextureData * tex)
{
    ImGui_ImplHonkordSoftware_Data * const bd = GetBackendData();
    if (!tex || !bd)
        return;

    std::vector<unsigned char> scratch_rgba;

    if (tex->Status == ImTextureStatus_WantDestroy) {
        if (tex->TexID != ImTextureID_Invalid && tex->TexID != 0) {
            auto it = bd->Textures.find(tex->TexID);
            if (it != bd->Textures.end()) {
                delete it->second;
                bd->Textures.erase(it);
            }
            tex->SetTexID(ImTextureID_Invalid);
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
        return;
    }

    if (tex->Status == ImTextureStatus_WantCreate) {
        unsigned char const * upload_ptr = tex->Pixels;
        if (tex->Format == ImTextureFormat_Alpha8) {
            if (!ExpandAlpha8ToRgba(tex, &scratch_rgba))
                return;
            upload_ptr = scratch_rgba.data();
        } else if (tex->Format != ImTextureFormat_RGBA32 || !tex->Pixels) {
            return;
        }

        auto * const ct = new CpuTexture{};
        ct->w = tex->Width;
        ct->h = tex->Height;
        ct->rgba.resize(static_cast<std::size_t>(tex->Width * tex->Height) * 4u);
        std::memcpy(ct->rgba.data(), upload_ptr, ct->rgba.size());

        ImTextureID const id = reinterpret_cast<ImTextureID>(static_cast<void *>(ct));
        bd->Textures[id] = ct;
        tex->SetTexID(id);
        tex->SetStatus(ImTextureStatus_OK);
        return;
    }

    if (tex->Status == ImTextureStatus_WantUpdates) {
        ImTextureID const tid = tex->TexID;
        auto it = bd->Textures.find(tid);
        if (it == bd->Textures.end() || !it->second)
            return;
        CpuTexture * const ct = it->second;

        auto uploadBlock = [&](int x, int y, int w, int h) {
            if (w <= 0 || h <= 0 || !tex->Pixels)
                return;
            if (tex->Format == ImTextureFormat_RGBA32) {
                for (int row = 0; row < h; ++row) {
                    unsigned char const * src =
                        tex->Pixels + (x + (y + row) * tex->Width) * tex->BytesPerPixel;
                    unsigned char * dst =
                        ct->rgba.data()
                        + (static_cast<std::size_t>(y + row) * static_cast<std::size_t>(ct->w)
                              + static_cast<std::size_t>(x))
                            * 4u;
                    std::memcpy(dst, src, static_cast<std::size_t>(w) * 4u);
                }
                return;
            }
            if (tex->Format == ImTextureFormat_Alpha8) {
                for (int row = 0; row < h; ++row) {
                    unsigned char const * src_row =
                        tex->Pixels + (x + (y + row) * tex->Width);
                    unsigned char * dst_row =
                        ct->rgba.data()
                        + (static_cast<std::size_t>(y + row) * static_cast<std::size_t>(ct->w)
                              + static_cast<std::size_t>(x))
                            * 4u;
                    for (int col = 0; col < w; ++col) {
                        unsigned char const a = src_row[col];
                        unsigned char * d = dst_row + static_cast<std::size_t>(col) * 4u;
                        d[0] = 255;
                        d[1] = 255;
                        d[2] = 255;
                        d[3] = a;
                    }
                }
            }
        };

        if (tex->Updates.Size > 0) {
            for (ImTextureRect const & r : tex->Updates)
                uploadBlock(r.x, r.y, r.w, r.h);
        } else {
            ImTextureRect const & u = tex->UpdateRect;
            uploadBlock(u.x, u.y, u.w, u.h);
        }

        tex->SetStatus(ImTextureStatus_OK);
    }
}

void ImGui_ImplHonkordSoftware_RenderDrawData(
    ImDrawData * draw_data,
    SoftwareRenderer * renderer,
    std::uint8_t clear_r,
    std::uint8_t clear_g,
    std::uint8_t clear_b)
{
    ImGui_ImplHonkordSoftware_Data * const bd = GetBackendData();
    if (!draw_data || !bd || !renderer)
        return;
    IM_ASSERT(renderer == bd->Renderer && "Pass the same SoftwareRenderer given to ImGui_ImplHonkordSoftware_Init");

    int const fb_width =
        static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int const fb_height =
        static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    if (!renderer->EnsureSurface())
        return;

    Software2DContext & surf = renderer->Surface();
    surf.Clear(clear_r, clear_g, clear_b, 255);

    if (draw_data->Textures != nullptr) {
        for (ImTextureData * tex : *draw_data->Textures) {
            if (tex && tex->Status != ImTextureStatus_OK)
                ImGui_ImplHonkordSoftware_UpdateTexture(tex);
        }
    }

    ImVec2 const clip_off = draw_data->DisplayPos;
    ImVec2 const clip_scale = draw_data->FramebufferScale;

    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        ImDrawList const * const cmd_list = draw_data->CmdLists[n];
        ImDrawVert const * const vtx = cmd_list->VtxBuffer.Data;
        ImDrawIdx const * const ib = cmd_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            ImDrawCmd const * const pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                if (platform_io.DrawCallback_ResetRenderState
                    && pcmd->UserCallback == platform_io.DrawCallback_ResetRenderState)
                    continue;
                pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            ImVec4 const cr = pcmd->ClipRect;
            ImVec2 clip_min((cr.x - clip_off.x) * clip_scale.x, (cr.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((cr.z - clip_off.x) * clip_scale.x, (cr.w - clip_off.y) * clip_scale.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            ImTextureID const tex_id = pcmd->GetTexID();
            CpuTexture const * cpu_tex = nullptr;
            auto const tit = bd->Textures.find(tex_id);
            if (tit != bd->Textures.end())
                cpu_tex = tit->second;

            unsigned int const vtx_base = static_cast<unsigned int>(pcmd->VtxOffset);
            unsigned int const idx_end = static_cast<unsigned int>(pcmd->IdxOffset + pcmd->ElemCount);

            for (unsigned int ii = static_cast<unsigned int>(pcmd->IdxOffset); ii + 2 < idx_end; ii += 3) {
                ImDrawIdx const i0 = ib[ii + 0];
                ImDrawIdx const i1 = ib[ii + 1];
                ImDrawIdx const i2 = ib[ii + 2];
                ImDrawVert const & v0 = vtx[static_cast<size_t>(i0) + vtx_base];
                ImDrawVert const & v1 = vtx[static_cast<size_t>(i1) + vtx_base];
                ImDrawVert const & v2 = vtx[static_cast<size_t>(i2) + vtx_base];

                RasterizeTriangle(
                    surf,
                    cpu_tex,
                    v0,
                    v1,
                    v2,
                    clip_min.x,
                    clip_min.y,
                    clip_max.x,
                    clip_max.y);
            }
        }
    }
}

#endif // IMGUI_DISABLE
