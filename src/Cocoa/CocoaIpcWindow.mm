/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa IPC helper (NSNotificationCenter, same process)
    * Copyright (C) 2026 Honkord
    */

#include <HonkordGL/CocoaIpcWindow.h>

#include <cstring>

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include <cstdint>

namespace HonkordGL::Graphics::Cocoa
{

namespace {

unsigned long HashAtomName(const char * s) noexcept
{
    unsigned long h = 5381u;
    if (!s)
        return 0;
    for (; *s; ++s)
        h = ((h << 5u) + h) + static_cast<unsigned char>(*s);
    return h;
}

} // namespace

bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle display_handle,
    int screen,
    const char * message_atom_name,
    IpcHelperWindow * out) noexcept
{
    (void)screen;
    if (!out || !display_handle || !message_atom_name || message_atom_name[0] == '\0')
        return false;

    std::memset(out, 0, sizeof(*out));

    @autoreleasepool {
        NSPanel * const panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(-2000.0, -2000.0, 1.0, 1.0)
                                                           styleMask:NSWindowStyleMaskBorderless
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
        if (!panel)
            return false;
        [panel setOpaque:NO];
        [panel setBackgroundColor:[NSColor clearColor]];
        [panel setReleasedWhenClosed:NO];
        [panel orderOut:nil];

        out->display = display_handle;
        out->window = reinterpret_cast<HonkordGL_GW_Handle>((__bridge_retained void *)panel);
        out->message_atom = HashAtomName(message_atom_name);
        return true;
    }
}
void DestroyIpcHelperWindow(IpcHelperWindow * w) noexcept
{
    if (!w || !w->window)
        return;
    @autoreleasepool {
        NSPanel * const panel = (__bridge_transfer NSPanel *)w->window;
        w->window = nullptr;
        w->message_atom = 0;
        [panel close];
    }
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
    (void)display_handle;
    (void)target_window;
    if (!message_atom_name || message_atom_name[0] == '\0')
        return false;
    @autoreleasepool {
        NSString * const name = [NSString stringWithUTF8String:message_atom_name];
        if (!name)
            return false;
        NSDictionary * const userInfo = @{
            @"d0" : @(data0),
            @"d1" : @(data1),
            @"d2" : @(data2),
            @"d3" : @(data3),
            @"d4" : @(data4),
        };
        [[NSNotificationCenter defaultCenter] postNotificationName:name object:nil userInfo:userInfo];
        return true;
    }
}

} // namespace HonkordGL::Graphics::Cocoa

#else

namespace HonkordGL::Graphics::Cocoa
{

bool CreateIpcHelperWindow(
    HonkordGL_GW_Handle,
    int,
    const char *,
    IpcHelperWindow *) noexcept
{
    return false;
}
void DestroyIpcHelperWindow(IpcHelperWindow *) noexcept {}
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
    return false;
}

} // namespace HonkordGL::Graphics::Cocoa

#endif
