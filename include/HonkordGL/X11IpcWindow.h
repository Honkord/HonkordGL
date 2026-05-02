/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 helper window for ClientMessage-based IPC
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_IPC_WINDOW_H
#define HONKORDGL_X11_IPC_WINDOW_H

#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics::X11 {

/**
 * Unmapped override-redirect window used as an IPC sink (peers send ClientMessage to `window`).
 * `message_atom` is XInternAtom(message_atom_name); use it when filtering events or matching senders.
 */
HONKORDGL_API struct IpcHelperWindow {
    HonkordGL_GW_Handle display; /**< Display* — not owned, must outlive the helper window */
    HonkordGL_GW_Handle window; /**< X Window ID as handle */
    unsigned long message_atom; /**< X11 Atom value (0 before successful create) */
};

/**
 * Creates a 1×1, unmapped, override-redirect child of the root so the WM does not decorate or map it.
 *
 * @param display_handle  Display* (e.g. WindowBackend::GetDisplay() or ApplicationContextSettings::device_context)
 * @param screen          DefaultScreen index for that display
 * @param message_atom_name  Protocol id; becomes client_message.message_type after XInternAtom
 */
HONKORDGL_API HONKORDGL_ND bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int screen,
    const char * message_atom_name,
    IpcHelperWindow* out) noexcept;

HONKORDGL_API void DestroyIpcHelperWindow(IpcHelperWindow* w) noexcept;

/**
 * XSendEvent ClientMessage, format 32, propagate False, NoEventMask.
 * Peers use the same `message_atom_name` (or matching Atom) as the message type.
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

} // namespace HonkordGL::Graphics::X11

#endif
