/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 IPC helper window
    * Copyright (C) 2026 Honkord
    *
    * Link: -lX11
    */

#include <HonkordGL/X11IpcWindow.h>

#include <cstring>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <X11/Xlib.h>

#include <cstdint>

namespace HonkordGL::Graphics::X11
{

bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int screen,
    const char * message_atom_name,
    IpcHelperWindow * out) noexcept
{
    if (!out || !display_handle || !message_atom_name || message_atom_name[0] == '\0')
        return false;

    std::memset(out, 0, sizeof(*out));

    auto * const dpy = reinterpret_cast<Display*>(display_handle);
    if (screen < 0 || screen >= ScreenCount(dpy))
        return false;

    const Window root = RootWindow(dpy, screen);
    const int depth = DefaultDepth(dpy, screen);
    Visual * const visual = DefaultVisual(dpy, screen);

    XSetWindowAttributes swa{};
    swa.override_redirect = True;
    swa.event_mask = StructureNotifyMask;

    const Window win = XCreateWindow(
        dpy,
        root,
        -200,
        -200,
        1,
        1,
        0,
        depth,
        InputOutput,
        visual,
        CWOverrideRedirect | CWEventMask,
        &swa);

    if (!win)
        return false;

    const Atom atom = XInternAtom(dpy, message_atom_name, False);

    out->display = display_handle;
    out->window = reinterpret_cast<HonkordGL_GW_Handle>(static_cast<std::uintptr_t>(win));
    out->message_atom = static_cast<unsigned long>(atom);
    return true;
}

void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept
{
    if (!w || !w->display || !w->window)
        return;

    auto * const dpy = reinterpret_cast<Display*>(w->display);
    const Window win = static_cast<Window>(reinterpret_cast<std::uintptr_t>(w->window));
    XDestroyWindow(dpy, win);
    XFlush(dpy);

    w->window = nullptr;
    w->message_atom = 0;
}

bool SendIpcClientMessage(
    HonkordGL_GW_Handle display_handle,
    HonkordGL_GW_Handle target_window,
    const char * message_atom_name,
    long data0,
    long data1,
    long data2,
    long data3,
    long data4) noexcept
{
    if (!display_handle || !target_window || !message_atom_name || message_atom_name[0] == '\0')
        return false;

    auto * const dpy = reinterpret_cast<Display*>(display_handle);
    const Window target = static_cast<Window>(reinterpret_cast<std::uintptr_t>(target_window));

    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.serial = 0;
    ev.xclient.send_event = True;
    ev.xclient.display = dpy;
    ev.xclient.window = target;
    ev.xclient.message_type = XInternAtom(dpy, message_atom_name, False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = data0;
    ev.xclient.data.l[1] = data1;
    ev.xclient.data.l[2] = data2;
    ev.xclient.data.l[3] = data3;
    ev.xclient.data.l[4] = data4;

    const Status st = XSendEvent(dpy, target, False, NoEventMask, &ev);
    XFlush(dpy);
    return st != 0;
}

} // namespace HonkordGL::Graphics::X11

#else

namespace HonkordGL::Graphics::X11
{

bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle,
    int,
    const char *,
    IpcHelperWindow * out) noexcept
{
    if (out)
        std::memset(out, 0, sizeof(*out));
    return false;
}

void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept
{
    if (w)
        std::memset(w, 0, sizeof(*w));
}

bool SendIpcClientMessage(
    HonkordGL_GW_Handle,
    HonkordGL_GW_Handle,
    const char*,
    long,
    long,
    long,
    long,
    long) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::X11

#endif