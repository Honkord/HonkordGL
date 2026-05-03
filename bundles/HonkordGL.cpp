/**
 * C bundle implementation — thin wrappers around HonkordGL C++ API.
 */

#include "HonkordGL.h"

#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/SoftwareRenderer.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/WindowApplication.h>

#include <new>

using HonkordGL::Events::Event;
using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::SoftwareRenderer;
using HonkordGL::Graphics::WindowBackend;

struct HonkordGL_App {
    WindowBackend backend{};
    ApplicationContextSettings ctx{};
    GpuRenderer gpu;
    SoftwareRenderer sw;
    bool display_initialized{false};
    bool window_created{false};
    bool gpu_initialized{false};

    HonkordGL_App();
};

HonkordGL_App::HonkordGL_App() : gpu(ctx), sw(ctx, backend) {}

namespace {

void CopyEvent(HonkordGL_Event * const out, Event const & e) noexcept
{
    if (!out)
        return;
    out->type = static_cast<uint32_t>(e.type);
    out->key = static_cast<uint32_t>(e.key);
    out->mouse_button = e.mouse_button;
    out->x = e.x;
    out->y = e.y;
    out->width = e.width;
    out->height = e.height;
    out->focused = e.focused;
    out->modifiers = e.modifiers;
    out->character = e.character;
}

} // namespace

HonkordGL_App *HonkordGL_App_Create(void)
{
    try {
        return new (std::nothrow) HonkordGL_App{};
    } catch (...) {
        return nullptr;
    }
}

void HonkordGL_App_Destroy(HonkordGL_App * app)
{
    if (!app)
        return;

    if (app->gpu_initialized) {
        app->gpu.Destroy();
        app->gpu_initialized = false;
    }

    if (app->window_created) {
        app->backend.DestroyWindow(app->ctx);
        app->window_created = false;
    }

    if (app->display_initialized) {
        app->backend.Shutdown();
        app->display_initialized = false;
    }

    delete app;
}

int HonkordGL_InitSubsystem(HonkordGL_App * app, char const * display_or_null)
{
    if (!app)
        return 0;
    if (app->display_initialized)
        return 1;
    if (!app->backend.Initialize(display_or_null))
        return 0;
    app->display_initialized = true;
    return 1;
}

HonkordGL_BackendKind HonkordGL_GetBackendKind(HonkordGL_App const * app)
{
    if (!app)
        return HONKORDGL_BACKEND_UNKNOWN;
    return static_cast<HonkordGL_BackendKind>(static_cast<unsigned char>(app->backend.GetKind()));
}

int HonkordGL_Window_Create(HonkordGL_App * app, char const * title, int x, int y, int width, int height)
{
    if (!app || !title || width <= 0 || height <= 0)
        return 0;

    app->ctx.window_title = title;
    app->ctx.client_rect.x = x;
    app->ctx.client_rect.y = y;
    app->ctx.client_rect.w = width;
    app->ctx.client_rect.z = height;

    if (!app->backend.CreateWindow(app->ctx))
        return 0;
    app->window_created = true;
    return 1;
}

void HonkordGL_Window_Destroy(HonkordGL_App * app)
{
    if (!app || !app->window_created)
        return;

    if (app->gpu_initialized) {
        app->gpu.Destroy();
        app->gpu_initialized = false;
    }

    app->backend.DestroyWindow(app->ctx);
    app->window_created = false;
}

int HonkordGL_PollEvent(HonkordGL_App * app, HonkordGL_Event * out_event)
{
    if (!app || !out_event)
        return 0;

    Event ev{};
    if (!HonkordGL::Events::PollEvent(app->ctx, ev))
        return 0;

    CopyEvent(out_event, ev);
    return 1;
}

void HonkordGL_SetTargetFrameRate(HonkordGL_App * app, int fps)
{
    if (!app)
        return;
    app->backend.SetTargetFrameRate(fps);
}

void HonkordGL_FrameDelay(HonkordGL_App * app)
{
    if (!app)
        return;
    app->backend.DelayFrame();
}

int HonkordGL_GetFramebufferWidth(HonkordGL_App const * app)
{
    if (!app)
        return 0;
    ApplicationContextSettings const & c = app->ctx;
    if (c.frame_buffer_width > 0)
        return c.frame_buffer_width;
    return c.client_rect.w > 0 ? c.client_rect.w : 0;
}

int HonkordGL_GetFramebufferHeight(HonkordGL_App const * app)
{
    if (!app)
        return 0;
    ApplicationContextSettings const & c = app->ctx;
    if (c.frame_buffer_height > 0)
        return c.frame_buffer_height;
    return c.client_rect.z > 0 ? c.client_rect.z : 0;
}

int HonkordGL_Gpu_CreateShaderBackend(HonkordGL_App * app)
{
    if (!app || !app->window_created)
        return static_cast<int>(RendererContextResult::NO_WINDOW);

    app->gpu.Bind(app->ctx);
    int const rc = app->gpu.CreateBestAvailableShaderBackend();
    if (rc == static_cast<int>(RendererContextResult::OK))
        app->gpu_initialized = true;
    return rc;
}

void HonkordGL_Gpu_Destroy(HonkordGL_App * app)
{
    if (!app || !app->gpu_initialized)
        return;
    app->gpu.Destroy();
    app->gpu_initialized = false;
}

void HonkordGL_Gpu_MakeCurrent(HonkordGL_App * app)
{
    if (!app || !app->gpu_initialized)
        return;
    (void)app->gpu.MakeCurrent();
}

void HonkordGL_Gpu_SetDefaultViewport(HonkordGL_App * app)
{
    if (!app || !app->gpu_initialized)
        return;
    app->gpu.SetDefaultViewport();
}

void HonkordGL_Gpu_ClearColor(HonkordGL_App * app, float r, float g, float b, float a)
{
    if (!app || !app->gpu_initialized)
        return;
    app->gpu.ClearColorBuffer(r, g, b, a);
}

void HonkordGL_Gpu_SwapBuffers(HonkordGL_App * app)
{
    if (!app || !app->gpu_initialized)
        return;
    app->gpu.SwapBuffers();
}

char const *HonkordGL_Gpu_BackendLabel(HonkordGL_App const * app)
{
    if (!app || !app->gpu_initialized)
        return "";
    return app->gpu.ActiveBackendLabel();
}

int HonkordGL_Software_EnsureSurface(HonkordGL_App * app)
{
    if (!app || !app->window_created)
        return 0;
    return app->sw.EnsureSurface() ? 1 : 0;
}

uint8_t *HonkordGL_Software_Pixels(HonkordGL_App * app)
{
    if (!app || !app->window_created)
        return nullptr;
    return app->sw.Surface().Pixels();
}

int HonkordGL_Software_Width(HonkordGL_App const * app)
{
    if (!app)
        return 0;
    return app->sw.Surface().Width();
}

int HonkordGL_Software_Height(HonkordGL_App const * app)
{
    if (!app)
        return 0;
    return app->sw.Surface().Height();
}

int HonkordGL_Software_StrideBytes(HonkordGL_App const * app)
{
    if (!app)
        return 0;
    return app->sw.Surface().StrideBytes();
}

void HonkordGL_Software_Clear(HonkordGL_App * app, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!app)
        return;
    app->sw.Clear(r, g, b, a);
}

int HonkordGL_Software_Present(HonkordGL_App * app)
{
    if (!app || !app->window_created)
        return 0;
    return app->sw.Present() ? 1 : 0;
}

int HonkordGL_PresentSoftwareFramebuffer(
    HonkordGL_App * app,
    uint8_t const * rgba_pixels,
    int width,
    int height,
    int stride_bytes)
{
    if (!app || !app->window_created || !rgba_pixels || width <= 0 || height <= 0 || stride_bytes <= 0)
        return 0;
    return app->backend.PresentSoftwareFramebuffer(app->ctx, rgba_pixels, width, height, stride_bytes) ? 1 : 0;
}
