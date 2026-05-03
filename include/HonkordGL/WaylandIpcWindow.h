/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland IPC helper (layout matches portable IpcHelperWindow)
 * Copyright (C) 2026 Honkord
 *
 * Wayland has no X11-style ClientMessage atoms; helpers are reserved for future
 * fd-based or protocol-specific IPC. Creation currently fails unless extended.
 */

#ifndef HONKORDGL_WAYLAND_IPC_WINDOW_H
#define HONKORDGL_WAYLAND_IPC_WINDOW_H

#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics::Wayland {

HONKORDGL_API struct IpcHelperWindow {
    HonkordGL_GW_Handle display; /**< wl_display* when used */
    HonkordGL_GW_Handle window;  /**< reserved */
    unsigned long message_atom;    /**< reserved (no X11 Atom on Wayland) */
};

HONKORDGL_API HONKORDGL_ND bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int /* screen unused */,
    const char* /* message_atom_name */,
    IpcHelperWindow* out) noexcept;

HONKORDGL_API void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept;

HONKORDGL_API HONKORDGL_ND bool SendIpcClientMessage(
    HonkordGL_GW_Handle display_handle,
    HonkordGL_GW_Handle target_window,
    const char* message_atom_name,
    long data0,
    long data1,
    long data2,
    long data3,
    long data4) noexcept;

} // namespace HonkordGL::Graphics::Wayland

#endif