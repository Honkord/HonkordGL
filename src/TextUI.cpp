/**
 * HonkordGL — TextUI (GPU text, embedded font, stb_truetype)
 * Copyright (C) 2026 Honkord
 *
 * Define `HONKORDGL_TEXTUI_GPU` to `0` when compiling this TU for a software-only link (no `GpuRenderer`).
 */

#if !defined(HONKORDGL_TEXTUI_GPU)
#define HONKORDGL_TEXTUI_GPU 1
#endif

#include <HonkordGL/TextUI.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Software2DContext.h>

#include "Font.h"
#if HONKORDGL_TEXTUI_GPU
#include "internal/DesktopGlIncludes.h"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include <HonkordGL/third_party/stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace HonkordGL::Graphics
{
namespace
{
#if HONKORDGL_TEXTUI_GPU
bool GlslGpuPathOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}
#endif

constexpr int kAtlasSize = 512;
constexpr int kFirstChar = 32;
constexpr int kCharCount = 96;
constexpr float kBakePx = 32.0f;

constexpr unsigned kMaxSlots = 128;

struct TextUiGlobals
{
    GpuRenderer * gpu{nullptr};
    ApplicationContextSettings * app{nullptr};
    Software2DContext * softSurface{nullptr};
    bool softMode{false};

    bool bound{false};
    bool beginActive{false};
    bool resourcesReady{false};

    std::vector<unsigned char> cpuAtlasRgba;

    stbtt_fontinfo font{};
    stbtt_bakedchar bakeChars[kCharCount]{};

    GpuObjectName program{0};
    GpuObjectName atlas{0};
    GpuObjectName vbo{0};
    GpuObjectName vao{0};

    int locMvp{-1};
    int locColor{-1};
    int locTex{-1};

    float pendingR{1.f};
    float pendingG{1.f};
    float pendingB{1.f};
    float pendingA{1.f};
    float pendingSize{16.f};
    float pendingRectX{0.f};
    float pendingRectY{0.f};
    float pendingRectW{0.f};
    float pendingRectH{0.f};
    bool pendingMovable{false};

    bool defaultsFromWindow{false};

    unsigned persistedCount{0};
    float persistedRectX[kMaxSlots]{};
    float persistedRectY[kMaxSlots]{};
    float persistedRectW[kMaxSlots]{};
    float persistedRectH[kMaxSlots]{};
    bool persistedMovable[kMaxSlots]{};

    unsigned slotCount{0};
    float instRectX[kMaxSlots]{};
    float instRectY[kMaxSlots]{};
    float instRectW[kMaxSlots]{};
    float instRectH[kMaxSlots]{};
    bool instMovable[kMaxSlots]{};
    float dragX[kMaxSlots]{};
    float dragY[kMaxSlots]{};

    int dragSlot{-1};
    float grabDx{0.f};
    float grabDy{0.f};

    float fontScaleBake{1.f};
    int fontAscent{0};
    int fontDescent{0};
    int fontLineGap{0};
};

TextUiGlobals g{};

#if HONKORDGL_TEXTUI_GPU
using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar *);
using PFN_glUniformMatrix4fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniform4fv = void (*)(GLint, GLsizei, const GLfloat *);
using PFN_glUniform1i = void (*)(GLint, GLint);
using PFN_glEnable = void (*)(GLenum);
using PFN_glDisable = void (*)(GLenum);
using PFN_glBlendFunc = void (*)(GLenum, GLenum);

bool LoadGlTextProcs(GpuRenderer & gpu) noexcept
{
    (void)gpu;
#if HONKORDGL_PLATFORM_APPLE
    return true;
#else
    return gpu.GetGraphicsProc("glGetUniformLocation") != nullptr;
#endif
}
void OrthoUi(float fbW, float fbH, float * out16) noexcept
{
    if (fbW <= 0.f)
        fbW = 1.f;
    if (fbH <= 0.f)
        fbH = 1.f;
    const float invW = 2.f / fbW;
    const float invH = -2.f / fbH;
    out16[0] = invW;
    out16[1] = 0.f;
    out16[2] = 0.f;
    out16[3] = 0.f;
    out16[4] = 0.f;
    out16[5] = invH;
    out16[6] = 0.f;
    out16[7] = 0.f;
    out16[8] = 0.f;
    out16[9] = 0.f;
    out16[10] = 1.f;
    out16[11] = 0.f;
    out16[12] = -1.f;
    out16[13] = 1.f;
    out16[14] = 0.f;
    out16[15] = 1.f;
}
#endif // HONKORDGL_TEXTUI_GPU

void ReleaseSoftwareBinding() noexcept
{
    g.softSurface = nullptr;
    g.softMode = false;
}
bool EnsureFontAtlasCpu() noexcept
{
    if (!g.cpuAtlasRgba.empty())
        return true;

    std::vector<unsigned char> bitmap(static_cast<std::size_t>(kAtlasSize * kAtlasSize), 0);
    const int bakeResult = stbtt_BakeFontBitmap(
        const_cast<unsigned char *>(font_data),
        0,
        kBakePx,
        bitmap.data(),
        kAtlasSize,
        kAtlasSize,
        kFirstChar,
        kCharCount,
        g.bakeChars);
    if (bakeResult <= 0)
        return false;

    g.cpuAtlasRgba.resize(static_cast<std::size_t>(kAtlasSize * kAtlasSize * 4u), 0);
    for (int i = 0; i < kAtlasSize * kAtlasSize; ++i) {
        const unsigned char a = bitmap[static_cast<std::size_t>(i)];
        g.cpuAtlasRgba[static_cast<std::size_t>(i * 4 + 0)] = 255;
        g.cpuAtlasRgba[static_cast<std::size_t>(i * 4 + 1)] = 255;
        g.cpuAtlasRgba[static_cast<std::size_t>(i * 4 + 2)] = 255;
        g.cpuAtlasRgba[static_cast<std::size_t>(i * 4 + 3)] = a;
    }

    if (!stbtt_InitFont(&g.font, const_cast<unsigned char *>(font_data), stbtt_GetFontOffsetForIndex(font_data, 0)))
        return false;
    g.fontScaleBake = stbtt_ScaleForPixelHeight(&g.font, kBakePx);
    stbtt_GetFontVMetrics(&g.font, &g.fontAscent, &g.fontDescent, &g.fontLineGap);
    return true;
}
bool InitSoftwareResources() noexcept
{
    if (!g.app || !g.softSurface)
        return false;
    if (g.resourcesReady)
        return true;
    if (!EnsureFontAtlasCpu())
        return false;
    g.resourcesReady = true;
    return true;
}
static void DrawTriangleTexturedSoft(
    Software2DContext & fb,
    float x0,
    float y0,
    float u0,
    float v0,
    float x1,
    float y1,
    float u1,
    float v1,
    float x2,
    float y2,
    float u2,
    float v2,
    const unsigned char * atlas,
    int aw,
    int ah,
    float Cr,
    float Cg,
    float Cb,
    float drawA) noexcept
{
    const float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
    if (denom == 0.f)
        return;

    const int W = fb.Width();
    const int H = fb.Height();
    if (W <= 0 || H <= 0)
        return;
    std::uint8_t * const pixels = fb.Pixels();

    const int minx = (int)std::floor((std::min)({x0, x1, x2}));
    const int maxx = (int)std::ceil((std::max)({x0, x1, x2}));
    const int miny = (int)std::floor((std::min)({y0, y1, y2}));
    const int maxy = (int)std::ceil((std::max)({y0, y1, y2}));

    for (int py = miny; py <= maxy; ++py) {
        for (int px = minx; px <= maxx; ++px) {
            const float pxm = static_cast<float>(px) + 0.5f;
            const float pym = static_cast<float>(py) + 0.5f;
            const float w0 = ((y1 - y2) * (pxm - x2) + (x2 - x1) * (pym - y2)) / denom;
            const float w1 = ((y2 - y0) * (pxm - x2) + (x0 - x2) * (pym - y2)) / denom;
            const float w2 = 1.f - w0 - w1;
            if (w0 < 0.f || w1 < 0.f || w2 < 0.f)
                continue;
            if (px < 0 || py < 0 || px >= W || py >= H)
                continue;
            float u = w0 * u0 + w1 * u1 + w2 * u2;
            float vv = w0 * v0 + w1 * v1 + w2 * v2;
            u = std::clamp(u, 0.f, 1.f);
            vv = std::clamp(vv, 0.f, 1.f);
            const int txi = static_cast<int>(u * static_cast<float>(aw - 1));
            const int tyi = static_cast<int>(vv * static_cast<float>(ah - 1));
            const int tcx = std::clamp(txi, 0, aw - 1);
            const int tcy = std::clamp(tyi, 0, ah - 1);
            const std::size_t ti =
                (static_cast<std::size_t>(tcy) * static_cast<std::size_t>(aw) + static_cast<std::size_t>(tcx)) * 4u;
            const float texA = static_cast<float>(atlas[ti + 3]) / 255.f;
            const float srcA = drawA * texA;
            if (srcA <= 0.f)
                continue;
            std::uint8_t * const p =
                pixels + (static_cast<std::size_t>(py) * static_cast<std::size_t>(W) + static_cast<std::size_t>(px)) * 4u;
            const float dr = static_cast<float>(p[0]);
            const float dg = static_cast<float>(p[1]);
            const float db = static_cast<float>(p[2]);
            const float br = std::clamp(Cr, 0.f, 1.f) * 255.f;
            const float bg = std::clamp(Cg, 0.f, 1.f) * 255.f;
            const float bb = std::clamp(Cb, 0.f, 1.f) * 255.f;
            p[0] = static_cast<std::uint8_t>(br * srcA + dr * (1.f - srcA));
            p[1] = static_cast<std::uint8_t>(bg * srcA + dg * (1.f - srcA));
            p[2] = static_cast<std::uint8_t>(bb * srcA + db * (1.f - srcA));
            p[3] = 255;
        }
    }
}
static void RasterizeTextVertsOverSoftware2D(
    Software2DContext & fb, std::vector<float> const & verts, float Cr, float Cg, float Cb, float drawA) noexcept
{
    if (g.cpuAtlasRgba.empty())
        return;
    const unsigned char * const atlas = g.cpuAtlasRgba.data();
    for (std::size_t i = 0; i + 23 < verts.size(); i += 24) {
        const float xa = verts[i + 0];
        const float ya = verts[i + 1];
        const float ua = verts[i + 2];
        const float va = verts[i + 3];
        const float xb = verts[i + 4];
        const float yb = verts[i + 5];
        const float ub = verts[i + 6];
        const float vb = verts[i + 7];
        const float xc = verts[i + 8];
        const float yc = verts[i + 9];
        const float uc = verts[i + 10];
        const float vc = verts[i + 11];
        const float xd = verts[i + 12];
        const float yd = verts[i + 13];
        const float ud = verts[i + 14];
        const float vd = verts[i + 15];
        const float xe = verts[i + 16];
        const float ye = verts[i + 17];
        const float ue = verts[i + 18];
        const float ve = verts[i + 19];
        const float xf = verts[i + 20];
        const float yf = verts[i + 21];
        const float uf = verts[i + 22];
        const float vf = verts[i + 23];
        DrawTriangleTexturedSoft(
            fb, xa, ya, ua, va, xb, yb, ub, vb, xc, yc, uc, vc, atlas, kAtlasSize, kAtlasSize, Cr, Cg, Cb, drawA);
        DrawTriangleTexturedSoft(
            fb, xd, yd, ud, vd, xe, ye, ue, ve, xf, yf, uf, vf, atlas, kAtlasSize, kAtlasSize, Cr, Cg, Cb, drawA);
    }
}

#if HONKORDGL_TEXTUI_GPU
bool InitGpuResources() noexcept
{
    if (!g.gpu || !g.app || !GlslGpuPathOk(g.app))
        return false;
    if (g.resourcesReady)
        return true;

    if (g.gpu->LoadShaderCompilerProcs() != static_cast<int>(GpuShaderCompileError::OK))
        return false;

    static const char kVs[] = R"(#version 330 core
        layout(location = 0) in vec2 a_pos;
        layout(location = 1) in vec2 a_uv;
        uniform mat4 u_mvp;
        out vec2 v_uv;
        void main() {
        v_uv = a_uv;
        gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
        }
    )";

    static const char kFs[] = R"(#version 330 core
        in vec2 v_uv;
        uniform sampler2D u_tex;
        uniform vec4 u_color;
        layout(location = 0) out vec4 o_color;
        void main() {
        float a = texture(u_tex, v_uv).a;
        o_color = vec4(u_color.rgb, u_color.a * a);
        }
    )";

    const int locs[2] = {0, 1};
    const char * names[2] = {"a_pos", "a_uv"};
    char log[512]{};
    GpuObjectName prog = 0;
    const int cr = g.gpu->CompileShaderProgram(kVs, kFs, &prog, log, sizeof log, locs, names, 2);
    if (cr != static_cast<int>(GpuShaderCompileError::OK) || prog == 0)
        return false;
    g.program = prog;

    if (g.gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return false;

    auto * const glGetUniformLocationFn =
        reinterpret_cast<PFN_glGetUniformLocation>(g.gpu->GetGraphicsProc("glGetUniformLocation"));
    if (!glGetUniformLocationFn)
        return false;
    g.locMvp = glGetUniformLocationFn(static_cast<GLuint>(g.program), "u_mvp");
    g.locColor = glGetUniformLocationFn(static_cast<GLuint>(g.program), "u_color");
    g.locTex = glGetUniformLocationFn(static_cast<GLuint>(g.program), "u_tex");
    if (g.locMvp < 0 || g.locColor < 0 || g.locTex < 0)
        return false;

    if (!EnsureFontAtlasCpu())
        return false;

    GpuObjectName tex = 0;
    if (g.gpu->CreateTexture2DRgba8(kAtlasSize, kAtlasSize, g.cpuAtlasRgba.data(), &tex)
        != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    g.atlas = tex;

    if (g.gpu->CreateBuffer(&g.vbo) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    if (g.gpu->CreateVertexArray(&g.vao) != static_cast<int>(GpuShaderCompileError::OK))
        return false;

    if (g.gpu->BindVertexArray(g.vao) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    if (g.gpu->BindBuffer(GpuBufferTarget::Vertex, g.vbo) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    if (g.gpu->EnableVertexAttribute(0) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    if (g.gpu->EnableVertexAttribute(1) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    constexpr int stride = 4 * static_cast<int>(sizeof(float));
    if (g.gpu->SetVertexAttributeFloat(0, 2, stride, 0, false) != static_cast<int>(GpuShaderCompileError::OK))
        return false;
    if (g.gpu->SetVertexAttributeFloat(1, 2, stride, 2u * sizeof(float), false)
        != static_cast<int>(GpuShaderCompileError::OK))
        return false;

    g.resourcesReady = true;
    return true;
}
#endif // HONKORDGL_TEXTUI_GPU

void ReleaseGpuResources() noexcept
{
#if HONKORDGL_TEXTUI_GPU
    if (g.gpu) {
        (void)g.gpu->MakeCurrent();
        if (g.vao)
            g.gpu->DestroyVertexArray(g.vao);
        if (g.vbo)
            g.gpu->DestroyBuffer(g.vbo);
        if (g.atlas)
            g.gpu->DestroyTexture(g.atlas);
        if (g.program)
            g.gpu->DeleteShaderProgram(g.program);
    }
#endif
    g.vao = 0;
    g.vbo = 0;
    g.atlas = 0;
    g.program = 0;
    g.locMvp = -1;
    g.locColor = -1;
    g.locTex = -1;
    g.resourcesReady = false;
}
bool Utf8Next(const char *& p, unsigned int & outCp) noexcept
{
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 0)
        return false;
    if (c < 0x80u) {
        outCp = c;
        ++p;
        return true;
    }
    if ((c & 0xE0u) == 0xC0u) {
        const unsigned char c1 = static_cast<unsigned char>(p[1]);
        if ((c1 & 0xC0u) != 0x80u)
            return false;
        outCp = (static_cast<unsigned int>(c & 0x1Fu) << 6u) | static_cast<unsigned int>(c1 & 0x3Fu);
        p += 2;
        return true;
    }
    if ((c & 0xF0u) == 0xE0u) {
        const unsigned char c1 = static_cast<unsigned char>(p[1]);
        const unsigned char c2 = static_cast<unsigned char>(p[2]);
        if (((c1 | c2) & 0xC0u) != 0x80u)
            return false;
        outCp = (static_cast<unsigned int>(c & 0x0Fu) << 12u)
            | (static_cast<unsigned int>(c1 & 0x3Fu) << 6u)
            | static_cast<unsigned int>(c2 & 0x3Fu);
        p += 3;
        return true;
    }
    if ((c & 0xF8u) == 0xF0u) {
        const unsigned char c1 = static_cast<unsigned char>(p[1]);
        const unsigned char c2 = static_cast<unsigned char>(p[2]);
        const unsigned char c3 = static_cast<unsigned char>(p[3]);
        if (((c1 | c2 | c3) & 0xC0u) != 0x80u)
            return false;
        outCp = (static_cast<unsigned int>(c & 0x07u) << 18u)
            | (static_cast<unsigned int>(c1 & 0x3Fu) << 12u)
            | (static_cast<unsigned int>(c2 & 0x3Fu) << 6u)
            | static_cast<unsigned int>(c3 & 0x3Fu);
        p += 4;
        return true;
    }
    ++p;
    outCp = static_cast<unsigned int>(0xFFFDu);
    return true;
}

} // namespace

#if HONKORDGL_TEXTUI_GPU
int TextUIBindDisplay(GpuRenderer & gpu, ApplicationContextSettings & app) noexcept
{
    ReleaseSoftwareBinding();
    ReleaseGpuResources();
    g.gpu = &gpu;
    g.app = &app;
    g.bound = true;
    g.beginActive = false;
    g.defaultsFromWindow = true;
    g.slotCount = 0;
    g.persistedCount = 0;
    g.dragSlot = -1;

    if (!GlslGpuPathOk(&app))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadGlTextProcs(gpu))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);

    if (!InitGpuResources())
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);

    return static_cast<int>(GpuShaderCompileError::OK);
}
#else
int TextUIBindDisplay(GpuRenderer & gpu, ApplicationContextSettings & app) noexcept
{
    (void)gpu;
    (void)app;
    return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
}
#endif
int TextUIBindSoftware2D(ApplicationContextSettings & app, Software2DContext & surface) noexcept
{
    ReleaseSoftwareBinding();
    ReleaseGpuResources();
    g.gpu = nullptr;
    g.app = &app;
    g.softSurface = &surface;
    g.softMode = true;
    g.bound = true;
    g.beginActive = false;
    g.defaultsFromWindow = true;
    g.slotCount = 0;
    g.persistedCount = 0;
    g.dragSlot = -1;

    if (!InitSoftwareResources())
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    return static_cast<int>(GpuShaderCompileError::OK);
}
void TextUIShutdown() noexcept
{
    ReleaseGpuResources();
    ReleaseSoftwareBinding();
    g.cpuAtlasRgba.clear();
    g.gpu = nullptr;
    g.app = nullptr;
    g.bound = false;
    g.beginActive = false;
    g.slotCount = 0;
    g.dragSlot = -1;
}
void TextUIBegin() noexcept
{
    if (!g.bound || !g.resourcesReady || !g.app)
        return;
    if (g.softMode) {
        if (!g.softSurface)
            return;
    } else if (!g.gpu) {
        return;
    }
    g.beginActive = true;
    g.slotCount = 0;
}
void TextUIEnd() noexcept
{
    if (g.beginActive) {
        g.persistedCount = g.slotCount;
        for (unsigned i = 0; i < g.slotCount && i < kMaxSlots; ++i) {
            g.persistedRectX[i] = g.instRectX[i];
            g.persistedRectY[i] = g.instRectY[i];
            g.persistedRectW[i] = g.instRectW[i];
            g.persistedRectH[i] = g.instRectH[i];
            g.persistedMovable[i] = g.instMovable[i];
        }
    }
    g.beginActive = false;
}
void TextUINextColor(float r, float gChannel, float b, float a) noexcept
{
    if (!g.beginActive)
        return;
    g.pendingR = r;
    g.pendingG = gChannel;
    g.pendingB = b;
    g.pendingA = a;
}
void TextUITextSize(float heightPixels) noexcept
{
    if (!g.beginActive)
        return;
    if (heightPixels > 0.f)
        g.pendingSize = heightPixels;
}
void TextUITextRect(float x, float y, float width, float height) noexcept
{
    if (!g.beginActive)
        return;
    g.pendingRectX = x;
    g.pendingRectY = y;
    g.pendingRectW = width;
    g.pendingRectH = height;
    g.defaultsFromWindow = false;
}
void TextUINextTextMovable() noexcept
{
    if (!g.beginActive)
        return;
    g.pendingMovable = true;
}
void TextUINextTextFixed() noexcept
{
    if (!g.beginActive)
        return;
    g.pendingMovable = false;
}
void TextUIDraw(const char * utf8Text) noexcept
{
    if (!g.beginActive || !utf8Text || !g.app || !g.resourcesReady)
        return;
    const bool swPath = g.softMode && g.softSurface;
    const bool gpuPath = !g.softMode && g.gpu;
    if (!swPath && !gpuPath)
        return;
#if HONKORDGL_TEXTUI_GPU
    if (gpuPath && g.gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
#endif

    const float fbW = static_cast<float>(g.app->frame_buffer_width > 0 ? g.app->frame_buffer_width : 1);
    const float fbH = static_cast<float>(g.app->frame_buffer_height > 0 ? g.app->frame_buffer_height : 1);

    if (g.defaultsFromWindow) {
        g.pendingRectX = 0.f;
        g.pendingRectY = 0.f;
        g.pendingRectW = fbW;
        g.pendingRectH = fbH;
    }

    if (g.slotCount >= kMaxSlots)
        return;

    const float drawR = g.pendingR;
    const float drawG = g.pendingG;
    const float drawB = g.pendingB;
    const float drawA = g.pendingA;

    const unsigned slot = g.slotCount;
    const float baseX = g.pendingRectX;
    const float baseY = g.pendingRectY;
    const float rw = g.pendingRectW;
    const float rh = g.pendingRectH;

    g.instRectX[slot] = baseX;
    g.instRectY[slot] = baseY;
    g.instRectW[slot] = rw;
    g.instRectH[slot] = rh;
    g.instMovable[slot] = g.pendingMovable;

    const float ox = g.dragX[slot];
    const float oy = g.dragY[slot];

    const float scale = g.pendingSize / kBakePx;
    const float lineHeightBake =
        static_cast<float>(g.fontAscent - g.fontDescent + g.fontLineGap) * g.fontScaleBake;

    float xpos = 0.f;
    float ypos = static_cast<float>(g.fontAscent) * g.fontScaleBake;

    std::vector<float> verts;
    verts.reserve(std::strlen(utf8Text) * 6u * 4u);

    const char * p = utf8Text;
    unsigned int cp = 0;
    while (Utf8Next(p, cp)) {
        if (cp == '\n' || cp == '\r') {
            if (cp == '\r' && *p == '\n')
                Utf8Next(p, cp);
            xpos = 0.f;
            ypos += lineHeightBake;
            continue;
        }
        if (cp < 32u || cp > 126u)
            continue;
        const int gix = static_cast<int>(cp) - kFirstChar;
        if (gix < 0 || gix >= kCharCount)
            continue;

        if (xpos * scale > rw && rw > 0.f) {
            xpos = 0.f;
            ypos += lineHeightBake;
        }

        stbtt_aligned_quad q{};
        float lx = xpos;
        float ly = ypos;
        stbtt_GetBakedQuad(g.bakeChars, kAtlasSize, kAtlasSize, gix, &lx, &ly, &q, 1);

        const float x0 = baseX + ox + q.x0 * scale;
        const float y0 = baseY + oy + q.y0 * scale;
        const float x1 = baseX + ox + q.x1 * scale;
        const float y1 = baseY + oy + q.y1 * scale;

        const float s0 = q.s0;
        const float t0 = q.t0;
        const float s1 = q.s1;
        const float t1 = q.t1;

        verts.insert(verts.end(), {x0, y0, s0, t0, x1, y0, s1, t0, x1, y1, s1, t1, x0, y0, s0, t0, x1, y1, s1, t1, x0, y1, s0, t1});

        xpos = lx;
        ypos = ly;
    }

    g.pendingR = 1.f;
    g.pendingG = 1.f;
    g.pendingB = 1.f;
    g.pendingA = 1.f;
    g.pendingMovable = false;

    ++g.slotCount;

    if (verts.empty())
        return;

    if (swPath) {
        RasterizeTextVertsOverSoftware2D(*g.softSurface, verts, drawR, drawG, drawB, drawA);
        return;
    }

#if HONKORDGL_TEXTUI_GPU
    auto * const glEnableFn = reinterpret_cast<PFN_glEnable>(g.gpu->GetGraphicsProc("glEnable"));
    auto * const glDisableFn = reinterpret_cast<PFN_glDisable>(g.gpu->GetGraphicsProc("glDisable"));
    auto * const glBlendFuncFn = reinterpret_cast<PFN_glBlendFunc>(g.gpu->GetGraphicsProc("glBlendFunc"));
    auto * const glUniformMatrix4fvFn =
        reinterpret_cast<PFN_glUniformMatrix4fv>(g.gpu->GetGraphicsProc("glUniformMatrix4fv"));
    auto * const glUniform4fvFn = reinterpret_cast<PFN_glUniform4fv>(g.gpu->GetGraphicsProc("glUniform4fv"));
    auto * const glUniform1iFn = reinterpret_cast<PFN_glUniform1i>(g.gpu->GetGraphicsProc("glUniform1i"));
    if (!glEnableFn || !glBlendFuncFn || !glUniformMatrix4fvFn || !glUniform4fvFn || !glUniform1iFn)
        return;

    float mvp[16];
    OrthoUi(fbW, fbH, mvp);

    const float col[4] = {drawR, drawG, drawB, drawA};

    glEnableFn(GL_BLEND);
    glBlendFuncFn(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    g.gpu->BindShaderProgram(g.program);
    glUniformMatrix4fvFn(g.locMvp, 1, GL_FALSE, mvp);
    glUniform4fvFn(g.locColor, 1, col);
    glUniform1iFn(g.locTex, 0);

    g.gpu->BindTexture2D(0, g.atlas);
    g.gpu->SetDefaultViewport();

    if (g.gpu->BindVertexArray(g.vao) != 0)
        return;
    if (g.gpu->BindBuffer(GpuBufferTarget::Vertex, g.vbo) != 0)
        return;

    const std::size_t byteSize = verts.size() * sizeof(float);
    if (g.gpu->UploadBufferData(
            GpuBufferTarget::Vertex,
            verts.data(),
            byteSize,
            GpuBufferUsage::Stream)
        != 0)
        return;

    const int vcount = static_cast<int>(verts.size() / 4u);
    if (g.gpu->DrawArrays(GpuPrimitive::Triangles, 0, vcount) != 0)
        return;

    if (glDisableFn)
        glDisableFn(GL_BLEND);
    g.gpu->BindShaderProgram(0);
#endif
}
void TextUIProcessEvent(ApplicationContextSettings & app, HonkordGL::Events::Event const & ev) noexcept
{
    if (!g.bound || g.app != &app)
        return;

    using ET = HonkordGL::Events::EventType;

    if (ev.type == ET::MOUSE_BUTTON_PRESS && ev.mouse_button == 1u) {
        const float mx = static_cast<float>(ev.x);
        const float my = static_cast<float>(ev.y);
        for (int si = static_cast<int>(g.persistedCount) - 1; si >= 0; --si) {
            const unsigned u = static_cast<unsigned>(si);
            if (!g.persistedMovable[u])
                continue;
            const float lx = g.persistedRectX[u] + g.dragX[u];
            const float ly = g.persistedRectY[u] + g.dragY[u];
            const float lw = g.persistedRectW[u];
            const float lh = g.persistedRectH[u];
            if (mx >= lx && mx < lx + lw && my >= ly && my < ly + lh) {
                g.dragSlot = si;
                g.grabDx = mx - lx;
                g.grabDy = my - ly;
                break;
            }
        }
        return;
    }

    if (ev.type == ET::MOUSE_BUTTON_RELEASE && ev.mouse_button == 1u) {
        g.dragSlot = -1;
        return;
    }

    if (ev.type == ET::MOUSE_MOVE && g.dragSlot >= 0) {
        const unsigned u = static_cast<unsigned>(g.dragSlot);
        const float mx = static_cast<float>(ev.x);
        const float my = static_cast<float>(ev.y);
        g.dragX[u] = mx - g.grabDx - g.persistedRectX[u];
        g.dragY[u] = my - g.grabDy - g.persistedRectY[u];
    }
}

} // namespace HonkordGL::Graphics