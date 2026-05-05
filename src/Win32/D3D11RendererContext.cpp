/**
 * HonkordGL — Direct3D 11 raster attach (Windows)
 * Copyright (C) 2026 Honkord
 *
 * Link: -ld3d11 -ldxgi -ld3dcompiler (MinGW/MSVC)
 */

#include "../internal/D3D11RendererContextWin32.hpp"

#if defined(_WIN32)

#include <HonkordGL/Direct3DIntegration.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/Video.h>

#include "Win32WindowContext.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>

#include <cstring>
#include <utility>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace HonkordGL::Graphics {

namespace {

using Win32::Internal::Win32WindowState;

HWND GetHwnd(ApplicationContextSettings const & app) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32)
        return nullptr;
    auto * const st = reinterpret_cast<Win32WindowState *>(app.window_handle);
    return st ? st->hwnd : nullptr;
}

} // namespace

namespace Internal {

D3D11HonkordRendererState * GetD3D11State(ApplicationContextSettings & app) noexcept
{
    if (app.active_renderer != static_cast<int>(Renderers::DIRECT3D) || !app.renderer_private)
        return nullptr;
    return reinterpret_cast<D3D11HonkordRendererState *>(app.renderer_private);
}

int AttachD3D11(ApplicationContextSettings & app, RendererContextSettings const & spec) noexcept
{
    (void)spec;
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32)
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    HWND const hwnd = GetHwnd(app);
    if (!hwnd) {
        SetInternalApiError("AttachRendererContext (D3D11): no Win32 window.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    auto * const st = new (std::nothrow) D3D11HonkordRendererState();
    if (!st) {
        SetInternalApiError("AttachRendererContext (D3D11): out of memory.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    IDXGIFactory * factory = nullptr;
    if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory))) || !factory) {
        delete st;
        SetInternalApiError("AttachRendererContext (D3D11): CreateDXGIFactory failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }
    st->factory = factory;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL obtained{};
    UINT flags = 0;
    if (FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
            D3D11_SDK_VERSION,
            &st->device,
            &obtained,
            &st->immediate))) {
        factory->Release();
        delete st;
        SetInternalApiError("AttachRendererContext (D3D11): D3D11CreateDevice failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int cw = rc.right > rc.left ? (rc.right - rc.left) : 1;
    const int ch = rc.bottom > rc.top ? (rc.bottom - rc.top) : 1;
    UINT w = app.frame_buffer_width > 0 ? static_cast<UINT>(app.frame_buffer_width) : static_cast<UINT>(cw);
    UINT h = app.frame_buffer_height > 0 ? static_cast<UINT>(app.frame_buffer_height) : static_cast<UINT>(ch);
    st->bbWidth = w;
    st->bbHeight = h;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = w;
    scd.BufferDesc.Height = h;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT const hr = factory->CreateSwapChain(st->device, &scd, &st->swap);
    if (FAILED(hr) || !st->swap) {
        st->immediate->Release();
        st->device->Release();
        delete st;
        SetInternalApiError("AttachRendererContext (D3D11): CreateSwapChain failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    ID3D11Texture2D * bb = nullptr;
    if (FAILED(st->swap->GetBuffer(0, IID_PPV_ARGS(&bb))) || !bb) {
        st->swap->Release();
        st->immediate->Release();
        st->device->Release();
        delete st;
        SetInternalApiError("AttachRendererContext (D3D11): GetBuffer failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }
    if (FAILED(st->device->CreateRenderTargetView(bb, nullptr, &st->rtv))) {
        bb->Release();
        st->swap->Release();
        st->immediate->Release();
        st->device->Release();
        delete st;
        SetInternalApiError("AttachRendererContext (D3D11): CreateRenderTargetView failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }
    bb->Release();

    st->textureSrvs.clear();
    app.renderer_private = reinterpret_cast<HonkordGL_GW_Handle>(st);
    app.device = reinterpret_cast<HonkordGL_GW_Handle>(st->device);
    app.device_context = reinterpret_cast<HonkordGL_GW_Handle>(st->immediate);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(st->swap);
    app.active_renderer = static_cast<int>(Renderers::DIRECT3D);
    app.color_bits = 32;
    app.depth_bits = spec.depth_bits_override > 0 ? spec.depth_bits_override : app.depth_bits;
    app.stencil_bits = spec.stencil_bits_override > 0 ? spec.stencil_bits_override : app.stencil_bits;
    app.msaa_samples = 1;
    app.vsync_enabled = spec.vsync ? 1 : 0;
    app.frame_buffer_width = static_cast<int>(w);
    app.frame_buffer_height = static_cast<int>(h);

    return static_cast<int>(RendererContextResult::OK);
}

void DetachD3D11(ApplicationContextSettings & app) noexcept
{
    auto * const st = reinterpret_cast<D3D11HonkordRendererState *>(app.renderer_private);
    if (!st) {
        app.device = nullptr;
        app.device_context = nullptr;
        app.surface = nullptr;
        app.active_renderer = 0;
        return;
    }
    for (auto & slot : st->textureSrvs) {
        if (slot.srv)
            slot.srv->Release();
        if (slot.tex)
            slot.tex->Release();
        slot.srv = nullptr;
        slot.tex = nullptr;
    }
    st->textureSrvs.clear();
    if (st->rtv)
        st->rtv->Release();
    if (st->swap)
        st->swap->Release();
    if (st->immediate)
        st->immediate->Release();
    if (st->device)
        st->device->Release();
    if (st->factory)
        st->factory->Release();
    delete st;
    app.renderer_private = nullptr;
    app.device = nullptr;
    app.device_context = nullptr;
    app.surface = nullptr;
    app.active_renderer = 0;
}

int RendererContextMakeCurrentD3D11(ApplicationContextSettings &) noexcept
{
    return static_cast<int>(RendererContextResult::OK);
}

void RendererContextSwapBuffersD3D11(ApplicationContextSettings & app) noexcept
{
    auto * const st = GetD3D11State(app);
    if (!st || !st->swap)
        return;
    constexpr UINT kPresentFlags = 0;
    (void)st->swap->Present(app.vsync_enabled ? 1 : 0, kPresentFlags);
}

int D3D11CreateTexture2DRgba8(
    ApplicationContextSettings & app,
    int width,
    int height,
    void const * pixels,
    GpuObjectName * outTexture) noexcept
{
    auto * const st = GetD3D11State(app);
    if (!st || !st->device || !outTexture || width <= 0 || height <= 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outTexture = 0;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = pixels;
    srd.SysMemPitch = static_cast<UINT>(width * 4);
    srd.SysMemSlicePitch = 0;

    ID3D11Texture2D * tex = nullptr;
    HRESULT hr = st->device->CreateTexture2D(&td, pixels ? &srd : nullptr, &tex);
    if (FAILED(hr) || !tex)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    srvd.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView * srv = nullptr;
    hr = st->device->CreateShaderResourceView(tex, &srvd, &srv);
    if (FAILED(hr) || !srv) {
        tex->Release();
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    }

    D3D11TextureSlot slot{};
    slot.tex = tex;
    slot.srv = srv;
    st->textureSrvs.push_back(slot);
    *outTexture = static_cast<GpuObjectName>(st->textureSrvs.size());
    return static_cast<int>(GpuShaderCompileError::OK);
}

int D3D11UpdateTexture2DRgba8(
    ApplicationContextSettings & app,
    GpuObjectName texture,
    int width,
    int height,
    void const * pixels) noexcept
{
    auto * const st = GetD3D11State(app);
    if (!st || !st->immediate || !pixels || width <= 0 || height <= 0 || texture == 0
        || texture > st->textureSrvs.size())
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    auto & slot = st->textureSrvs[static_cast<std::size_t>(texture) - 1];
    if (!slot.tex)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);

    st->immediate->UpdateSubresource(
        slot.tex,
        0,
        nullptr,
        pixels,
        static_cast<UINT>(width * 4),
        static_cast<UINT>(width * height * 4));
    return static_cast<int>(GpuShaderCompileError::OK);
}

void D3D11BindTexture2D(ApplicationContextSettings & app, unsigned textureUnit, GpuObjectName textureName) noexcept
{
    auto * const st = GetD3D11State(app);
    if (!st || !st->immediate || textureName == 0 || textureName > st->textureSrvs.size())
        return;
    ID3D11ShaderResourceView * srv = st->textureSrvs[static_cast<std::size_t>(textureName) - 1].srv;
    if (!srv)
        return;
    st->immediate->PSSetShaderResources(textureUnit, 1, &srv);
}

void D3D11DestroyTexture(ApplicationContextSettings & app, GpuObjectName textureName) noexcept
{
    auto * const st = GetD3D11State(app);
    if (!st || textureName == 0 || textureName > st->textureSrvs.size())
        return;
    auto & slot = st->textureSrvs[static_cast<std::size_t>(textureName) - 1];
    if (slot.srv) {
        slot.srv->Release();
        slot.srv = nullptr;
    }
    if (slot.tex) {
        slot.tex->Release();
        slot.tex = nullptr;
    }
}

} // namespace Internal

int Direct3D11IntegrationGetNativeHandles(ApplicationContextSettings const * app, Direct3D11NativeHandles * out) noexcept
{
    if (!app || !out)
        return static_cast<int>(Direct3DIntegrationResult::INVALID_ARGUMENT);
    auto * const st = Internal::GetD3D11State(const_cast<ApplicationContextSettings &>(*app));
    if (!st)
        return static_cast<int>(Direct3DIntegrationResult::UNSUPPORTED_BACKEND);
    std::memset(out, 0, sizeof(*out));
    out->dxgi_factory = reinterpret_cast<HonkordGL_GW_Handle>(st->factory);
    out->d3d11_device = reinterpret_cast<HonkordGL_GW_Handle>(st->device);
    out->d3d11_immediate_context = reinterpret_cast<HonkordGL_GW_Handle>(st->immediate);
    out->dxgi_swap_chain = reinterpret_cast<HonkordGL_GW_Handle>(st->swap);
    out->d3d11_render_target_view = reinterpret_cast<HonkordGL_GW_Handle>(st->rtv);
    out->backbuffer_width = st->bbWidth;
    out->backbuffer_height = st->bbHeight;
    return static_cast<int>(Direct3DIntegrationResult::OK);
}

} // namespace HonkordGL::Graphics

#endif // defined(_WIN32)
