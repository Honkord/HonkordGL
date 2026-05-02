/**
    * @author Honkord <https://github.com>

    Copyright (C) 2026 Honkord
    (See LICENSE.md for what this file is licensed under)
*/

#ifndef __HonkordGL_Events
#define __HonkordGL_Events

#include <HonkordGL/Config.h>

namespace HonkordGL
{
namespace Graphics
{
struct ApplicationContextSettings;
}

namespace Events
{
    HONKORDGL_API enum class KeyCode : unsigned int {
        NULL_KEY = 0,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        NUMBER_0,
        NUMBER_1,
        NUMBER_2,
        NUMBER_3,
        NUMBER_4,
        NUMBER_5,
        NUMBER_6,
        NUMBER_7,
        NUMBER_8,
        NUMBER_9,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        ESCAPE,
        ENTER,
        TAB,
        SPACE,
        SHIFT,
        CTRL,
        ALT,
        SUPER,
        META,
        CAPS_LOCK,
        NUM_LOCK,
        PRINT_SCREEN,
        SCROLL_LOCK,
        PAUSE,
        BREAK,
        INSERT,
        DELETE,
        HOME,
        END,
        PAGE_UP,
        PAGE_DOWN,
        UP,
        DOWN,
        LEFT,
        RIGHT,
        BRACKET_LEFT,
        BRACKET_RIGHT,
        SEMICOLON,
        COMMA,
        PERIOD,
        SLASH,
        BACKSLASH,
        TILDE,
        BACKQUOTE,
        EQUAL,
        MINUS,
        PLUS,
        ASTERISK,
        FORWARD_SLASH,
        BACKWARD_SLASH,
        PIPE,
        AMPERSAND,
    };

    HONKORDGL_API enum class EventType : unsigned int {
        QUIT,
        RESIZE,
        FOCUS,
        MOUSE_ENTER,
        MOUSE_LEAVE,
        MOUSE_MOVE,
        MOUSE_BUTTON_PRESS,
        MOUSE_BUTTON_RELEASE,
        KEY_PRESS,
        KEY_RELEASE,
    };

    /** Bitmask: SHIFT, CTRL, ALT, SUPER, CAPS_LOCK, NUM_LOCK */
    HONKORDGL_API enum class ModifierFlags : unsigned int {
        SHIFT = 1u << 0,
        CTRL = 1u << 1,
        ALT = 1u << 2,
        SUPER = 1u << 3,
        CAPS_LOCK = 1u << 4,
        NUM_LOCK = 1u << 5,
    };

    HONKORDGL_API struct Event {
        EventType type;
        KeyCode key;
        /** X11: 1 = left, 2 = middle, 3 = right, 4 = wheel up, 5 = wheel down */
        unsigned int mouse_button;
        /** Client-area coordinates (relative to window) */
        int x;
        int y;
        /** RESIZE: new drawable size */
        int width;
        int height;
        /** FOCUS: 1 = gained, 0 = lost */
        int focused;
        unsigned int modifiers;
        /** UTF-32 code point when available from key event, else 0 */
        unsigned int character;
    };

    /**
     * Dequeues the next event for the window in `ctx` (non-blocking).
     * Updates `ctx` on configure/resize.
     * - X11: `ctx.device_context` = Display *, `ctx.window_handle` = X Window (uintptr_t).
     * - Wayland: `ctx.device_context` = wl_display *, `ctx.window_handle` = internal window context (opaque).
     * - macOS: `ctx.window_handle` = NSWindow *; `device_context` unused for polling.
     * - Win32: `ctx.device_context` = HINSTANCE; `ctx.window_handle` = internal window state (opaque).
     * - Android: `ctx.device_context` = `android_app *`; `ctx.window_handle` = internal window state (opaque).
     * - DRM: `ctx.device_context` = internal tag with DRM fd; `ctx.window_handle` = internal surface state (opaque).
     * - NetBSD / FreeBSD / Raspberry Pi (X11 driver): same as X11 (`Display *`, X window id as uintptr in `window_handle`); `native_platform` matches the active driver.
     */
    HONKORDGL_API HONKORDGL_ND bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept;

} /* namespace Events */

} /* namespace HonkordGL */

#endif