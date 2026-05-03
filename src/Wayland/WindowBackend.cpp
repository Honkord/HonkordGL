/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland window backend
 * Copyright (C) 2026 Honkord
 */

#if defined(__linux__) && !defined(__ANDROID__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "WindowBackend.hpp"
#include "PlatformSession.hpp"
#include "WaylandWindowContext.hpp"

#include "generated/xdg-shell-client-protocol.h"

#include <HonkordGL/Events.h>
#include <HonkordGL/Video.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#if defined(__linux__) && !defined(__ANDROID__)

#include <linux/input-event-codes.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdlib>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <memory>
#include <vector>

namespace HonkordGL::Graphics::Wayland {

namespace detail {

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Events::KeyCode;
using HonkordGL::Events::ModifierFlags;

struct WaylandBackendState {
    struct OutputSlot {
        WaylandBackendState * owner{nullptr};
        wl_output * output{nullptr};
        uint32_t global_name{0};
        int x{0};
        int y{0};
        int width{0};
        int height{0};
        bool has_geometry{false};
        bool has_mode{false};
    };

    wl_display * display{nullptr};
    bool owns_display{false};
    wl_registry * registry{nullptr};
    wl_compositor * compositor{nullptr};
    xdg_wm_base * wm_base{nullptr};
    wl_seat * seat{nullptr};
    wl_pointer * pointer{nullptr};
    wl_keyboard * keyboard{nullptr};
    wl_shm * shm{nullptr};

    wl_surface * pointer_focus{nullptr};
    wl_surface * keyboard_focus{nullptr};
    int pointer_x{0};
    int pointer_y{0};
    uint32_t pointer_serial{0};

    wl_cursor_theme * cursor_theme{nullptr};
    wl_surface * cursor_surface{nullptr};

    bool ext_xkb{true};
    bool ext_xrandr{true};
    bool ext_xinput2{false};
    int xi_opcode{0};
    int xi2_major{0};
    int xi2_minor{0};

    bool monitor_poll_enabled{false};
    bool monitors_dirty{false};

    std::vector<std::unique_ptr<OutputSlot>> output_slots;

    WindowBackend * self{nullptr};
};

constexpr int kDefaultDpi = 96;

static Internal::WaylandWindowContext * WctxFromSurface(wl_surface * surf) noexcept
{
    if (!surf)
        return nullptr;
    return static_cast<Internal::WaylandWindowContext *>(wl_proxy_get_user_data(reinterpret_cast<wl_proxy *>(surf)));
}
static unsigned MapModifierWayland(uint32_t depressed) noexcept
{
    unsigned m = 0;
    if (depressed & 0x01)
        m |= static_cast<unsigned>(ModifierFlags::SHIFT);
    if (depressed & 0x04)
        m |= static_cast<unsigned>(ModifierFlags::CTRL);
    if (depressed & 0x08)
        m |= static_cast<unsigned>(ModifierFlags::ALT);
    if (depressed & 0x40)
        m |= static_cast<unsigned>(ModifierFlags::SUPER);
    return m;
}
static KeyCode MapLinuxEvdev(unsigned evdev) noexcept
{
    switch (evdev) {
    case KEY_A:
        return KeyCode::A;
    case KEY_B:
        return KeyCode::B;
    case KEY_C:
        return KeyCode::C;
    case KEY_D:
        return KeyCode::D;
    case KEY_E:
        return KeyCode::E;
    case KEY_F:
        return KeyCode::F;
    case KEY_G:
        return KeyCode::G;
    case KEY_H:
        return KeyCode::H;
    case KEY_I:
        return KeyCode::I;
    case KEY_J:
        return KeyCode::J;
    case KEY_K:
        return KeyCode::K;
    case KEY_L:
        return KeyCode::L;
    case KEY_M:
        return KeyCode::M;
    case KEY_N:
        return KeyCode::N;
    case KEY_O:
        return KeyCode::O;
    case KEY_P:
        return KeyCode::P;
    case KEY_Q:
        return KeyCode::Q;
    case KEY_R:
        return KeyCode::R;
    case KEY_S:
        return KeyCode::S;
    case KEY_T:
        return KeyCode::T;
    case KEY_U:
        return KeyCode::U;
    case KEY_V:
        return KeyCode::V;
    case KEY_W:
        return KeyCode::W;
    case KEY_X:
        return KeyCode::X;
    case KEY_Y:
        return KeyCode::Y;
    case KEY_Z:
        return KeyCode::Z;
    case KEY_1:
        return KeyCode::NUMBER_1;
    case KEY_2:
        return KeyCode::NUMBER_2;
    case KEY_3:
        return KeyCode::NUMBER_3;
    case KEY_4:
        return KeyCode::NUMBER_4;
    case KEY_5:
        return KeyCode::NUMBER_5;
    case KEY_6:
        return KeyCode::NUMBER_6;
    case KEY_7:
        return KeyCode::NUMBER_7;
    case KEY_8:
        return KeyCode::NUMBER_8;
    case KEY_9:
        return KeyCode::NUMBER_9;
    case KEY_0:
        return KeyCode::NUMBER_0;
    case KEY_F1:
        return KeyCode::F1;
    case KEY_F2:
        return KeyCode::F2;
    case KEY_F3:
        return KeyCode::F3;
    case KEY_F4:
        return KeyCode::F4;
    case KEY_F5:
        return KeyCode::F5;
    case KEY_F6:
        return KeyCode::F6;
    case KEY_F7:
        return KeyCode::F7;
    case KEY_F8:
        return KeyCode::F8;
    case KEY_F9:
        return KeyCode::F9;
    case KEY_F10:
        return KeyCode::F10;
    case KEY_F11:
        return KeyCode::F11;
    case KEY_F12:
        return KeyCode::F12;
    case KEY_ESC:
        return KeyCode::ESCAPE;
    case KEY_ENTER:
        return KeyCode::ENTER;
    case KEY_TAB:
        return KeyCode::TAB;
    case KEY_SPACE:
        return KeyCode::SPACE;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        return KeyCode::SHIFT;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        return KeyCode::CTRL;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        return KeyCode::ALT;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
        return KeyCode::META;
    case KEY_CAPSLOCK:
        return KeyCode::CAPS_LOCK;
    case KEY_NUMLOCK:
        return KeyCode::NUM_LOCK;
    case KEY_PRINT:
        return KeyCode::PRINT_SCREEN;
    case KEY_SCROLLLOCK:
        return KeyCode::SCROLL_LOCK;
    case KEY_PAUSE:
        return KeyCode::PAUSE;
    case KEY_INSERT:
        return KeyCode::INSERT;
    case KEY_DELETE:
        return KeyCode::DELETE;
    case KEY_HOME:
        return KeyCode::HOME;
    case KEY_END:
        return KeyCode::END;
    case KEY_PAGEUP:
        return KeyCode::PAGE_UP;
    case KEY_PAGEDOWN:
        return KeyCode::PAGE_DOWN;
    case KEY_UP:
        return KeyCode::UP;
    case KEY_DOWN:
        return KeyCode::DOWN;
    case KEY_LEFT:
        return KeyCode::LEFT;
    case KEY_RIGHT:
        return KeyCode::RIGHT;
    case KEY_LEFTBRACE:
        return KeyCode::BRACKET_LEFT;
    case KEY_RIGHTBRACE:
        return KeyCode::BRACKET_RIGHT;
    case KEY_SEMICOLON:
        return KeyCode::SEMICOLON;
    case KEY_COMMA:
        return KeyCode::COMMA;
    case KEY_DOT:
        return KeyCode::PERIOD;
    case KEY_SLASH:
        return KeyCode::FORWARD_SLASH;
    case KEY_BACKSLASH:
        return KeyCode::BACKSLASH;
    case KEY_GRAVE:
        return KeyCode::BACKQUOTE;
    case KEY_EQUAL:
        return KeyCode::EQUAL;
    case KEY_MINUS:
        return KeyCode::MINUS;
    default:
        return KeyCode::NULL_KEY;
    }
}
static unsigned WaylandButtonToHonkord(unsigned btn) noexcept
{
    if (btn == BTN_LEFT)
        return 1u;
    if (btn == BTN_MIDDLE)
        return 2u;
    if (btn == BTN_RIGHT)
        return 3u;
    return btn;
}

using HonkordGL::Graphics::CursorKind;

static char const * XcursorNameForKind(CursorKind kind) noexcept
{
    switch (kind) {
    case CursorKind::Arrow:
    case CursorKind::ArrowWait:
    case CursorKind::Help:
        return "left_ptr";
    case CursorKind::Wait:
        return "watch";
    case CursorKind::Text:
        return "text";
    case CursorKind::Hand:
        return "hand2";
    case CursorKind::SizeHorizontal:
    case CursorKind::SizeLeft:
    case CursorKind::SizeRight:
        return "sb_h_double_arrow";
    case CursorKind::SizeVertical:
    case CursorKind::SizeTop:
    case CursorKind::SizeBottom:
        return "sb_v_double_arrow";
    case CursorKind::SizeTopLeftBottomRight:
    case CursorKind::SizeTopLeft:
    case CursorKind::SizeBottomRight:
        return "bd_double_arrow";
    case CursorKind::SizeBottomLeftTopRight:
    case CursorKind::SizeBottomLeft:
    case CursorKind::SizeTopRight:
        return "fd_double_arrow";
    case CursorKind::SizeAll:
        return "fleur";
    case CursorKind::Cross:
        return "crosshair";
    case CursorKind::NotAllowed:
        return "crossed_circle";
    default:
        return "left_ptr";
    }
}

static bool EnsureCursorTheme(WaylandBackendState * impl) noexcept
{
    if (!impl || !impl->shm)
        return false;
    if (impl->cursor_theme)
        return true;
    char const * const theme_name = std::getenv("XCURSOR_THEME");
    int size = 24;
    if (char const * const s = std::getenv("XCURSOR_SIZE")) {
        const int v = std::atoi(s);
        if (v > 0)
            size = v;
    }
    impl->cursor_theme = wl_cursor_theme_load(theme_name, size, impl->shm);
    if (!impl->cursor_theme)
        impl->cursor_theme = wl_cursor_theme_load(theme_name, 32, impl->shm);
    return impl->cursor_theme != nullptr;
}

static void ReleaseWctxCustomCursor(Internal::WaylandWindowContext * wctx) noexcept
{
    if (!wctx)
        return;
    if (wctx->cursor_custom_buffer) {
        wl_buffer_destroy(wctx->cursor_custom_buffer);
        wctx->cursor_custom_buffer = nullptr;
    }
    if (wctx->cursor_custom_pool) {
        wl_shm_pool_destroy(wctx->cursor_custom_pool);
        wctx->cursor_custom_pool = nullptr;
    }
    if (wctx->cursor_custom_mmap && wctx->cursor_custom_mmap_size > 0) {
        munmap(wctx->cursor_custom_mmap, wctx->cursor_custom_mmap_size);
        wctx->cursor_custom_mmap = nullptr;
        wctx->cursor_custom_mmap_size = 0;
    }
    if (wctx->cursor_custom_fd >= 0) {
        close(wctx->cursor_custom_fd);
        wctx->cursor_custom_fd = -1;
    }
    wctx->cursor_is_custom = false;
    wctx->cursor_custom_w = 0;
    wctx->cursor_custom_h = 0;
}

static void ApplyThemeCursorKind(WaylandBackendState * impl, CursorKind kind) noexcept
{
    if (!impl || !impl->pointer || !impl->compositor)
        return;
    if (!EnsureCursorTheme(impl))
        return;
    char const * name = XcursorNameForKind(kind);
    wl_cursor * c = wl_cursor_theme_get_cursor(impl->cursor_theme, name);
    if (!c || c->image_count < 1)
        c = wl_cursor_theme_get_cursor(impl->cursor_theme, "left_ptr");
    if (!c || c->image_count < 1)
        return;
    wl_cursor_image * const image = c->images[0];
    if (!impl->cursor_surface)
        impl->cursor_surface = wl_compositor_create_surface(impl->compositor);
    if (!impl->cursor_surface)
        return;
    wl_buffer * const buffer = wl_cursor_image_get_buffer(image);
    if (!buffer)
        return;
    wl_surface_attach(impl->cursor_surface, buffer, 0, 0);
    wl_surface_damage(
        impl->cursor_surface,
        0,
        0,
        static_cast<int32_t>(image->width),
        static_cast<int32_t>(image->height));
    wl_surface_commit(impl->cursor_surface);
    wl_pointer_set_cursor(
        impl->pointer,
        impl->pointer_serial,
        impl->cursor_surface,
        static_cast<int32_t>(image->hotspot_x),
        static_cast<int32_t>(image->hotspot_y));
}

static void ApplyPointerForWindow(WaylandBackendState * impl, Internal::WaylandWindowContext * wctx) noexcept
{
    if (!impl || !wctx || !impl->pointer)
        return;
    if (wctx->cursor_is_custom && wctx->cursor_custom_buffer && impl->compositor) {
        if (!impl->cursor_surface)
            impl->cursor_surface = wl_compositor_create_surface(impl->compositor);
        if (!impl->cursor_surface)
            return;
        wl_surface_attach(impl->cursor_surface, wctx->cursor_custom_buffer, 0, 0);
        wl_surface_damage(
            impl->cursor_surface,
            0,
            0,
            wctx->cursor_custom_w,
            wctx->cursor_custom_h);
        wl_surface_commit(impl->cursor_surface);
        wl_pointer_set_cursor(
            impl->pointer,
            impl->pointer_serial,
            impl->cursor_surface,
            static_cast<int32_t>(wctx->cursor_hotspot_x),
            static_cast<int32_t>(wctx->cursor_hotspot_y));
        return;
    }
    ApplyThemeCursorKind(impl, wctx->cursor_kind);
}

static void wm_base_ping(void *, xdg_wm_base * wm, uint32_t serial)
{
    xdg_wm_base_pong(wm, serial);
}
static const xdg_wm_base_listener wm_base_listener = {wm_base_ping};
static void xdg_surface_configure(void *, xdg_surface * xdg_surf, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surf, serial);
}
static const xdg_surface_listener xdg_surface_listener_impl = {xdg_surface_configure};
static void xdg_toplevel_configure(void * data, xdg_toplevel *, int32_t width, int32_t height, wl_array *)
{
    auto * wctx = static_cast<Internal::WaylandWindowContext *>(data);
    if (!wctx)
        return;
    const int nw = width > 0 ? width : wctx->width;
    const int nh = height > 0 ? height : wctx->height;
    if (nw <= 0 || nh <= 0)
        return;
    if (nw == wctx->width && nh == wctx->height && wctx->configured)
        return;
    wctx->width = nw;
    wctx->height = nh;
    wctx->configured = true;
    Event e{};
    e.type = EventType::RESIZE;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = nw;
    e.height = nh;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static void xdg_toplevel_close(void * data, xdg_toplevel *)
{
    auto * wctx = static_cast<Internal::WaylandWindowContext *>(data);
    if (!wctx)
        return;
    wctx->closed = true;
    Event e{};
    e.type = EventType::QUIT;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static const xdg_toplevel_listener xdg_toplevel_listener_impl = {xdg_toplevel_configure, xdg_toplevel_close};
static void pointer_enter(void * data, wl_pointer *, uint32_t serial, wl_surface * surf, wl_fixed_t x, wl_fixed_t y)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    impl->pointer_serial = serial;
    impl->pointer_focus = surf;
    impl->pointer_x = wl_fixed_to_int(x);
    impl->pointer_y = wl_fixed_to_int(y);
    Internal::WaylandWindowContext * wctx = WctxFromSurface(surf);
    if (!wctx)
        return;
    Event e{};
    e.type = EventType::MOUSE_ENTER;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = impl->pointer_x;
    e.y = impl->pointer_y;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
    ApplyPointerForWindow(impl, wctx);
}
static void pointer_leave(void * data, wl_pointer *, uint32_t, wl_surface * surf)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    Internal::WaylandWindowContext * wctx = WctxFromSurface(surf ? surf : impl->pointer_focus);
    if (wctx) {
        Event e{};
        e.type = EventType::MOUSE_LEAVE;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = 0;
        e.x = impl->pointer_x;
        e.y = impl->pointer_y;
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = 0;
        e.character = 0;
        wctx->push_event(e);
    }
    impl->pointer_focus = nullptr;
}
static void pointer_motion(void * data, wl_pointer *, uint32_t, wl_fixed_t x, wl_fixed_t y)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    impl->pointer_x = wl_fixed_to_int(x);
    impl->pointer_y = wl_fixed_to_int(y);
    Internal::WaylandWindowContext * wctx = WctxFromSurface(impl->pointer_focus);
    if (!wctx)
        return;
    Event e{};
    e.type = EventType::MOUSE_MOVE;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = impl->pointer_x;
    e.y = impl->pointer_y;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static void pointer_button(void * data, wl_pointer *, uint32_t serial, uint32_t, uint32_t button, uint32_t state)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    impl->pointer_serial = serial;
    Internal::WaylandWindowContext * wctx = WctxFromSurface(impl->pointer_focus);
    if (!wctx)
        return;
    Event e{};
    e.type = state == WL_POINTER_BUTTON_STATE_PRESSED ? EventType::MOUSE_BUTTON_PRESS : EventType::MOUSE_BUTTON_RELEASE;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = WaylandButtonToHonkord(button);
    e.x = impl->pointer_x;
    e.y = impl->pointer_y;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static void pointer_axis(void *, wl_pointer *, uint32_t, uint32_t, wl_fixed_t) {}
static void pointer_frame(void *, wl_pointer *) {}
static void pointer_axis_source(void *, wl_pointer *, uint32_t) {}
static void pointer_axis_stop(void *, wl_pointer *, uint32_t, uint32_t) {}
static void pointer_axis_discrete(void *, wl_pointer *, uint32_t, int32_t) {}
static void pointer_axis_value120(void *, wl_pointer *, uint32_t, int32_t) {}
static void pointer_axis_relative_direction(void *, wl_pointer *, uint32_t, uint32_t) {}
static const wl_pointer_listener pointer_listener_impl = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
    pointer_frame,
    pointer_axis_source,
    pointer_axis_stop,
    pointer_axis_discrete,
    pointer_axis_value120,
    pointer_axis_relative_direction,
};
static void keyboard_keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t)
{
    if (fd >= 0)
        close(fd);
}
static void keyboard_enter(void * data, wl_keyboard *, uint32_t, wl_surface * surf, wl_array *)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    impl->keyboard_focus = surf;
    Internal::WaylandWindowContext * wctx = WctxFromSurface(surf);
    if (!wctx)
        return;
    Event e{};
    e.type = EventType::FOCUS;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = 0;
    e.height = 0;
    e.focused = 1;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static void keyboard_leave(void * data, wl_keyboard *, uint32_t, wl_surface * surf)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    Internal::WaylandWindowContext * wctx = WctxFromSurface(surf ? surf : impl->keyboard_focus);
    if (wctx) {
        Event e{};
        e.type = EventType::FOCUS;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = 0;
        e.x = 0;
        e.y = 0;
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = 0;
        e.character = 0;
        wctx->push_event(e);
    }
    impl->keyboard_focus = nullptr;
}
static void keyboard_key(void * data, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    Internal::WaylandWindowContext * wctx = WctxFromSurface(impl->keyboard_focus);
    if (!wctx)
        return;
    const unsigned evdev = key + 8u;
    Event e{};
    e.type = state == WL_KEYBOARD_KEY_STATE_PRESSED ? EventType::KEY_PRESS : EventType::KEY_RELEASE;
    e.key = MapLinuxEvdev(evdev);
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    wctx->push_event(e);
}
static void keyboard_modifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
static void keyboard_repeat_info(void *, wl_keyboard *, int32_t, int32_t) {}
static const wl_keyboard_listener keyboard_listener_impl = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info,
};

static void seat_capabilities(void * data, wl_seat * seat, uint32_t caps)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !impl->pointer) {
        impl->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(impl->pointer, &pointer_listener_impl, impl);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !impl->keyboard) {
        impl->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(impl->keyboard, &keyboard_listener_impl, impl);
    }
}
static void seat_name(void *, wl_seat *, const char *) {}
static const wl_seat_listener seat_listener_impl = {seat_capabilities, seat_name};
static void output_geometry(
    void * data,
    wl_output *,
    int32_t x,
    int32_t y,
    int32_t,
    int32_t,
    int32_t,
    const char *,
    const char *,
    int32_t)
{
    auto * slot = static_cast<WaylandBackendState::OutputSlot *>(data);
    if (!slot)
        return;
    slot->x = x;
    slot->y = y;
    slot->has_geometry = true;
}
static void output_mode(void * data, wl_output *, uint32_t flags, int32_t width, int32_t height, int32_t)
{
    auto * slot = static_cast<WaylandBackendState::OutputSlot *>(data);
    if (!slot)
        return;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        slot->width = width;
        slot->height = height;
        slot->has_mode = true;
    }
}
static void output_done(void * data, wl_output *)
{
    auto * slot = static_cast<WaylandBackendState::OutputSlot *>(data);
    if (!slot || !slot->owner)
        return;
    slot->owner->monitors_dirty = true;
}
static void output_scale(void *, wl_output *, int32_t) {}
static void output_name(void *, wl_output *, const char *) {}
static void output_description(void *, wl_output *, const char *) {}
static const wl_output_listener output_listener_impl = {
    output_geometry,
    output_mode,
    output_done,
    output_scale,
    output_name,
    output_description,
};

static void registry_global(void * data, wl_registry * reg, uint32_t name, const char * iface, uint32_t version)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    if (std::strcmp(iface, "wl_compositor") == 0) {
        impl->compositor = static_cast<wl_compositor *>(
            wl_registry_bind(reg, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(iface, "wl_shm") == 0) {
        impl->shm = static_cast<wl_shm *>(wl_registry_bind(reg, name, &wl_shm_interface, 1));
    } else if (std::strcmp(iface, xdg_wm_base_interface.name) == 0) {
        impl->wm_base = static_cast<xdg_wm_base *>(
            wl_registry_bind(reg, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(impl->wm_base, &wm_base_listener, impl);
    } else if (std::strcmp(iface, "wl_seat") == 0) {
        impl->seat = static_cast<wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, 5));
        wl_seat_add_listener(impl->seat, &seat_listener_impl, impl);
    } else if (std::strcmp(iface, "wl_output") == 0) {
        wl_output * const o = static_cast<wl_output *>(
            wl_registry_bind(reg, name, &wl_output_interface, 3));
        auto slot = std::make_unique<WaylandBackendState::OutputSlot>();
        slot->owner = impl;
        slot->output = o;
        slot->global_name = name;
        impl->output_slots.push_back(std::move(slot));
        WaylandBackendState::OutputSlot * const p = impl->output_slots.back().get();
        wl_output_add_listener(o, &output_listener_impl, p);
    }
    (void)version;
}
static void registry_global_remove(void * data, wl_registry *, uint32_t name)
{
    auto * impl = static_cast<WaylandBackendState *>(data);
    if (!impl)
        return;
    for (auto it = impl->output_slots.begin(); it != impl->output_slots.end();) {
        if ((*it)->global_name == name) {
            if ((*it)->output)
                wl_output_destroy((*it)->output);
            it = impl->output_slots.erase(it);
            impl->monitors_dirty = true;
        } else {
            ++it;
        }
    }
}
static const wl_registry_listener registry_listener_impl = {registry_global, registry_global_remove};

} // namespace detail

WindowBackend::WindowBackend() noexcept
    : impl_(std::make_unique<detail::WaylandBackendState>())
{
    impl_->self = this;
}
WindowBackend::~WindowBackend()
{
    CloseDisplay();
}
WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : impl_(std::move(other.impl_))
{
    if (impl_)
        impl_->self = this;
}
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        impl_ = std::move(other.impl_);
        if (impl_)
            impl_->self = this;
    }
    return *this;
}
bool WindowBackend::OpenDisplay(const char * name) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<detail::WaylandBackendState>();
    impl_->self = this;
    if (impl_->display)
        return true;

    impl_->display = wl_display_connect(name);
    if (!impl_->display) {
        HonkordGL::Graphics::SetInternalApiError("Wayland: wl_display_connect failed (is WAYLAND_DISPLAY set?).");
        return false;
    }
    impl_->owns_display = true;

    impl_->registry = wl_display_get_registry(impl_->display);
    wl_registry_add_listener(impl_->registry, &detail::registry_listener_impl, impl_.get());
    wl_display_roundtrip(impl_->display);

    if (!impl_->compositor || !impl_->wm_base) {
        HonkordGL::Graphics::SetInternalApiError("Wayland: missing wl_compositor or xdg_wm_base.");
        CloseDisplay();
        return false;
    }

    wl_display_roundtrip(impl_->display);
    return true;
}
bool WindowBackend::BorrowFromSession(Internal::PlatformSession& session) noexcept
{
    if (!session.is_connected()) {
        HonkordGL::Graphics::SetInternalApiError("BorrowFromSession: Wayland PlatformSession is not connected.");
        return false;
    }
    CloseDisplay();
    if (!impl_)
        impl_ = std::make_unique<detail::WaylandBackendState>();
    impl_->self = this;
    impl_->display = session.display();
    impl_->owns_display = false;

    impl_->registry = wl_display_get_registry(impl_->display);
    wl_registry_add_listener(impl_->registry, &detail::registry_listener_impl, impl_.get());
    wl_display_roundtrip(impl_->display);

    if (!impl_->compositor || !impl_->wm_base) {
        HonkordGL::Graphics::SetInternalApiError("Wayland: missing wl_compositor or xdg_wm_base.");
        CloseDisplay();
        return false;
    }
    wl_display_roundtrip(impl_->display);
    return true;
}
void WindowBackend::CloseDisplay() noexcept
{
    if (!impl_)
        return;
    if (impl_->cursor_surface) {
        wl_surface_destroy(impl_->cursor_surface);
        impl_->cursor_surface = nullptr;
    }
    if (impl_->cursor_theme) {
        wl_cursor_theme_destroy(impl_->cursor_theme);
        impl_->cursor_theme = nullptr;
    }
    if (impl_->shm) {
        wl_shm_destroy(impl_->shm);
        impl_->shm = nullptr;
    }
    if (impl_->pointer) {
        wl_pointer_destroy(impl_->pointer);
        impl_->pointer = nullptr;
    }
    if (impl_->keyboard) {
        wl_keyboard_destroy(impl_->keyboard);
        impl_->keyboard = nullptr;
    }
    if (impl_->seat) {
        wl_seat_destroy(impl_->seat);
        impl_->seat = nullptr;
    }
    for (auto& s : impl_->output_slots) {
        if (s && s->output) {
            wl_output_destroy(s->output);
            s->output = nullptr;
        }
    }
    impl_->output_slots.clear();

    if (impl_->wm_base) {
        xdg_wm_base_destroy(impl_->wm_base);
        impl_->wm_base = nullptr;
    }
    if (impl_->registry) {
        wl_registry_destroy(impl_->registry);
        impl_->registry = nullptr;
    }
    impl_->compositor = nullptr;

    if (impl_->display) {
        if (impl_->owns_display)
            wl_display_disconnect(impl_->display);
        impl_->display = nullptr;
        impl_->owns_display = false;
    }
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    const char * const title = ctx.window_title;
    const unsigned width = static_cast<unsigned>(ctx.client_rect.w);
    const unsigned height = static_cast<unsigned>(ctx.client_rect.z);

    if (!impl_ || !impl_->display || !impl_->compositor || !impl_->wm_base || width == 0 || height == 0) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: Wayland display or size invalid.");
        return false;
    }

    auto wctx = std::make_unique<Internal::WaylandWindowContext>();
    wctx->display = impl_->display;
    wctx->width = static_cast<int>(width);
    wctx->height = static_cast<int>(height);

    wctx->surface = wl_compositor_create_surface(impl_->compositor);
    if (!wctx->surface) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: wl_compositor_create_surface failed.");
        return false;
    }
    wl_proxy_set_user_data(reinterpret_cast<wl_proxy *>(wctx->surface), wctx.get());

    wctx->xdg_shell_surface = xdg_wm_base_get_xdg_surface(impl_->wm_base, wctx->surface);
    if (!wctx->xdg_shell_surface) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: xdg_wm_base_get_xdg_surface failed.");
        wl_surface_destroy(wctx->surface);
        return false;
    }
    xdg_surface_add_listener(wctx->xdg_shell_surface, &detail::xdg_surface_listener_impl, wctx.get());

    wctx->xdg_shell_toplevel = xdg_surface_get_toplevel(wctx->xdg_shell_surface);
    if (!wctx->xdg_shell_toplevel) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: xdg_surface_get_toplevel failed.");
        xdg_surface_destroy(wctx->xdg_shell_surface);
        wl_surface_destroy(wctx->surface);
        return false;
    }
    xdg_toplevel_add_listener(wctx->xdg_shell_toplevel, &detail::xdg_toplevel_listener_impl, wctx.get());

    if (title && std::strlen(title) > 0)
        xdg_toplevel_set_title(wctx->xdg_shell_toplevel, title);
    xdg_toplevel_set_app_id(wctx->xdg_shell_toplevel, "org.honkordgl.app");

    wl_surface_commit(wctx->surface);
    wl_display_roundtrip(impl_->display);

    ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>(wctx.release());
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(impl_->display);
    ctx.client_rect.x = 0;
    ctx.client_rect.y = 0;
    ctx.client_rect.w = static_cast<int>(width);
    ctx.client_rect.z = static_cast<int>(height);
    ctx.is_visible = 1;
    ctx.is_minimized = 0;
    ctx.dpi_x = detail::kDefaultDpi;
    ctx.dpi_y = detail::kDefaultDpi;
    ctx.frame_buffer_width = static_cast<int>(width);
    ctx.frame_buffer_height = static_cast<int>(height);
    ctx.needs_resize = 0;
    ctx.native_platform = static_cast<int>(NativePlatform::Wayland);
    return true;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx)
        return;

    detail::ReleaseWctxCustomCursor(wctx);

    if (wctx->xdg_shell_toplevel) {
        xdg_toplevel_destroy(wctx->xdg_shell_toplevel);
        wctx->xdg_shell_toplevel = nullptr;
    }
    if (wctx->xdg_shell_surface) {
        xdg_surface_destroy(wctx->xdg_shell_surface);
        wctx->xdg_shell_surface = nullptr;
    }
    if (wctx->surface) {
        wl_surface_destroy(wctx->surface);
        wctx->surface = nullptr;
    }
    delete wctx;
    ctx.window_handle = nullptr;
    if (impl_ && ctx.device_context == reinterpret_cast<HonkordGL_GW_Handle>(impl_->display))
        ctx.device_context = nullptr;
    ctx.native_platform = 0;
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_ || !impl_->display || !ctx.window_handle)
        return false;
    if (!ctx.device_context)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(impl_->display);

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == HonkordGL::Events::EventType::QUIT)
            return false;
    }
    return true;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx || !wctx->xdg_shell_toplevel || !title)
        return;
    xdg_toplevel_set_title(wctx->xdg_shell_toplevel, title);
    wl_surface_commit(wctx->surface);
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_ || !impl_->display || !ctx.window_handle)
        return;
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx)
        return;
    if (ctx.window_title && std::strlen(ctx.window_title) > 0 && wctx->xdg_shell_toplevel)
        xdg_toplevel_set_title(wctx->xdg_shell_toplevel, ctx.window_title);
    const int w = ctx.client_rect.w;
    const int h = ctx.client_rect.z;
    if (w > 0 && h > 0) {
        wctx->width = w;
        wctx->height = h;
        if (wctx->egl_window)
            wl_egl_window_resize(wctx->egl_window, w, h, 0, 0);
    }
    if (wctx->surface)
        wl_surface_commit(wctx->surface);
    ctx.frame_buffer_width = wctx->width;
    ctx.frame_buffer_height = wctx->height;
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    if (impl_)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(impl_->display);
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!impl_ || !impl_->display || !out)
        return false;
    return CreateIpcHelperWindow(
        reinterpret_cast<HonkordGL_GW_Handle>(impl_->display),
        0,
        message_atom_name,
        out);
}
wl_display * WindowBackend::GetDisplay() const noexcept
{
    return impl_ ? impl_->display : nullptr;
}
bool WindowBackend::HasXkbExtension() const noexcept
{
    return impl_ && impl_->ext_xkb;
}
bool WindowBackend::HasXRandrExtension() const noexcept
{
    return impl_ && impl_->ext_xrandr;
}
bool WindowBackend::HasXInput2Extension() const noexcept
{
    return impl_ && impl_->ext_xinput2;
}
int WindowBackend::XInput2MajorVersion() const noexcept
{
    return impl_ ? impl_->xi2_major : 0;
}
int WindowBackend::XInput2MinorVersion() const noexcept
{
    return impl_ ? impl_->xi2_minor : 0;
}
int WindowBackend::XInputOpcode() const noexcept
{
    return impl_ ? impl_->xi_opcode : 0;
}
int WindowBackend::RandrEventBase() const noexcept
{
    return 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    if (!impl_)
        return false;
    impl_->monitor_poll_enabled = true;
    return true;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (!impl_)
        return false;
    const bool d = impl_->monitors_dirty;
    impl_->monitors_dirty = false;
    return d;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (!impl_)
        return 0;
    int n = 0;
    for (const auto& s : impl_->output_slots) {
        if (s && s->has_geometry && s->has_mode && s->width > 0 && s->height > 0)
            ++n;
    }
    return n;
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!impl_ || !out)
        return false;
    int i = 0;
    for (const auto& s : impl_->output_slots) {
        if (!s || !(s->has_geometry && s->has_mode && s->width > 0 && s->height > 0))
            continue;
        if (i == index) {
            out->x = s->x;
            out->y = s->y;
            out->w = s->width;
            out->z = s->height;
            return true;
        }
        ++i;
    }
    return false;
}
void WindowBackend::SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept
{
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx || !impl_)
        return;
    detail::ReleaseWctxCustomCursor(wctx);
    wctx->cursor_kind = kind;
    if (impl_->pointer_focus == wctx->surface)
        detail::ApplyPointerForWindow(impl_.get(), wctx);
}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept
{
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx || !impl_ || !impl_->shm || !params.rgba_pixels)
        return false;
    const int w = params.width;
    const int h = params.height;
    if (w <= 0 || h <= 0)
        return false;
    const int sstride = params.stride_bytes > 0 ? params.stride_bytes : (w * 4);
    if (sstride < w * 4)
        return false;

    detail::ReleaseWctxCustomCursor(wctx);

    const int stride = w * 4;
    const std::size_t bufsize = static_cast<std::size_t>(stride) * static_cast<std::size_t>(h);
    int const fd = memfd_create("honkordgl-cursor", MFD_CLOEXEC);
    if (fd < 0)
        return false;
    if (ftruncate(fd, static_cast<off_t>(bufsize)) != 0) {
        close(fd);
        return false;
    }
    void * const map = mmap(nullptr, bufsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return false;
    }
    auto * const dst = static_cast<std::uint32_t *>(map);
    for (int row = 0; row < h; ++row) {
        const std::uint8_t * src =
            params.rgba_pixels + static_cast<std::size_t>(row) * static_cast<std::size_t>(sstride);
        for (int col = 0; col < w; ++col) {
            const std::uint8_t * px = src + static_cast<std::size_t>(col * 4);
            const unsigned r = px[0];
            const unsigned g = px[1];
            const unsigned b = px[2];
            const unsigned a = px[3];
            dst[row * w + col] =
                (static_cast<std::uint32_t>(a) << 24u) | (static_cast<std::uint32_t>(r) << 16u)
                | (static_cast<std::uint32_t>(g) << 8u) | static_cast<std::uint32_t>(b);
        }
    }

    wl_shm_pool * const pool = wl_shm_create_pool(impl_->shm, fd, static_cast<int32_t>(bufsize));
    if (!pool) {
        munmap(map, bufsize);
        close(fd);
        return false;
    }
    wl_buffer * const buffer =
        wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!buffer) {
        munmap(map, bufsize);
        close(fd);
        return false;
    }

    wctx->cursor_custom_buffer = buffer;
    wctx->cursor_custom_mmap = map;
    wctx->cursor_custom_mmap_size = bufsize;
    wctx->cursor_custom_fd = fd;
    wctx->cursor_custom_w = w;
    wctx->cursor_custom_h = h;
    wctx->cursor_is_custom = true;
    wctx->cursor_kind = CursorKind::Arrow;

    int hx = params.hotspot_x;
    int hy = params.hotspot_y;
    if (hx < 0)
        hx = 0;
    if (hy < 0)
        hy = 0;
    if (hx >= w)
        hx = w - 1;
    if (hy >= h)
        hy = h - 1;
    wctx->cursor_hotspot_x = hx;
    wctx->cursor_hotspot_y = hy;

    if (impl_->pointer_focus == wctx->surface)
        detail::ApplyPointerForWindow(impl_.get(), wctx);
    return true;
}
void WindowBackend::ResetCursor(ApplicationContextSettings& ctx) noexcept
{
    auto * wctx = reinterpret_cast<Internal::WaylandWindowContext *>(ctx.window_handle);
    if (!wctx || !impl_)
        return;
    detail::ReleaseWctxCustomCursor(wctx);
    wctx->cursor_kind = CursorKind::Arrow;
    if (impl_->pointer_focus == wctx->surface)
        detail::ApplyPointerForWindow(impl_.get(), wctx);
}
} // namespace HonkordGL::Graphics::Wayland

#else

namespace HonkordGL::Graphics::Wayland {

namespace detail {
struct WaylandBackendState {};
} // namespace detail

WindowBackend::WindowBackend() noexcept = default;
WindowBackend::~WindowBackend() = default;
WindowBackend::WindowBackend(WindowBackend&& other) noexcept = default;
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept = default;
bool WindowBackend::OpenDisplay(const char *) noexcept
{
    return false;
}
bool WindowBackend::BorrowFromSession(Internal::PlatformSession&) noexcept
{
    return false;
}
void WindowBackend::CloseDisplay() noexcept {}
bool WindowBackend::CreateWindow(ApplicationContextSettings&) noexcept
{
    return false;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings&) noexcept {}
bool WindowBackend::ProcessEvents(ApplicationContextSettings&) noexcept
{
    return false;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings&, const char *) noexcept {}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings&) noexcept {}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings&) const noexcept {}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow *, const char *) const noexcept
{
    return false;
}
wl_display * WindowBackend::GetDisplay() const noexcept
{
    return nullptr;
}
bool WindowBackend::HasXkbExtension() const noexcept
{
    return false;
}
bool WindowBackend::HasXRandrExtension() const noexcept
{
    return false;
}
bool WindowBackend::HasXInput2Extension() const noexcept
{
    return false;
}
int WindowBackend::XInput2MajorVersion() const noexcept
{
    return 0;
}
int WindowBackend::XInput2MinorVersion() const noexcept
{
    return 0;
}
int WindowBackend::XInputOpcode() const noexcept
{
    return 0;
}
int WindowBackend::RandrEventBase() const noexcept
{
    return 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    return false;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    return false;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    return 0;
}
bool WindowBackend::GetMonitorBounds(int, Recti *) const noexcept
{
    return false;
}
void WindowBackend::SetCursorKind(ApplicationContextSettings&, CursorKind) noexcept {}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings&, const CursorImageParams&) noexcept
{
    return false;
}
void WindowBackend::ResetCursor(ApplicationContextSettings&) noexcept {}
} // namespace HonkordGL::Graphics::Wayland

#endif