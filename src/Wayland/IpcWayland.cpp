/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland IPC stubs
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/WaylandIpcWindow.h>

#if defined(__linux__) && !defined(__ANDROID__)

#include <HonkordGL/Video.h>

#include <cstring>

namespace HonkordGL::Graphics::Wayland {

bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int,
    const char *,
    IpcHelperWindow * out) noexcept
{
    (void)display_handle;
    if (!out)
        return false;
    std::memset(out, 0, sizeof(*out));
    HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow: Wayland IPC helper not implemented.");
    return false;
}
void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept
{
    if (!w)
        return;
    std::memset(w, 0, sizeof(*w));
}
bool SendIpcClientMessage(
    HonkordGL_GW_Handle,
    HonkordGL_GW_Handle,
    const char *,
    long,
    long,
    long,
    long,
    long) noexcept
{
    HonkordGL::Graphics::SetInternalApiError("SendIpcClientMessage: Wayland IPC not implemented.");
    return false;
}

} // namespace HonkordGL::Graphics::Wayland

#else

namespace HonkordGL::Graphics::Wayland {

bool CreateIpcHelperWindow(HonkordGL_GW_Handle, int, const char *, IpcHelperWindow *) noexcept
{
    return false;
}
void DestroyIpcHelperWindow(IpcHelperWindow *) noexcept {}
bool SendIpcClientMessage(HonkordGL_GW_Handle, HonkordGL_GW_Handle, const char*, long, long, long, long, long) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::Wayland

#endif