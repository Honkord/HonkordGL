/**
 * HonkordGL — internal D3D11 raster attach (Windows only). Not public API.
 */

#pragma once

#if defined(_WIN32)

#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace HonkordGL::Graphics {

struct D3D11TextureSlot {
    ID3D11Texture2D * tex{};
    ID3D11ShaderResourceView * srv{};
};

struct D3D11HonkordRendererState {
    IDXGIFactory * factory{};
    ID3D11Device * device{};
    ID3D11DeviceContext * immediate{};
    IDXGISwapChain * swap{};
    ID3D11RenderTargetView * rtv{};
    unsigned int bbWidth{};
    unsigned int bbHeight{};
    std::vector<D3D11TextureSlot> textureSrvs{};
};

namespace Internal {

int AttachD3D11(ApplicationContextSettings & app, RendererContextSettings const & spec) noexcept;
void DetachD3D11(ApplicationContextSettings & app) noexcept;
int RendererContextMakeCurrentD3D11(ApplicationContextSettings & app) noexcept;
void RendererContextSwapBuffersD3D11(ApplicationContextSettings & app) noexcept;

int D3D11CreateTexture2DRgba8(
    ApplicationContextSettings & app,
    int width,
    int height,
    void const * pixels,
    GpuObjectName * outTexture) noexcept;
int D3D11UpdateTexture2DRgba8(
    ApplicationContextSettings & app,
    GpuObjectName texture,
    int width,
    int height,
    void const * pixels) noexcept;
void D3D11BindTexture2D(ApplicationContextSettings & app, unsigned textureUnit, GpuObjectName textureName) noexcept;
void D3D11DestroyTexture(ApplicationContextSettings & app, GpuObjectName textureName) noexcept;

D3D11HonkordRendererState * GetD3D11State(ApplicationContextSettings & app) noexcept;

} // namespace Internal
} // namespace HonkordGL::Graphics

#endif // defined(_WIN32)