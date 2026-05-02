/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — DRM/KMS window backend
 * Copyright (C) 2026 Honkord
 */

#include "WindowBackend.hpp"
#include "DRMWindowContext.hpp"

#include <HonkordGL/Events.h>
#include <HonkordGL/Video.h>

#if defined(HONKORDGL_ENABLE_DRM) && defined(__linux__) && !defined(__ANDROID__)

#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cerrno>
#include <cstring>
#include <poll.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace HonkordGL::Graphics::DRM {

namespace {

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Events::KeyCode;
using HonkordGL::Graphics::NativePlatform;
using Internal::DrmWindowState;

void EnqueueResize(DrmWindowState * st, int w, int h) noexcept
{
    if (!st || !st->ctx)
        return;
    st->ctx->client_rect.w = w;
    st->ctx->client_rect.z = h;
    st->ctx->frame_buffer_width = w;
    st->ctx->frame_buffer_height = h;
    st->ctx->needs_resize = 1;
    Event e{};
    e.type = EventType::RESIZE;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = w;
    e.height = h;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    st->pending.push_back(e);
}
drmModeModeInfo const * PickMode(drmModeConnector * conn, int req_w, int req_h) noexcept
{
    if (!conn || conn->count_modes <= 0)
        return nullptr;
    if (req_w > 0 && req_h > 0) {
        for (int i = 0; i < conn->count_modes; ++i) {
            if (conn->modes[i].hdisplay == static_cast<unsigned>(req_w)
                && conn->modes[i].vdisplay == static_cast<unsigned>(req_h))
                return &conn->modes[i];
        }
        drmModeModeInfo const * best = &conn->modes[0];
        unsigned best_area = 0;
        for (int i = 0; i < conn->count_modes; ++i) {
            const unsigned a = conn->modes[i].hdisplay * conn->modes[i].vdisplay;
            if (a > best_area) {
                best_area = a;
                best = &conn->modes[i];
            }
        }
        return best;
    }
    for (int i = 0; i < conn->count_modes; ++i) {
        if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED)
            return &conn->modes[i];
    }
    return &conn->modes[0];
}
uint32_t FindCrtcForConnector(int fd, drmModeRes * res, drmModeConnector * conn) noexcept
{
    if (!res || !conn)
        return 0;
    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder * enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc)
            continue;
        for (int j = 0; j < res->count_crtcs; ++j) {
            if (enc->possible_crtcs & (1u << j)) {
                const uint32_t crtc = res->crtcs[j];
                drmModeFreeEncoder(enc);
                return crtc;
            }
        }
        drmModeFreeEncoder(enc);
    }
    return 0;
}
drmModeConnector * FirstConnectedConnector(int fd, drmModeRes * res) noexcept
{
    if (!res)
        return nullptr;
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector * conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
            return conn;
        drmModeFreeConnector(conn);
    }
    return nullptr;
}
int CountConnectedOutputs(int fd, drmModeRes * res) noexcept
{
    if (!res)
        return 0;
    int n = 0;
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector * conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
            ++n;
        drmModeFreeConnector(conn);
    }
    return n;
}
bool ConnectorBoundsAt(int fd, drmModeRes * res, int index, Recti * out) noexcept
{
    if (!res || !out || index < 0)
        return false;
    int seen = 0;
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector * conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes <= 0) {
            drmModeFreeConnector(conn);
            continue;
        }
        if (seen == index) {
            drmModeModeInfo const * m = PickMode(conn, 0, 0);
            out->x = 0;
            out->y = 0;
            out->w = static_cast<int>(m->hdisplay);
            out->z = static_cast<int>(m->vdisplay);
            drmModeFreeConnector(conn);
            return true;
        }
        ++seen;
        drmModeFreeConnector(conn);
    }
    return false;
}
bool DestroyDumb(int fd, uint32_t handle) noexcept
{
    if (fd < 0 || handle == 0)
        return true;
    drm_mode_destroy_dumb d{};
    d.handle = handle;
    return drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d) == 0;
}

} // namespace

WindowBackend::~WindowBackend()
{
    CloseDisplay();
}
WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : drm_fd_(other.drm_fd_)
    , display_tag_(other.display_tag_)
    , active_(other.active_)
{
    other.drm_fd_ = -1;
    other.display_tag_.fd = -1;
    other.active_ = nullptr;
}
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        drm_fd_ = other.drm_fd_;
        display_tag_ = other.display_tag_;
        active_ = other.active_;
        other.drm_fd_ = -1;
        other.display_tag_.fd = -1;
        other.active_ = nullptr;
    }
    return *this;
}
bool WindowBackend::OpenDisplay(const char * device_path) noexcept
{
    if (drm_fd_ >= 0)
        return true;
    const char * path = "/dev/dri/card0";
    if (device_path && device_path[0] != '\0')
        path = device_path;
    const int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        HonkordGL::Graphics::SetInternalApiError("DRM: could not open KMS device (open failed).");
        return false;
    }
    (void)drmSetMaster(fd);
    drm_fd_ = fd;
    display_tag_.fd = fd;
    return true;
}
void WindowBackend::CloseDisplay() noexcept
{
    if (active_ && active_->ctx)
        DestroyWindow(*active_->ctx);
    if (drm_fd_ >= 0) {
        drmDropMaster(drm_fd_);
        close(drm_fd_);
        drm_fd_ = -1;
        display_tag_.fd = -1;
    }
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    if (active_) {
        HonkordGL::Graphics::SetInternalApiError("DRM: only one scanout surface per display connection.");
        return false;
    }
    if (drm_fd_ < 0 && !OpenDisplay(nullptr))
        return false;

    drmModeRes * res = drmModeGetResources(drm_fd_);
    if (!res) {
        HonkordGL::Graphics::SetInternalApiError("DRM: drmModeGetResources failed.");
        return false;
    }

    drmModeConnector * conn = FirstConnectedConnector(drm_fd_, res);
    if (!conn) {
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: no connected display with modes.");
        return false;
    }

    const int req_w = ctx.client_rect.w;
    const int req_h = ctx.client_rect.z;
    drmModeModeInfo const * mode = PickMode(conn, req_w, req_h);
    if (!mode) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: no video mode.");
        return false;
    }

    const uint32_t crtc_id = FindCrtcForConnector(drm_fd_, res, conn);
    if (crtc_id == 0) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: could not find a CRTC for the connector.");
        return false;
    }

    const uint32_t w = mode->hdisplay;
    const uint32_t h = mode->vdisplay;

    drm_mode_create_dumb create{};
    create.width = w;
    create.height = h;
    create.bpp = 32;
    if (drmIoctl(drm_fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: MODE_CREATE_DUMB failed.");
        return false;
    }

    uint32_t fb_id = 0;
    if (drmModeAddFB(drm_fd_, w, h, 24, 32, create.pitch, create.handle, &fb_id) != 0) {
        DestroyDumb(drm_fd_, create.handle);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: drmModeAddFB failed.");
        return false;
    }

    const uint32_t connector_id = conn->connector_id;
    uint32_t connectors_arg[1] = {connector_id};
    if (drmModeSetCrtc(drm_fd_, crtc_id, fb_id, 0, 0, connectors_arg, 1, const_cast<drmModeModeInfo *>(mode)) != 0) {
        drmModeRmFB(drm_fd_, fb_id);
        DestroyDumb(drm_fd_, create.handle);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        HonkordGL::Graphics::SetInternalApiError("DRM: drmModeSetCrtc failed (need DRM master / no conflicting session).");
        return false;
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    auto * const st = new DrmWindowState;
    st->ctx = &ctx;
    st->drm_fd = drm_fd_;
    st->crtc_id = crtc_id;
    st->connector_id = connector_id;
    st->fb_id = fb_id;
    st->dumb_handle = create.handle;
    st->width = w;
    st->height = h;
    st->pitch = create.pitch;
    active_ = st;

    ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>(st);
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(&display_tag_);
    ctx.client_rect.x = 0;
    ctx.client_rect.y = 0;
    ctx.client_rect.w = static_cast<int>(w);
    ctx.client_rect.z = static_cast<int>(h);
    ctx.is_visible = 1;
    ctx.is_minimized = 0;
    ctx.is_fullscreen = 1;
    ctx.dpi_x = 96;
    ctx.dpi_y = 96;
    ctx.frame_buffer_width = static_cast<int>(w);
    ctx.frame_buffer_height = static_cast<int>(h);
    ctx.needs_resize = 0;
    ctx.native_platform = static_cast<int>(NativePlatform::DRM);
    if (ctx.color_bits <= 0)
        ctx.color_bits = 32;
    if (ctx.depth_bits <= 0)
        ctx.depth_bits = 24;

    EnqueueResize(st, static_cast<int>(w), static_cast<int>(h));
    return true;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<DrmWindowState *>(ctx.window_handle);
    if (!st || st != active_)
        return;

    if (st->drm_fd >= 0) {
        if (st->fb_id != 0)
            drmModeRmFB(st->drm_fd, st->fb_id);
        DestroyDumb(st->drm_fd, st->dumb_handle);
    }

    delete st;
    active_ = nullptr;
    ctx.window_handle = nullptr;
    ctx.device_context = nullptr;
    ctx.native_platform = 0;
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<DrmWindowState *>(ctx.window_handle);
    if (!st || st != active_ || st->drm_fd < 0)
        return false;

    pollfd pfd{};
    pfd.fd = st->drm_fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        drmEventContext evctx{};
        evctx.version = DRM_EVENT_CONTEXT_VERSION;
        (void)drmHandleEvent(st->drm_fd, &evctx);
    }

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == HonkordGL::Events::EventType::QUIT)
            return false;
    }
    return true;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings&, const char *) noexcept {}

void WindowBackend::ApplyWindowSettings(ApplicationContextSettings&) noexcept {}

void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    if (drm_fd_ >= 0)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(
            const_cast<DrmDisplayTag *>(&display_tag_));
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow *, const char *) const noexcept
{
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    if (drm_fd_ < 0)
        return nullptr;
    return const_cast<DrmDisplayTag*>(&display_tag_);
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    return false;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (drm_fd_ < 0)
        return false;
    pollfd pfd{};
    pfd.fd = drm_fd_;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        drmEventContext evctx{};
        evctx.version = DRM_EVENT_CONTEXT_VERSION;
        (void)drmHandleEvent(drm_fd_, &evctx);
        return true;
    }
    return false;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (drm_fd_ < 0)
        return 0;
    drmModeRes * res = drmModeGetResources(drm_fd_);
    if (!res)
        return 0;
    const int n = CountConnectedOutputs(drm_fd_, res);
    drmModeFreeResources(res);
    return n;
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (drm_fd_ < 0 || !out)
        return false;
    drmModeRes * res = drmModeGetResources(drm_fd_);
    if (!res)
        return false;
    const bool ok = ConnectorBoundsAt(drm_fd_, res, index, out);
    drmModeFreeResources(res);
    return ok;
}

void WindowBackend::SetCursorKind(ApplicationContextSettings&, CursorKind) noexcept {}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings&, const CursorImageParams&) noexcept
{
    return false;
}
void WindowBackend::ResetCursor(ApplicationContextSettings&) noexcept {}

} // namespace HonkordGL::Graphics::DRM

#elif defined(__linux__) && !defined(__ANDROID__)

namespace HonkordGL::Graphics::DRM {
// DRM backend omitted (define HONKORDGL_ENABLE_DRM and link libdrm).
}

#endif