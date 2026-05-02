/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — SoftwareRenderer implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/SoftwareRenderer.h>

#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/WindowBackend.h>

#include <cmath>
#include <cstring>
#include <utility>

#if HONKORDGL_GPU_RENDERER_OPENGL_ES
#include <GLES2/gl2.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "internal/D3D11RendererContextWin32.hpp"
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace HonkordGL::Graphics {

namespace {

int EffectiveWidth(const ApplicationContextSettings& s) noexcept
{
    const int w = s.frame_buffer_width > 0 ? s.frame_buffer_width : s.client_rect.w;
    return w;
}
int EffectiveHeight(const ApplicationContextSettings& s) noexcept
{
    const int h = s.frame_buffer_height > 0 ? s.frame_buffer_height : s.client_rect.z;
    return h;
}

#if HONKORDGL_GPU_RENDERER_OPENGL_ES
static const char kVsBlit[] = R"(attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";
static const char kFsBlit[] = R"(precision mediump float;
varying vec2 v_uv;
uniform sampler2D u_tex;
void main() {
  gl_FragColor = texture2D(u_tex, vec2(v_uv.x, 1.0 - v_uv.y));
}
)";
#else
static const char kVsBlit[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";
static const char kFsBlit[] = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  o_color = texture(u_tex, vec2(v_uv.x, 1.0 - v_uv.y));
}
)";
#endif

void SetSamplerUniformUnit0(GpuRenderer & gpu, GpuObjectName program) noexcept
{
    gpu.BindShaderProgram(program);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
    const GLint loc = glGetUniformLocation(static_cast<GLuint>(program), "u_tex");
    if (loc >= 0)
        glUniform1i(loc, 0);
#else
    using PFN_glGetUniformLocation = int (*)(unsigned int, const char *);
    using PFN_glUniform1i = void (*)(int, int);
    auto * const gul = reinterpret_cast<PFN_glGetUniformLocation>(gpu.GetGraphicsProc("glGetUniformLocation"));
    auto * const u1i = reinterpret_cast<PFN_glUniform1i>(gpu.GetGraphicsProc("glUniform1i"));
    if (gul && u1i) {
        const int loc = gul(static_cast<unsigned int>(program), "u_tex");
        if (loc >= 0)
            u1i(loc, 0);
    }
#endif
    gpu.BindShaderProgram(0);
}

} // namespace

struct SoftwareRenderer::GpuPresentState {
    GpuRenderer renderer{};
    GpuObjectName program{};
    GpuObjectName tex{};
    GpuObjectName vao{};
    GpuObjectName vbo{};
    int texW{};
    int texH{};
#if defined(_WIN32)
    bool d3d11_present_path{};
    void * d3d_vs{};
    void * d3d_ps{};
    void * d3d_sampler{};
#endif
};

#if defined(_WIN32)
namespace {

bool InitSoftwareD3d11Blit(SoftwareRenderer::GpuPresentState & g, ApplicationContextSettings & app) noexcept
{
    using namespace Internal;
    auto * const st = GetD3D11State(app);
    if (!st || !st->device)
        return false;

    static char const kVsHlsl[] = R"(struct VS_OUT {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};
VS_OUT main(uint vid : SV_VertexID) {
  VS_OUT o;
  o.uv = float2((vid << 1) & 2, vid & 2);
  o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
  return o;
}
)";
    static char const kPsHlsl[] = R"(Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);
struct PS_IN {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};
float4 main(PS_IN i) : SV_Target {
  return gTex.Sample(gSamp, float2(i.uv.x, 1.0 - i.uv.y));
}
)";

    ID3DBlob * vsBlob = nullptr;
    ID3DBlob * psBlob = nullptr;
    ID3DBlob * errBlob = nullptr;
    HRESULT hr = D3DCompile(
        kVsHlsl,
        sizeof(kVsHlsl) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        &vsBlob,
        &errBlob);
    if (FAILED(hr) || !vsBlob) {
        if (errBlob)
            errBlob->Release();
        return false;
    }
    if (errBlob)
        errBlob->Release();
    errBlob = nullptr;

    hr = D3DCompile(
        kPsHlsl,
        sizeof(kPsHlsl) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        &psBlob,
        &errBlob);
    if (FAILED(hr) || !psBlob) {
        vsBlob->Release();
        if (errBlob)
            errBlob->Release();
        return false;
    }
    if (errBlob)
        errBlob->Release();

    ID3D11VertexShader * vs = nullptr;
    ID3D11PixelShader * ps = nullptr;
    if (FAILED(st->device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs))
        || FAILED(st->device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps))) {
        vsBlob->Release();
        psBlob->Release();
        if (vs)
            vs->Release();
        return false;
    }
    vsBlob->Release();
    psBlob->Release();

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState * samp = nullptr;
    if (FAILED(st->device->CreateSamplerState(&sd, &samp)) || !samp) {
        vs->Release();
        ps->Release();
        return false;
    }

    g.d3d_vs = vs;
    g.d3d_ps = ps;
    g.d3d_sampler = samp;
    g.d3d11_present_path = true;
    return true;
}

void ReleaseSoftwareD3d11Blit(SoftwareRenderer::GpuPresentState & g) noexcept
{
    if (g.d3d_sampler) {
        static_cast<ID3D11SamplerState *>(g.d3d_sampler)->Release();
        g.d3d_sampler = nullptr;
    }
    if (g.d3d_ps) {
        static_cast<ID3D11PixelShader *>(g.d3d_ps)->Release();
        g.d3d_ps = nullptr;
    }
    if (g.d3d_vs) {
        static_cast<ID3D11VertexShader *>(g.d3d_vs)->Release();
        g.d3d_vs = nullptr;
    }
    g.d3d11_present_path = false;
}

bool PresentSoftwareD3d11Blit(
    SoftwareRenderer::GpuPresentState & g,
    ApplicationContextSettings & app,
    Software2DContext & surface) noexcept
{
    using namespace Internal;
    auto * const st = GetD3D11State(app);
    if (!st || !st->immediate || !st->rtv || !g.d3d11_present_path)
        return false;

    const int w = surface.Width();
    const int h = surface.Height();
    if (w <= 0 || h <= 0)
        return false;

    if (g.tex == 0 || g.texW != w || g.texH != h) {
        if (g.tex)
            g.renderer.DestroyTexture(g.tex);
        g.tex = 0;
        g.texW = w;
        g.texH = h;
        if (g.renderer.CreateTexture2DRgba8(w, h, surface.Pixels(), &g.tex)
            != static_cast<int>(GpuShaderCompileError::OK))
            return false;
    } else {
        if (g.renderer.UpdateTexture2DRgba8(g.tex, w, h, surface.Pixels())
            != static_cast<int>(GpuShaderCompileError::OK))
            return false;
    }

    ID3D11DeviceContext * ctx = st->immediate;
    const float clearColor[4] = {0.f, 0.f, 0.f, 1.f};
    ctx->OMSetRenderTargets(1, &st->rtv, nullptr);
    ctx->ClearRenderTargetView(st->rtv, clearColor);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(st->bbWidth);
    vp.Height = static_cast<float>(st->bbHeight);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    ctx->RSSetViewports(1, &vp);

    ctx->VSSetShader(static_cast<ID3D11VertexShader *>(g.d3d_vs), nullptr, 0);
    ctx->PSSetShader(static_cast<ID3D11PixelShader *>(g.d3d_ps), nullptr, 0);
    ID3D11SamplerState * samp = static_cast<ID3D11SamplerState *>(g.d3d_sampler);
    ctx->PSSetSamplers(0, 1, &samp);
    g.renderer.BindTexture2D(0, g.tex);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);

    g.renderer.SwapBuffers();
    return true;
}

} // namespace
#endif // defined(_WIN32)

SoftwareRenderer::SoftwareRenderer() noexcept = default;

void SoftwareRenderer::ReleaseGpuPresent() noexcept
{
    if (!gpu_)
        return;
#if defined(_WIN32)
    ReleaseSoftwareD3d11Blit(*gpu_);
#endif
    if (gpu_->program)
        gpu_->renderer.DeleteShaderProgram(gpu_->program);
    if (gpu_->tex)
        gpu_->renderer.DestroyTexture(gpu_->tex);
    if (gpu_->vbo)
        gpu_->renderer.DestroyBuffer(gpu_->vbo);
    if (gpu_->vao)
        gpu_->renderer.DestroyVertexArray(gpu_->vao);
    gpu_->renderer.Destroy();
    gpu_.reset();
}

SoftwareRenderer::SoftwareRenderer(ApplicationContextSettings& app, WindowBackend& backend) noexcept
    : app_(&app)
    , backend_(&backend)
{
}

SoftwareRenderer::SoftwareRenderer(SoftwareRenderer&& other) noexcept
    : app_(other.app_)
    , backend_(other.backend_)
    , surface_(std::move(other.surface_))
    , gpu_(std::move(other.gpu_))
{
    other.app_ = nullptr;
    other.backend_ = nullptr;
}

SoftwareRenderer& SoftwareRenderer::operator=(SoftwareRenderer&& other) noexcept
{
    if (this != &other) {
        ReleaseGpuPresent();
        app_ = other.app_;
        backend_ = other.backend_;
        surface_ = std::move(other.surface_);
        gpu_ = std::move(other.gpu_);
        other.app_ = nullptr;
        other.backend_ = nullptr;
    }
    return *this;
}

SoftwareRenderer::~SoftwareRenderer() noexcept
{
    ReleaseGpuPresent();
}

void SoftwareRenderer::Bind(ApplicationContextSettings& app, WindowBackend& backend) noexcept
{
    if (app_ == &app && backend_ == &backend)
        return;
    ReleaseGpuPresent();
    app_ = &app;
    backend_ = &backend;
    (void)surface_.Resize(0, 0);
}
void SoftwareRenderer::Unbind() noexcept
{
    ReleaseGpuPresent();
    app_ = nullptr;
    backend_ = nullptr;
    (void)surface_.Resize(0, 0);
}

bool SoftwareRenderer::InitHardwareAcceleratedPresent() noexcept
{
    if (!app_ || !app_->window_handle || !backend_)
        return false;
    if (static_cast<NativePlatform>(app_->native_platform) == NativePlatform::DRM)
        return false;

    ReleaseGpuPresent();
    gpu_ = std::make_unique<GpuPresentState>();
    gpu_->renderer.Bind(*app_);

    int r = gpu_->renderer.CreateBestAvailableExhaustive();
    if (r == static_cast<int>(RendererContextResult::OK) && !gpu_->renderer.SupportsTextureRgbUpload()) {
        gpu_->renderer.Destroy();
        r = gpu_->renderer.CreateBestAvailableForCpuFramebufferPresent();
    }
    if (r != static_cast<int>(RendererContextResult::OK)) {
        ReleaseGpuPresent();
        return false;
    }

    if (gpu_->renderer.LoadShaderCompilerProcs() != static_cast<int>(GpuShaderCompileError::OK)) {
        ReleaseGpuPresent();
        return false;
    }

#if defined(_WIN32)
    if (gpu_->renderer.ActiveBackend() == Renderers::DIRECT3D) {
        if (!InitSoftwareD3d11Blit(*gpu_, *app_))
            ReleaseGpuPresent();
        return gpu_ && gpu_->d3d11_present_path;
    }
#endif

    char log[1024]{};
    GpuObjectName prog = 0;
    const int locs[2] = {0, 1};
    const char * const names[2] = {"a_pos", "a_uv"};
    const int crc = gpu_->renderer.CompileShaderProgram(
        kVsBlit,
        kFsBlit,
        &prog,
        log,
        sizeof(log),
        locs,
        names,
        2);
    if (crc != static_cast<int>(GpuShaderCompileError::OK) || !prog) {
        ReleaseGpuPresent();
        return false;
    }
    gpu_->program = prog;

    GpuObjectName vbo = 0;
    if (gpu_->renderer.CreateBuffer(&vbo) != 0 || !vbo) {
        ReleaseGpuPresent();
        return false;
    }
    gpu_->vbo = vbo;

    const float quad[] = {
        -1.f, -1.f, 0.f, 1.f,
        1.f, -1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 0.f,
        -1.f, -1.f, 0.f, 1.f,
        1.f, 1.f, 1.f, 0.f,
        -1.f, 1.f, 0.f, 0.f,
    };
    if (gpu_->renderer.BindBuffer(GpuBufferTarget::Vertex, gpu_->vbo) != 0
        || gpu_->renderer.UploadBufferData(
               GpuBufferTarget::Vertex, quad, sizeof(quad), GpuBufferUsage::Static) != 0) {
        ReleaseGpuPresent();
        return false;
    }

    GpuObjectName vao = 0;
    if (gpu_->renderer.CreateVertexArray(&vao) != 0 || !vao) {
        ReleaseGpuPresent();
        return false;
    }
    gpu_->vao = vao;
    if (gpu_->renderer.BindVertexArray(gpu_->vao) != 0) {
        ReleaseGpuPresent();
        return false;
    }
    if (gpu_->renderer.BindBuffer(GpuBufferTarget::Vertex, gpu_->vbo) != 0) {
        ReleaseGpuPresent();
        return false;
    }
    if (gpu_->renderer.SetVertexAttributeFloat(0, 2, 4 * sizeof(float), 0, false) != 0
        || gpu_->renderer.SetVertexAttributeFloat(1, 2, 4 * sizeof(float), 2 * sizeof(float), false) != 0) {
        ReleaseGpuPresent();
        return false;
    }
    if (gpu_->renderer.EnableVertexAttribute(0) != 0 || gpu_->renderer.EnableVertexAttribute(1) != 0) {
        ReleaseGpuPresent();
        return false;
    }
    gpu_->renderer.BindVertexArray(0);

    SetSamplerUniformUnit0(gpu_->renderer, gpu_->program);
    return true;
}

bool SoftwareRenderer::UsesHardwareAcceleratedPresent() const noexcept
{
    return static_cast<bool>(gpu_);
}

const char * SoftwareRenderer::PresentBackendLabel() const noexcept
{
    if (gpu_)
        return gpu_->renderer.ActiveBackendLabel();
    return "CPU";
}

bool SoftwareRenderer::EnsureSurface() noexcept
{
    if (!app_)
        return false;
    const int w = EffectiveWidth(*app_);
    const int h = EffectiveHeight(*app_);
    if (w <= 0 || h <= 0)
        return false;
    if (surface_.Width() == w && surface_.Height() == h)
        return surface_.Pixels() != nullptr;
    return surface_.Resize(w, h);
}
void SoftwareRenderer::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!EnsureSurface())
        return;
    surface_.Clear(r, g, b, a);
}
bool SoftwareRenderer::Present() noexcept
{
    if (!app_ || !backend_ || !app_->window_handle)
        return false;
    if (!EnsureSurface())
        return false;

    if (gpu_) {
        const int w = surface_.Width();
        const int h = surface_.Height();
        if (w <= 0 || h <= 0)
            return false;

#if defined(_WIN32)
        if (gpu_->d3d11_present_path)
            return PresentSoftwareD3d11Blit(*gpu_, *app_, surface_);
#endif

        if (gpu_->renderer.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
            return false;

        if (gpu_->tex == 0 || gpu_->texW != w || gpu_->texH != h) {
            if (gpu_->tex)
                gpu_->renderer.DestroyTexture(gpu_->tex);
            gpu_->tex = 0;
            gpu_->texW = w;
            gpu_->texH = h;
            if (gpu_->renderer.CreateTexture2DRgba8(w, h, surface_.Pixels(), &gpu_->tex)
                != static_cast<int>(GpuShaderCompileError::OK))
                return false;
        } else {
            if (gpu_->renderer.UpdateTexture2DRgba8(gpu_->tex, w, h, surface_.Pixels())
                != static_cast<int>(GpuShaderCompileError::OK))
                return false;
        }

        gpu_->renderer.SetDepthTestEnabled(false);
        gpu_->renderer.ClearColorBuffer(0.f, 0.f, 0.f, 1.f);
        gpu_->renderer.SetDefaultViewport();
        gpu_->renderer.BindShaderProgram(gpu_->program);
        gpu_->renderer.BindTexture2D(0, gpu_->tex);
        gpu_->renderer.BindVertexArray(gpu_->vao);
        if (gpu_->renderer.DrawArrays(GpuPrimitive::Triangles, 0, 6) != 0)
            return false;
        gpu_->renderer.BindVertexArray(0);
        gpu_->renderer.BindShaderProgram(0);
        gpu_->renderer.SwapBuffers();
        return true;
    }

    return backend_->PresentSoftwareFramebuffer(
        *app_,
        surface_.Pixels(),
        surface_.Width(),
        surface_.Height(),
        surface_.StrideBytes());
}
bool SoftwareRenderer::IsReady() const noexcept
{
    if (!app_ || !backend_ || !app_->window_handle)
        return false;
    const int w = EffectiveWidth(*app_);
    const int h = EffectiveHeight(*app_);
    if (w <= 0 || h <= 0)
        return false;
    return surface_.Width() == w && surface_.Height() == h && surface_.Pixels() != nullptr;
}

} // namespace HonkordGL::Graphics