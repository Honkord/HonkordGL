/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa / AppKit event translation (internal, mirrors EventsX11.cpp)
    * Copyright (C) 2026 Honkord
    *
    * Build: clang++ -std=c++17 -ObjC++ -fobjc-arc (or -fno-objc-arc) -framework Cocoa -framework Carbon
    */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace HonkordGL::Events
{

namespace
{

struct Pending {
    bool close = false;
    bool resize = false;
    bool focus_in = false;
    bool focus_out = false;
};

std::mutex g_mu;
std::unordered_map<uintptr_t, Pending> g_pending;
std::unordered_set<uintptr_t> g_registered;
std::unordered_map<uintptr_t, bool> g_mouse_inside;

void ZeroEvent(Event& o) noexcept
{
    std::memset(&o, 0, sizeof(o));
    o.type = EventType::QUIT;
    o.key = KeyCode::NULL_KEY;
}
unsigned MapNSEventModifiers(NSEvent * e) noexcept
{
    unsigned m = 0;
    const NSEventModifierFlags f = static_cast<NSEventModifierFlags>([e modifierFlags]);
    if (f & NSEventModifierFlagShift)
        m |= static_cast<unsigned>(ModifierFlags::SHIFT);
    if (f & NSEventModifierFlagControl)
        m |= static_cast<unsigned>(ModifierFlags::CTRL);
    if (f & NSEventModifierFlagOption)
        m |= static_cast<unsigned>(ModifierFlags::ALT);
    if (f & NSEventModifierFlagCommand)
        m |= static_cast<unsigned>(ModifierFlags::SUPER);
    if (f & NSEventModifierFlagCapsLock)
        m |= static_cast<unsigned>(ModifierFlags::CAPS_LOCK);
    if (f & NSEventModifierFlagNumericPad)
        m |= static_cast<unsigned>(ModifierFlags::NUM_LOCK);
    return m;
}
KeyCode MapVirtualKey(unsigned short kc) noexcept
{
    switch (kc) {
    case kVK_ANSI_A:
        return KeyCode::A;
    case kVK_ANSI_B:
        return KeyCode::B;
    case kVK_ANSI_C:
        return KeyCode::C;
    case kVK_ANSI_D:
        return KeyCode::D;
    case kVK_ANSI_E:
        return KeyCode::E;
    case kVK_ANSI_F:
        return KeyCode::F;
    case kVK_ANSI_G:
        return KeyCode::G;
    case kVK_ANSI_H:
        return KeyCode::H;
    case kVK_ANSI_I:
        return KeyCode::I;
    case kVK_ANSI_J:
        return KeyCode::J;
    case kVK_ANSI_K:
        return KeyCode::K;
    case kVK_ANSI_L:
        return KeyCode::L;
    case kVK_ANSI_M:
        return KeyCode::M;
    case kVK_ANSI_N:
        return KeyCode::N;
    case kVK_ANSI_O:
        return KeyCode::O;
    case kVK_ANSI_P:
        return KeyCode::P;
    case kVK_ANSI_Q:
        return KeyCode::Q;
    case kVK_ANSI_R:
        return KeyCode::R;
    case kVK_ANSI_S:
        return KeyCode::S;
    case kVK_ANSI_T:
        return KeyCode::T;
    case kVK_ANSI_U:
        return KeyCode::U;
    case kVK_ANSI_V:
        return KeyCode::V;
    case kVK_ANSI_W:
        return KeyCode::W;
    case kVK_ANSI_X:
        return KeyCode::X;
    case kVK_ANSI_Y:
        return KeyCode::Y;
    case kVK_ANSI_Z:
        return KeyCode::Z;
    case kVK_ANSI_0:
        return KeyCode::NUMBER_0;
    case kVK_ANSI_1:
        return KeyCode::NUMBER_1;
    case kVK_ANSI_2:
        return KeyCode::NUMBER_2;
    case kVK_ANSI_3:
        return KeyCode::NUMBER_3;
    case kVK_ANSI_4:
        return KeyCode::NUMBER_4;
    case kVK_ANSI_5:
        return KeyCode::NUMBER_5;
    case kVK_ANSI_6:
        return KeyCode::NUMBER_6;
    case kVK_ANSI_7:
        return KeyCode::NUMBER_7;
    case kVK_ANSI_8:
        return KeyCode::NUMBER_8;
    case kVK_ANSI_9:
        return KeyCode::NUMBER_9;
    case kVK_F1:
        return KeyCode::F1;
    case kVK_F2:
        return KeyCode::F2;
    case kVK_F3:
        return KeyCode::F3;
    case kVK_F4:
        return KeyCode::F4;
    case kVK_F5:
        return KeyCode::F5;
    case kVK_F6:
        return KeyCode::F6;
    case kVK_F7:
        return KeyCode::F7;
    case kVK_F8:
        return KeyCode::F8;
    case kVK_F9:
        return KeyCode::F9;
    case kVK_F10:
        return KeyCode::F10;
    case kVK_F11:
        return KeyCode::F11;
    case kVK_F12:
        return KeyCode::F12;
    case kVK_F13:
        return KeyCode::F13;
    case kVK_F14:
        return KeyCode::F14;
    case kVK_F15:
        return KeyCode::F15;
    case kVK_F16:
        return KeyCode::F16;
    case kVK_F17:
        return KeyCode::F17;
    case kVK_F18:
        return KeyCode::F18;
    case kVK_F19:
        return KeyCode::F19;
    case kVK_F20:
        return KeyCode::F20;
    case kVK_Escape:
        return KeyCode::ESCAPE;
    case kVK_Return:
    case kVK_ANSI_KeypadEnter:
        return KeyCode::ENTER;
    case kVK_Tab:
        return KeyCode::TAB;
    case kVK_Space:
        return KeyCode::SPACE;
    case kVK_Shift:
    case kVK_RightShift:
        return KeyCode::SHIFT;
    case kVK_Control:
    case kVK_RightControl:
        return KeyCode::CTRL;
    case kVK_Option:
    case kVK_RightOption:
        return KeyCode::ALT;
    case kVK_Command:
    case kVK_RightCommand:
        return KeyCode::SUPER;
    case kVK_CapsLock:
        return KeyCode::CAPS_LOCK;
    case kVK_Delete:
        return KeyCode::DELETE;
    case kVK_ForwardDelete:
        return KeyCode::DELETE;
    case kVK_Home:
        return KeyCode::HOME;
    case kVK_End:
        return KeyCode::END;
    case kVK_PageUp:
        return KeyCode::PAGE_UP;
    case kVK_PageDown:
        return KeyCode::PAGE_DOWN;
    case kVK_UpArrow:
        return KeyCode::UP;
    case kVK_DownArrow:
        return KeyCode::DOWN;
    case kVK_LeftArrow:
        return KeyCode::LEFT;
    case kVK_RightArrow:
        return KeyCode::RIGHT;
    case kVK_ANSI_LeftBracket:
        return KeyCode::BRACKET_LEFT;
    case kVK_ANSI_RightBracket:
        return KeyCode::BRACKET_RIGHT;
    case kVK_ANSI_Semicolon:
        return KeyCode::SEMICOLON;
    case kVK_ANSI_Comma:
        return KeyCode::COMMA;
    case kVK_ANSI_Period:
        return KeyCode::PERIOD;
    case kVK_ANSI_Slash:
        return KeyCode::FORWARD_SLASH;
    case kVK_ANSI_Backslash:
        return KeyCode::BACKSLASH;
    case kVK_ANSI_Grave:
        return KeyCode::BACKQUOTE;
    case kVK_ANSI_Equal:
        return KeyCode::EQUAL;
    case kVK_ANSI_Minus:
        return KeyCode::MINUS;
    case kVK_ANSI_KeypadPlus:
        return KeyCode::PLUS;
    case kVK_ANSI_KeypadMultiply:
        return KeyCode::ASTERISK;
    case kVK_ANSI_KeypadDivide:
        return KeyCode::SLASH;
    case kVK_ANSI_KeypadMinus:
        return KeyCode::MINUS;
    default:
        return KeyCode::NULL_KEY;
    }
}
unsigned CharacterFromNSEvent(NSEvent * e) noexcept
{
    NSString* s = [e charactersIgnoringModifiers];
    if (s.length == 0)
        return 0;
    const unichar u = [s characterAtIndex:0];
    return static_cast<unsigned>(u);
}
void ClientPoint(NSWindow * win, NSEvent * e, int& out_x, int& out_y) noexcept
{
    NSView* cv = [win contentView];
    if (!cv) {
        out_x = out_y = 0;
        return;
    }
    const NSPoint p = [e locationInWindow];
    const CGFloat h = cv.bounds.size.height;
    out_x = static_cast<int>(p.x);
    out_y = static_cast<int>(h - p.y);
}
unsigned CocoaToX11StyleMouseButton(NSEvent * e) noexcept
{
    const NSEventType t = static_cast<NSEventType>([e type]);
    if (t == NSEventTypeScrollWheel) {
        const CGFloat dy = [e scrollingDeltaY];
        const CGFloat dx = [e scrollingDeltaX];
        if (dy > 0.0 || dx > 0.0)
            return 4u;
        if (dy < 0.0 || dx < 0.0)
            return 5u;
        return 0u;
    }
    switch ([e buttonNumber]) {
    case 0:
        return 1u;
    case 1:
        return 3u;
    case 2:
        return 2u;
    default:
        return static_cast<unsigned int>([e buttonNumber]) + 1u;
    }
}
bool RefreshBackingSize(NSWindow * win, Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    NSView * cv = [win contentView];
    if (!cv)
        return false;
    const NSSize bs = [win convertSizeToBacking:cv.bounds.size];
    const int nw = static_cast<int>(bs.width);
    const int nh = static_cast<int>(bs.height);
    if (nw == ctx.frame_buffer_width && nh == ctx.frame_buffer_height)
        return false;

    ctx.needs_resize = 1;
    ctx.frame_buffer_width = nw;
    ctx.frame_buffer_height = nh;
    ctx.client_rect.w = nw;
    ctx.client_rect.z = nh;
    const CGFloat scale = win.backingScaleFactor;
    ctx.dpi_x = static_cast<int>(72.0 * scale);
    ctx.dpi_y = static_cast<int>(72.0 * scale);
    if (ctx.ResizeCallback)
        ctx.ResizeCallback(&ctx, nw, nh);

    ZeroEvent(out);
    out.type = EventType::RESIZE;
    out.width = nw;
    out.height = nh;
    return true;
}
void RegisterGlobalTerminate() noexcept
{
    static std::once_flag once;
    std::call_once(once, [] {
        [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillTerminateNotification
                                                            object:nil
                                                             queue:nil
                                                        usingBlock:^(NSNotification*) {
                                                            std::lock_guard<std::mutex> lk(g_mu);
                                                            for (uintptr_t k : g_registered)
                                                                g_pending[k].close = true;
                                                        }];
    });
}
void RegisterWindowObservers(NSWindow * w) noexcept
{
    if (!w)
        return;
    const uintptr_t k = reinterpret_cast<uintptr_t>(w);

    std::lock_guard<std::mutex> lock(g_mu);
    if (g_registered.count(k))
        return;
    g_registered.insert(k);
    g_mouse_inside[k] = false;

    [w setAcceptsMouseMovedEvents:YES];

    NSNotificationCenter * nc = [NSNotificationCenter defaultCenter];
    (void)[nc addObserverForName:NSWindowWillCloseNotification
                          object:w
                           queue:nil
                      usingBlock:^(NSNotification *) {
                          std::lock_guard<std::mutex> lk(g_mu);
                          g_pending[k].close = true;
                      }];
    (void)[nc addObserverForName:NSWindowDidResizeNotification
                          object:w
                           queue:nil
                      usingBlock:^(NSNotification *) {
                          std::lock_guard<std::mutex> lk(g_mu);
                          g_pending[k].resize = true;
                      }];
    (void)[nc addObserverForName:NSWindowDidBecomeKeyNotification
                          object:w
                           queue:nil
                      usingBlock:^(NSNotification *) {
                          std::lock_guard<std::mutex> lk(g_mu);
                          g_pending[k].focus_in = true;
                      }];
    (void)[nc addObserverForName:NSWindowDidResignKeyNotification
                          object:w
                           queue:nil
                      usingBlock:^(NSNotification *) {
                          std::lock_guard<std::mutex> lk(g_mu);
                          g_pending[k].focus_out = true;
                      }];
}
bool DrainPending(uintptr_t k, NSWindow * win, Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    Pending copy{};
    {
        std::lock_guard<std::mutex> lk(g_mu);
        copy = g_pending[k];
        g_pending[k] = Pending{};
    }

    if (copy.close) {
        ZeroEvent(out);
        out.type = EventType::QUIT;
        return true;
    }
    if (copy.resize) {
        return RefreshBackingSize(win, ctx, out);
    }
    if (copy.focus_in) {
        ZeroEvent(out);
        out.type = EventType::FOCUS;
        out.focused = 1;
        return true;
    }
    if (copy.focus_out) {
        ZeroEvent(out);
        out.type = EventType::FOCUS;
        out.focused = 0;
        return true;
    }
    return false;
}
bool EventTargetsWindow(NSEvent * e, NSWindow * win) noexcept
{
    if ([e window] == win)
        return true;
    const NSEventType t = static_cast<NSEventType>([e type]);
    if ([e window] == nil) {
        if (t == NSEventTypeKeyDown || t == NSEventTypeKeyUp || t == NSEventTypeFlagsChanged)
            return [NSApp keyWindow] == win;
    }
    return false;
}
bool FillFromNSEvent(NSEvent * e, NSWindow * win, Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    (void)ctx;
    const NSEventType t = static_cast<NSEventType>([e type]);

    switch (t) {
    case NSEventTypeKeyDown: {
        ZeroEvent(out);
        out.type = EventType::KEY_PRESS;
        out.key = MapVirtualKey(static_cast<unsigned short>([e keyCode]));
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        out.character = CharacterFromNSEvent(e);
        return true;
    }
    case NSEventTypeKeyUp: {
        ZeroEvent(out);
        out.type = EventType::KEY_RELEASE;
        out.key = MapVirtualKey(static_cast<unsigned short>([e keyCode]));
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        return true;
    }
    case NSEventTypeFlagsChanged: {
        ZeroEvent(out);
        out.type = EventType::KEY_PRESS;
        out.key = MapVirtualKey(static_cast<unsigned short>([e keyCode]));
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        return true;
    }
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged: {
        ZeroEvent(out);
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        const uintptr_t k = reinterpret_cast<uintptr_t>(win);
        NSView * cv = [win contentView];
        out.type = EventType::MOUSE_MOVE;
        if (cv) {
            const NSPoint p = [e locationInWindow];
            const bool inside = NSPointInRect(p, cv.bounds);
            std::lock_guard<std::mutex> lk(g_mu);
            bool& was = g_mouse_inside[k];
            if (inside != was) {
                was = inside;
                out.type = inside ? EventType::MOUSE_ENTER : EventType::MOUSE_LEAVE;
            }
        }
        return true;
    }
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown: {
        ZeroEvent(out);
        out.type = EventType::MOUSE_BUTTON_PRESS;
        out.mouse_button = CocoaToX11StyleMouseButton(e);
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        return true;
    }
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp: {
        ZeroEvent(out);
        out.type = EventType::MOUSE_BUTTON_RELEASE;
        out.mouse_button = CocoaToX11StyleMouseButton(e);
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        return true;
    }
    case NSEventTypeScrollWheel: {
        const unsigned btn = CocoaToX11StyleMouseButton(e);
        if (btn == 0u)
            return false;
        ZeroEvent(out);
        out.type = EventType::MOUSE_BUTTON_PRESS;
        out.mouse_button = btn;
        out.modifiers = MapNSEventModifiers(e);
        ClientPoint(win, e, out.x, out.y);
        return true;
    }
    default:
        return false;
    }
}

} // namespace
bool PollEventCocoa(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    @autoreleasepool {
        if (!ctx.window_handle)
            return false;

        NSWindow * const win = reinterpret_cast<NSWindow *>(ctx.window_handle);
        if (!win)
            return false;

        [NSApplication sharedApplication];
        RegisterGlobalTerminate();
        RegisterWindowObservers(win);

        const uintptr_t k = reinterpret_cast<uintptr_t>(win);

        if (DrainPending(k, win, ctx, out))
            return true;

        if (RefreshBackingSize(win, ctx, out))
            return true;

        for (;;) {
            NSEvent * const e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                  untilDate:[NSDate distantPast]
                                                     inMode:NSDefaultRunLoopMode
                                                    dequeue:YES];
            if (!e)
                return false;

            if (!EventTargetsWindow(e, win)) {
                [NSApp sendEvent:e];
                continue;
            }

            if (FillFromNSEvent(e, win, ctx, out))
                return true;
        }
    }
}

} // namespace HonkordGL::Events

#endif /* __APPLE__ */