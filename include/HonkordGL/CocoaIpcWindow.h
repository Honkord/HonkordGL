/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa helper window / notification-based IPC (same-process)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_IPC_WINDOW_H
#define HONKORDGL_COCOA_IPC_WINDOW_H

#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics::Cocoa {

/**
 * Hidden 1×1 panel plus protocol id (`message_atom` = stable hash of `message_atom_name`).
 * Peers use `SendIpcClientMessage` with the same name; delivery is via `NSNotificationCenter`
 * (same application process).
 */
HONKORDGL_API struct IpcHelperWindow {
    HonkordGL_GW_Handle display; /**< NSApplication * — not owned */
    HonkordGL_GW_Handle window; /**< NSPanel * / NSWindow * */
    unsigned long message_atom; /**< Hash of message_atom_name (0 before successful create) */
};

HONKORDGL_API HONKORDGL_ND bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int screen,
    const char * message_atom_name,
    IpcHelperWindow * out) noexcept;

HONKORDGL_API void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept;

/**
 * Posts `NSNotification` named `message_atom_name` with userInfo keys d0..d4 (NSNumber).
 */
HONKORDGL_API HONKORDGL_ND bool SendIpcClientMessage(
    HonkordGL_GW_Handle display_handle,
    HonkordGL_GW_Handle target_window,
    const char * message_atom_name,
    long data0,
    long data1,
    long data2,
    long data3,
    long data4) noexcept;

} // namespace HonkordGL::Graphics::Cocoa

#endif