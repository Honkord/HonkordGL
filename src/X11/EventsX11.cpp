/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 event translation (keyboard, mouse, window)
    * Copyright (C) 2026 Honkord
    *
    * Link: -lX11
    */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <cstdint>

namespace HonkordGL::Events
{

namespace
{

Window EventWindow(const XEvent& e) noexcept
{
    switch (e.type) {
    case KeyPress:
    case KeyRelease:
        return e.xkey.window;
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
        return e.xbutton.window;
    case EnterNotify:
    case LeaveNotify:
        return e.xcrossing.window;
    case FocusIn:
    case FocusOut:
        return e.xfocus.window;
    case ConfigureNotify:
        return e.xconfigure.window;
    case ClientMessage:
        return e.xclient.window;
    case VisibilityNotify:
        return e.xvisibility.window;
    case MapNotify:
    case UnmapNotify:
        return e.xmap.window;
    default:
        return e.xany.window;
    }
}
Bool IsOurWindow(Display*, XEvent* ev, XPointer arg) noexcept
{
    const auto * target = reinterpret_cast<const Window *>(arg);
    return EventWindow(*ev) == *target ? True : False;
}
unsigned MapModifierState(unsigned state) noexcept
{
    unsigned m = 0;
    if (state & ShiftMask)
        m |= static_cast<unsigned>(ModifierFlags::SHIFT);
    if (state & ControlMask)
        m |= static_cast<unsigned>(ModifierFlags::CTRL);
    if (state & Mod1Mask)
        m |= static_cast<unsigned>(ModifierFlags::ALT);
    if (state & Mod4Mask)
        m |= static_cast<unsigned>(ModifierFlags::SUPER);
    if (state & LockMask)
        m |= static_cast<unsigned>(ModifierFlags::CAPS_LOCK);
    if (state & Mod2Mask)
        m |= static_cast<unsigned>(ModifierFlags::NUM_LOCK);
    return m;
}
KeyCode MapKeysym(KeySym ks) noexcept
{
    switch (ks) {
    case XK_a:
    case XK_A:
        return KeyCode::A;
    case XK_b:
    case XK_B:
        return KeyCode::B;
    case XK_c:
    case XK_C:
        return KeyCode::C;
    case XK_d:
    case XK_D:
        return KeyCode::D;
    case XK_e:
    case XK_E:
        return KeyCode::E;
    case XK_f:
    case XK_F:
        return KeyCode::F;
    case XK_g:
    case XK_G:
        return KeyCode::G;
    case XK_h:
    case XK_H:
        return KeyCode::H;
    case XK_i:
    case XK_I:
        return KeyCode::I;
    case XK_j:
    case XK_J:
        return KeyCode::J;
    case XK_k:
    case XK_K:
        return KeyCode::K;
    case XK_l:
    case XK_L:
        return KeyCode::L;
    case XK_m:
    case XK_M:
        return KeyCode::M;
    case XK_n:
    case XK_N:
        return KeyCode::N;
    case XK_o:
    case XK_O:
        return KeyCode::O;
    case XK_p:
    case XK_P:
        return KeyCode::P;
    case XK_q:
    case XK_Q:
        return KeyCode::Q;
    case XK_r:
    case XK_R:
        return KeyCode::R;
    case XK_s:
    case XK_S:
        return KeyCode::S;
    case XK_t:
    case XK_T:
        return KeyCode::T;
    case XK_u:
    case XK_U:
        return KeyCode::U;
    case XK_v:
    case XK_V:
        return KeyCode::V;
    case XK_w:
    case XK_W:
        return KeyCode::W;
    case XK_x:
    case XK_X:
        return KeyCode::X;
    case XK_y:
    case XK_Y:
        return KeyCode::Y;
    case XK_z:
    case XK_Z:
        return KeyCode::Z;
    case XK_0:
        return KeyCode::NUMBER_0;
    case XK_1:
        return KeyCode::NUMBER_1;
    case XK_2:
        return KeyCode::NUMBER_2;
    case XK_3:
        return KeyCode::NUMBER_3;
    case XK_4:
        return KeyCode::NUMBER_4;
    case XK_5:
        return KeyCode::NUMBER_5;
    case XK_6:
        return KeyCode::NUMBER_6;
    case XK_7:
        return KeyCode::NUMBER_7;
    case XK_8:
        return KeyCode::NUMBER_8;
    case XK_9:
        return KeyCode::NUMBER_9;
    case XK_KP_0:
        return KeyCode::NUMBER_0;
    case XK_KP_1:
        return KeyCode::NUMBER_1;
    case XK_KP_2:
        return KeyCode::NUMBER_2;
    case XK_KP_3:
        return KeyCode::NUMBER_3;
    case XK_KP_4:
        return KeyCode::NUMBER_4;
    case XK_KP_5:
        return KeyCode::NUMBER_5;
    case XK_KP_6:
        return KeyCode::NUMBER_6;
    case XK_KP_7:
        return KeyCode::NUMBER_7;
    case XK_KP_8:
        return KeyCode::NUMBER_8;
    case XK_KP_9:
        return KeyCode::NUMBER_9;
    case XK_F1:
        return KeyCode::F1;
    case XK_F2:
        return KeyCode::F2;
    case XK_F3:
        return KeyCode::F3;
    case XK_F4:
        return KeyCode::F4;
    case XK_F5:
        return KeyCode::F5;
    case XK_F6:
        return KeyCode::F6;
    case XK_F7:
        return KeyCode::F7;
    case XK_F8:
        return KeyCode::F8;
    case XK_F9:
        return KeyCode::F9;
    case XK_F10:
        return KeyCode::F10;
    case XK_F11:
        return KeyCode::F11;
    case XK_F12:
        return KeyCode::F12;
    case XK_F13:
        return KeyCode::F13;
    case XK_F14:
        return KeyCode::F14;
    case XK_F15:
        return KeyCode::F15;
    case XK_F16:
        return KeyCode::F16;
    case XK_F17:
        return KeyCode::F17;
    case XK_F18:
        return KeyCode::F18;
    case XK_F19:
        return KeyCode::F19;
    case XK_F20:
        return KeyCode::F20;
    case XK_Escape:
        return KeyCode::ESCAPE;
    case XK_Return:
    case XK_KP_Enter:
        return KeyCode::ENTER;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return KeyCode::TAB;
    case XK_space:
        return KeyCode::SPACE;
    case XK_Shift_L:
    case XK_Shift_R:
        return KeyCode::SHIFT;
    case XK_Control_L:
    case XK_Control_R:
        return KeyCode::CTRL;
    case XK_Alt_L:
    case XK_Alt_R:
        return KeyCode::ALT;
    case XK_Meta_L:
    case XK_Meta_R:
        return KeyCode::META;
    case XK_Super_L:
    case XK_Super_R:
        return KeyCode::SUPER;
    case XK_Hyper_L:
    case XK_Hyper_R:
        return KeyCode::META;
    case XK_Caps_Lock:
        return KeyCode::CAPS_LOCK;
    case XK_Num_Lock:
        return KeyCode::NUM_LOCK;
    case XK_Print:
        return KeyCode::PRINT_SCREEN;
    case XK_Scroll_Lock:
        return KeyCode::SCROLL_LOCK;
    case XK_Pause:
        return KeyCode::PAUSE;
    case XK_Break:
        return KeyCode::BREAK;
    case XK_Insert:
    case XK_KP_Insert:
        return KeyCode::INSERT;
    case XK_Delete:
    case XK_KP_Delete:
        return KeyCode::DELETE;
    case XK_Home:
    case XK_KP_Home:
        return KeyCode::HOME;
    case XK_End:
    case XK_KP_End:
        return KeyCode::END;
    case XK_Page_Up:
    case XK_KP_Page_Up:
        return KeyCode::PAGE_UP;
    case XK_Page_Down:
    case XK_KP_Page_Down:
        return KeyCode::PAGE_DOWN;
    case XK_Up:
    case XK_KP_Up:
        return KeyCode::UP;
    case XK_Down:
    case XK_KP_Down:
        return KeyCode::DOWN;
    case XK_Left:
    case XK_KP_Left:
        return KeyCode::LEFT;
    case XK_Right:
    case XK_KP_Right:
        return KeyCode::RIGHT;
    case XK_bracketleft:
        return KeyCode::BRACKET_LEFT;
    case XK_bracketright:
        return KeyCode::BRACKET_RIGHT;
    case XK_semicolon:
        return KeyCode::SEMICOLON;
    case XK_comma:
        return KeyCode::COMMA;
    case XK_period:
        return KeyCode::PERIOD;
    case XK_slash:
        return KeyCode::FORWARD_SLASH;
    case XK_KP_Divide:
        return KeyCode::SLASH;
    case XK_backslash:
        return KeyCode::BACKSLASH;
    case XK_asciitilde:
        return KeyCode::TILDE;
    case XK_grave:
        return KeyCode::BACKQUOTE;
    case XK_equal:
    case XK_KP_Equal:
        return KeyCode::EQUAL;
    case XK_minus:
    case XK_KP_Subtract:
        return KeyCode::MINUS;
    case XK_plus:
    case XK_KP_Add:
        return KeyCode::PLUS;
    case XK_asterisk:
    case XK_KP_Multiply:
        return KeyCode::ASTERISK;
    case XK_bar:
        return KeyCode::PIPE;
    case XK_ampersand:
        return KeyCode::AMPERSAND;
    default:
        return KeyCode::NULL_KEY;
    }
}
unsigned CharacterFromKeyEvent(const XKeyEvent& xkey) noexcept
{
    char buf[8];
    KeySym ks;
    const int n = XLookupString(const_cast<XKeyEvent *>(&xkey), buf, sizeof(buf), &ks, nullptr);
    if (n == 1 && static_cast<unsigned char>(buf[0]) < 128u)
        return static_cast<unsigned char>(buf[0]);
    if (ks >= 0x20 && ks <= 0x7eu)
        return static_cast<unsigned>(ks);
    return 0;
}

} // namespace

bool PollEventWayland(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept;
bool PollEventDrm(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept;

bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    const auto plat = static_cast<Graphics::NativePlatform>(ctx.native_platform);
    if (plat == Graphics::NativePlatform::Wayland)
        return PollEventWayland(ctx, out);
    if (plat == Graphics::NativePlatform::DRM)
        return PollEventDrm(ctx, out);
    if (!Graphics::NativePlatformUsesX11ClientHandles(plat))
        return false;

    auto * const dpy = reinterpret_cast<Display *>(ctx.device_context);
    Window win_var = static_cast<Window>(reinterpret_cast<std::uintptr_t>(ctx.window_handle));
    if (!dpy || !win_var)
        return false;

    XEvent xe{};
    for (;;) {
        if (XCheckIfEvent(dpy, &xe, IsOurWindow, reinterpret_cast<XPointer>(&win_var)) == 0)
            return false;

        switch (xe.type) {
        case Expose:
        case GraphicsExpose:
        case NoExpose:
            continue;

        case ConfigureNotify:
            ctx.needs_resize = 1;
            ctx.client_rect.x = xe.xconfigure.x;
            ctx.client_rect.y = xe.xconfigure.y;
            ctx.client_rect.w = xe.xconfigure.width;
            ctx.client_rect.z = xe.xconfigure.height;
            ctx.frame_buffer_width = xe.xconfigure.width;
            ctx.frame_buffer_height = xe.xconfigure.height;
            if (ctx.ResizeCallback)
                ctx.ResizeCallback(&ctx, xe.xconfigure.width, xe.xconfigure.height);
            out.type = EventType::RESIZE;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = 0;
            out.y = 0;
            out.width = xe.xconfigure.width;
            out.height = xe.xconfigure.height;
            out.focused = 0;
            out.modifiers = 0;
            out.character = 0;
            return true;

        case ClientMessage: {
            const Atom wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
            const Atom wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
            if (xe.xclient.message_type == wm_proto && static_cast<Atom>(xe.xclient.data.l[0]) == wm_del) {
                out.type = EventType::QUIT;
                out.key = KeyCode::NULL_KEY;
                out.mouse_button = 0;
                out.x = 0;
                out.y = 0;
                out.width = 0;
                out.height = 0;
                out.focused = 0;
                out.modifiers = 0;
                out.character = 0;
                return true;
            }
            continue;
        }

        case KeyPress: {
            const KeySym ks = XLookupKeysym(&xe.xkey, 0);
            out.type = EventType::KEY_PRESS;
            out.key = MapKeysym(ks);
            out.modifiers = MapModifierState(xe.xkey.state);
            out.x = xe.xkey.x;
            out.y = xe.xkey.y;
            out.width = 0;
            out.height = 0;
            out.mouse_button = 0;
            out.focused = 0;
            out.character = CharacterFromKeyEvent(xe.xkey);
            return true;
        }

        case KeyRelease: {
            const KeySym ks = XLookupKeysym(&xe.xkey, 0);
            out.type = EventType::KEY_RELEASE;
            out.key = MapKeysym(ks);
            out.modifiers = MapModifierState(xe.xkey.state);
            out.x = xe.xkey.x;
            out.y = xe.xkey.y;
            out.width = 0;
            out.height = 0;
            out.mouse_button = 0;
            out.focused = 0;
            out.character = 0;
            return true;
        }

        case MotionNotify:
            out.type = EventType::MOUSE_MOVE;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = xe.xmotion.x;
            out.y = xe.xmotion.y;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = MapModifierState(xe.xmotion.state);
            out.character = 0;
            return true;

        case ButtonPress:
            out.type = EventType::MOUSE_BUTTON_PRESS;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = static_cast<unsigned int>(xe.xbutton.button);
            out.x = xe.xbutton.x;
            out.y = xe.xbutton.y;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = MapModifierState(xe.xbutton.state);
            out.character = 0;
            return true;

        case ButtonRelease:
            out.type = EventType::MOUSE_BUTTON_RELEASE;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = static_cast<unsigned int>(xe.xbutton.button);
            out.x = xe.xbutton.x;
            out.y = xe.xbutton.y;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = MapModifierState(xe.xbutton.state);
            out.character = 0;
            return true;

        case EnterNotify:
            if (xe.xcrossing.detail == NotifyInferior)
                continue;
            out.type = EventType::MOUSE_ENTER;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = xe.xcrossing.x;
            out.y = xe.xcrossing.y;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = MapModifierState(xe.xcrossing.state);
            out.character = 0;
            return true;

        case LeaveNotify:
            if (xe.xcrossing.detail == NotifyInferior)
                continue;
            out.type = EventType::MOUSE_LEAVE;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = xe.xcrossing.x;
            out.y = xe.xcrossing.y;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = MapModifierState(xe.xcrossing.state);
            out.character = 0;
            return true;

        case FocusIn:
            if (xe.xfocus.detail == NotifyPointer)
                continue;
            out.type = EventType::FOCUS;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = 0;
            out.y = 0;
            out.width = 0;
            out.height = 0;
            out.focused = 1;
            out.modifiers = 0;
            out.character = 0;
            return true;

        case FocusOut:
            if (xe.xfocus.detail == NotifyPointer)
                continue;
            out.type = EventType::FOCUS;
            out.key = KeyCode::NULL_KEY;
            out.mouse_button = 0;
            out.x = 0;
            out.y = 0;
            out.width = 0;
            out.height = 0;
            out.focused = 0;
            out.modifiers = 0;
            out.character = 0;
            return true;

        case VisibilityNotify:
            ctx.is_minimized = (xe.xvisibility.state == VisibilityFullyObscured) ? 1 : 0;
            continue;

        case MapNotify:
            ctx.is_visible = 1;
            continue;

        case UnmapNotify:
            ctx.is_visible = 0;
            continue;

        default:
            continue;
        }
    }
}

} // namespace HonkordGL::Events

#elif defined(__APPLE__)

namespace HonkordGL::Events
{

bool PollEventCocoa(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept;
bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    return PollEventCocoa(ctx, out);
}

} // namespace HonkordGL::Events

#elif defined(_WIN32)
// PollEvent for Win32 is implemented in src/Win32/EventsWin32.cpp.

#elif defined(__ANDROID__)
// PollEvent for Android is implemented in src/Android/EventsAndroid.cpp.

#else

namespace HonkordGL::Events
{

bool PollEvent(Graphics::ApplicationContextSettings&, Event&) noexcept
{
    return false;
}

} // namespace HonkordGL::Events

#endif