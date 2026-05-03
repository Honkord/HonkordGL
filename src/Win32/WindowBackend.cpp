/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Win32 window backend
 * Copyright (C) 2026 Honkord
 */

#include "WindowBackend.hpp"
#include "Win32WindowContext.hpp"

#include <HonkordGL/Events.h>
#include <HonkordGL/Video.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>

#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateWindowW
#undef CreateWindowW
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace HonkordGL::Graphics::Win32 {

namespace {

using HonkordGL::Graphics::CursorKind;
using HonkordGL::Graphics::CursorImageParams;
using HonkordGL::Graphics::NativePlatform;

constexpr int kDefaultDpi = 96;

static const wchar_t kWindowClassName[] = L"HonkordGL_Win32_Window";

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Events::KeyCode;
using HonkordGL::Events::ModifierFlags;
using Internal::Win32WindowState;

std::wstring Utf8ToWide(const char * utf8) noexcept
{
    if (!utf8 || !utf8[0])
        return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 0)
        return L"HonkordGL";
    std::vector<wchar_t> buf(static_cast<size_t>(n));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf.data(), n);
    return std::wstring(buf.data());
}

/** Clears WM_SETICON and destroys returned icon handles (LoadImage-owned icons). */
void ReleaseWin32WindowIcons(HWND hwnd) noexcept
{
    if (!hwnd)
        return;
    HICON const s = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_SETICON, ICON_SMALL, 0));
    HICON const b = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_SETICON, ICON_BIG, 0));
    if (s)
        DestroyIcon(s);
    if (b && b != s)
        DestroyIcon(b);
}

void ApplyWin32WindowIcon(HWND hwnd, const char * utf8_path) noexcept
{
    if (!hwnd)
        return;
    ReleaseWin32WindowIcons(hwnd);
    if (!utf8_path || !utf8_path[0])
        return;
    const std::wstring wpath = Utf8ToWide(utf8_path);
    if (wpath.empty())
        return;
    HICON const h = reinterpret_cast<HICON>(LoadImageW(
        nullptr,
        wpath.c_str(),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_LOADFROMFILE));
    if (!h)
        return;
    HICON const oldS = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(h)));
    HICON const oldB = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(h)));
    if (oldS)
        DestroyIcon(oldS);
    if (oldB && oldB != oldS)
        DestroyIcon(oldB);
}

unsigned MapModifierKeys() noexcept
{
    unsigned m = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        m |= static_cast<unsigned>(ModifierFlags::SHIFT);
    if (GetKeyState(VK_CONTROL) & 0x8000)
        m |= static_cast<unsigned>(ModifierFlags::CTRL);
    if (GetKeyState(VK_MENU) & 0x8000)
        m |= static_cast<unsigned>(ModifierFlags::ALT);
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000)
        m |= static_cast<unsigned>(ModifierFlags::SUPER);
    if (GetKeyState(VK_CAPITAL) & 1)
        m |= static_cast<unsigned>(ModifierFlags::CAPS_LOCK);
    if (GetKeyState(VK_NUMLOCK) & 1)
        m |= static_cast<unsigned>(ModifierFlags::NUM_LOCK);
    return m;
}

KeyCode MapVk(unsigned vk, unsigned /*flags*/) noexcept
{
    if (vk >= '0' && vk <= '9') {
        const KeyCode map[] = {
            KeyCode::NUMBER_0, KeyCode::NUMBER_1, KeyCode::NUMBER_2, KeyCode::NUMBER_3, KeyCode::NUMBER_4,
            KeyCode::NUMBER_5, KeyCode::NUMBER_6, KeyCode::NUMBER_7, KeyCode::NUMBER_8, KeyCode::NUMBER_9};
        return map[vk - '0'];
    }
    if (vk >= 'A' && vk <= 'Z') {
        const KeyCode map[] = {
            KeyCode::A, KeyCode::B, KeyCode::C, KeyCode::D, KeyCode::E, KeyCode::F, KeyCode::G, KeyCode::H,
            KeyCode::I, KeyCode::J, KeyCode::K, KeyCode::L, KeyCode::M, KeyCode::N, KeyCode::O, KeyCode::P,
            KeyCode::Q, KeyCode::R, KeyCode::S, KeyCode::T, KeyCode::U, KeyCode::V, KeyCode::W, KeyCode::X,
            KeyCode::Y, KeyCode::Z};
        return map[vk - 'A'];
    }
    switch (vk) {
    case VK_ESCAPE:
        return KeyCode::ESCAPE;
    case VK_RETURN:
        return KeyCode::ENTER;
    case VK_TAB:
        return KeyCode::TAB;
    case VK_SPACE:
        return KeyCode::SPACE;
    case VK_BACK:
        return KeyCode::BACKQUOTE;
    case VK_UP:
        return KeyCode::UP;
    case VK_DOWN:
        return KeyCode::DOWN;
    case VK_LEFT:
        return KeyCode::LEFT;
    case VK_RIGHT:
        return KeyCode::RIGHT;
    case VK_F1:
        return KeyCode::F1;
    case VK_F2:
        return KeyCode::F2;
    case VK_F3:
        return KeyCode::F3;
    case VK_F4:
        return KeyCode::F4;
    case VK_F5:
        return KeyCode::F5;
    case VK_F6:
        return KeyCode::F6;
    case VK_F7:
        return KeyCode::F7;
    case VK_F8:
        return KeyCode::F8;
    case VK_F9:
        return KeyCode::F9;
    case VK_F10:
        return KeyCode::F10;
    case VK_F11:
        return KeyCode::F11;
    case VK_F12:
        return KeyCode::F12;
    default:
        return KeyCode::NULL_KEY;
    }
}

void PushEmpty(EventType t, Win32WindowState * st) noexcept
{
    Event e{};
    e.type = t;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = 0;
    e.height = 0;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    st->pending.push_back(e);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    Win32WindowState * st = reinterpret_cast<Win32WindowState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto * const cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        st = reinterpret_cast<Win32WindowState *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;
        return TRUE;
    }
    if (!st)
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            HCURSOR const h = st->user_cursor ? st->user_cursor : LoadCursorW(nullptr, IDC_ARROW);
            SetCursor(h);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE: {
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
        st->pending.push_back(e);
        return 0;
    }
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            if (st->ctx)
                st->ctx->is_minimized = 1;
            break;
        }
        if (st->ctx)
            st->ctx->is_minimized = 0;
        const int nw = LOWORD(lParam);
        const int nh = HIWORD(lParam);
        if (st->ctx) {
            st->ctx->client_rect.w = nw;
            st->ctx->client_rect.z = nh;
            st->ctx->frame_buffer_width = nw;
            st->ctx->frame_buffer_height = nh;
            st->ctx->needs_resize = 1;
            if (st->ctx->ResizeCallback)
                st->ctx->ResizeCallback(st->ctx, nw, nh);
        }
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
        st->pending.push_back(e);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        Event e{};
        e.type = EventType::KEY_PRESS;
        e.key = MapVk(static_cast<unsigned>(wParam), static_cast<unsigned>(HIWORD(lParam)));
        e.modifiers = MapModifierKeys();
        e.x = 0;
        e.y = 0;
        e.width = 0;
        e.height = 0;
        e.mouse_button = 0;
        e.focused = 0;
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        Event e{};
        e.type = EventType::KEY_RELEASE;
        e.key = MapVk(static_cast<unsigned>(wParam), static_cast<unsigned>(HIWORD(lParam)));
        e.modifiers = MapModifierKeys();
        e.x = 0;
        e.y = 0;
        e.width = 0;
        e.height = 0;
        e.mouse_button = 0;
        e.focused = 0;
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_CHAR: {
        if (st->pending.empty() || st->pending.back().type != EventType::KEY_PRESS) {
            return 0;
        }
        st->pending.back().character = static_cast<unsigned int>(wParam);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st->track_leave) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            st->track_leave = true;
            Event ent{};
            ent.type = EventType::MOUSE_ENTER;
            ent.key = KeyCode::NULL_KEY;
            ent.mouse_button = 0;
            ent.x = GET_X_LPARAM(lParam);
            ent.y = GET_Y_LPARAM(lParam);
            ent.width = 0;
            ent.height = 0;
            ent.focused = 0;
            ent.modifiers = MapModifierKeys();
            ent.character = 0;
            st->pending.push_back(ent);
        }
        Event e{};
        e.type = EventType::MOUSE_MOVE;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = 0;
        e.x = GET_X_LPARAM(lParam);
        e.y = GET_Y_LPARAM(lParam);
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = MapModifierKeys();
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_MOUSELEAVE:
        st->track_leave = false;
        PushEmpty(EventType::MOUSE_LEAVE, st);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        Event e{};
        e.type = EventType::MOUSE_BUTTON_PRESS;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = (msg == WM_LBUTTONDOWN) ? 1u : (msg == WM_RBUTTONDOWN) ? 3u : 2u;
        e.x = GET_X_LPARAM(lParam);
        e.y = GET_Y_LPARAM(lParam);
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = MapModifierKeys();
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        Event e{};
        e.type = EventType::MOUSE_BUTTON_RELEASE;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = (msg == WM_LBUTTONUP) ? 1u : (msg == WM_RBUTTONUP) ? 3u : 2u;
        e.x = GET_X_LPARAM(lParam);
        e.y = GET_Y_LPARAM(lParam);
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = MapModifierKeys();
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        Event e{};
        e.type = EventType::MOUSE_BUTTON_PRESS;
        e.key = KeyCode::NULL_KEY;
        const short z = GET_WHEEL_DELTA_WPARAM(wParam);
        e.mouse_button = z > 0 ? 4u : 5u;
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        e.x = pt.x;
        e.y = pt.y;
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = MapModifierKeys();
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        Event e{};
        e.type = EventType::FOCUS;
        e.key = KeyCode::NULL_KEY;
        e.mouse_button = 0;
        e.x = 0;
        e.y = 0;
        e.width = 0;
        e.height = 0;
        e.focused = (msg == WM_SETFOCUS) ? 1 : 0;
        e.modifiers = 0;
        e.character = 0;
        st->pending.push_back(e);
        return 0;
    }
    case WM_DESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WindowDpi(HWND hwnd) noexcept
{
    using GetDpiForWindowFn = UINT(WINAPI *)(HWND);
    static HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto * const fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn)
            return static_cast<int>(fn(hwnd));
    }
    HDC hdc = GetDC(hwnd);
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : kDefaultDpi;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hmon, HDC, LPRECT, LPARAM lp) noexcept
{
    auto * const d = reinterpret_cast<std::vector<RECT> *>(lp);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hmon, &mi))
        d->push_back(mi.rcMonitor);
    return TRUE;
}

bool CreateIpcHelperWindowImpl(HINSTANCE hi, const char * message_atom_name, HonkordGL::Graphics::IpcHelperWindow * out) noexcept
{
    if (!out || !hi)
        return false;
    const UINT msg = message_atom_name ? RegisterWindowMessageA(message_atom_name) : 0;
    if (message_atom_name && msg == 0) {
        HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow: RegisterWindowMessage failed.");
        return false;
    }
    static const wchar_t kHelperClass[] = L"HonkordGL_IpcHelper";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hi;
    wc.lpszClassName = kHelperClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow: RegisterClass failed.");
        return false;
    }
    HWND const w = CreateWindowExW(
        0,
        kHelperClass,
        nullptr,
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        hi,
        nullptr);
    if (!w) {
        HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow: CreateWindowEx failed.");
        return false;
    }
    out->display = reinterpret_cast<HonkordGL_GW_Handle>(hi);
    out->window = w;
    out->message_atom = static_cast<unsigned long>(msg);
    return true;
}

void ResizeNearestRgba(
    const std::uint8_t * src,
    int sw,
    int sh,
    int sstride,
    int dw,
    int dh,
    std::vector<std::uint8_t> * out) noexcept
{
    out->resize(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4u);
    for (int y = 0; y < dh; ++y) {
        const int sy = (y * sh) / dh;
        for (int x = 0; x < dw; ++x) {
            const int sx = (x * sw) / dw;
            const std::uint8_t * const p = src + static_cast<size_t>(sy) * static_cast<size_t>(sstride) + static_cast<size_t>(sx) * 4u;
            std::uint8_t * const d = out->data() + (static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 4u;
            std::memcpy(d, p, 4u);
        }
    }
}

LPCWSTR IdcForCursorKind(CursorKind k) noexcept
{
    switch (k) {
    case CursorKind::Arrow:
        return IDC_ARROW;
    case CursorKind::ArrowWait:
        return IDC_APPSTARTING;
    case CursorKind::Wait:
        return IDC_WAIT;
    case CursorKind::Text:
        return IDC_IBEAM;
    case CursorKind::Hand:
        return IDC_HAND;
    case CursorKind::SizeHorizontal:
    case CursorKind::SizeLeft:
    case CursorKind::SizeRight:
        return IDC_SIZEWE;
    case CursorKind::SizeVertical:
    case CursorKind::SizeTop:
    case CursorKind::SizeBottom:
        return IDC_SIZENS;
    case CursorKind::SizeTopLeftBottomRight:
    case CursorKind::SizeTopLeft:
    case CursorKind::SizeBottomRight:
        return IDC_SIZENWSE;
    case CursorKind::SizeBottomLeftTopRight:
    case CursorKind::SizeBottomLeft:
    case CursorKind::SizeTopRight:
        return IDC_SIZENESW;
    case CursorKind::SizeAll:
        return IDC_SIZEALL;
    case CursorKind::Cross:
        return IDC_CROSS;
    case CursorKind::Help:
        return IDC_HELP;
    case CursorKind::NotAllowed:
        return IDC_NO;
    default:
        return IDC_ARROW;
    }
}

HCURSOR CreateArgbCursor(const CursorImageParams& params, bool * owned_out) noexcept
{
    *owned_out = false;
    if (!params.rgba_pixels || params.width <= 0 || params.height <= 0)
        return nullptr;
    const int sstride = params.stride_bytes > 0 ? params.stride_bytes : (params.width * 4);
    if (sstride < params.width * 4)
        return nullptr;

    int w = params.width;
    int h = params.height;
    int dw = params.display_width > 0 ? params.display_width : w;
    int dh = params.display_height > 0 ? params.display_height : h;
    if (dw <= 0)
        dw = w;
    if (dh <= 0)
        dh = h;

    const std::uint8_t * src = params.rgba_pixels;
    std::vector<std::uint8_t> resized;
    if (dw != w || dh != h) {
        ResizeNearestRgba(params.rgba_pixels, w, h, sstride, dw, dh, &resized);
        src = resized.data();
        w = dw;
        h = dh;
    }

    int hx = params.hotspot_x;
    int hy = params.hotspot_y;
    if (dw != params.width || dh != params.height) {
        if (params.width > 0 && params.height > 0) {
            hx = (hx * dw) / params.width;
            hy = (hy * dh) / params.height;
        }
    }
    hx = (std::max)(0, (std::min)(hx, w - 1));
    hy = (std::max)(0, (std::min)(hy, h - 1));

    const int rowBytesMask = ((w + 31) / 32) * 4;
    std::vector<std::uint8_t> mask(static_cast<size_t>(rowBytesMask) * static_cast<size_t>(h), 0);

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(bih);
    bih.biWidth = w;
    bih.biHeight = -h;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    BITMAPINFO bi{};
    bi.bmiHeader = bih;

    void * bits = nullptr;
    HDC const hdc = GetDC(nullptr);
    if (!hdc)
        return nullptr;
    HBITMAP const hColor = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hColor || !bits)
        return nullptr;

    const int dibStride = ((w * 32 + 31) / 32) * 4;
    auto * const px = static_cast<std::uint8_t *>(bits);
    for (int y = 0; y < h; ++y) {
        const std::uint8_t * row = src + static_cast<size_t>(y) * static_cast<size_t>(w * 4);
        std::uint8_t * dstRow = px + static_cast<size_t>(y) * static_cast<size_t>(dibStride);
        for (int x = 0; x < w; ++x) {
            const std::uint8_t * p = row + static_cast<size_t>(x) * 4u;
            const std::uint8_t r = p[0];
            const std::uint8_t g = p[1];
            const std::uint8_t b = p[2];
            const std::uint8_t a = p[3];
            dstRow[static_cast<size_t>(x) * 4u + 0u] = b;
            dstRow[static_cast<size_t>(x) * 4u + 1u] = g;
            dstRow[static_cast<size_t>(x) * 4u + 2u] = r;
            dstRow[static_cast<size_t>(x) * 4u + 3u] = a;
            if (a > 0) {
                const int bi = y * rowBytesMask + (x / 8);
                const int bt = 7 - (x % 8);
                mask[static_cast<size_t>(bi)] |= static_cast<std::uint8_t>(1u << bt);
            }
        }
    }

    HBITMAP const hMask = CreateBitmap(w, h, 1, 1, mask.data());
    if (!hMask) {
        DeleteObject(hColor);
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon = FALSE;
    ii.xHotspot = static_cast<DWORD>(hx);
    ii.yHotspot = static_cast<DWORD>(hy);
    ii.hbmMask = hMask;
    ii.hbmColor = hColor;

    HCURSOR const cur = reinterpret_cast<HCURSOR>(CreateIconIndirect(&ii));
    DeleteObject(hMask);
    DeleteObject(hColor);
    if (!cur)
        return nullptr;
    *owned_out = true;
    return cur;
}

} // namespace

WindowBackend::~WindowBackend()
{
    CloseDisplay();
}

WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : hinstance_(other.hinstance_),
      display_open_(other.display_open_),
      class_registered_(other.class_registered_),
      last_monitor_count_(other.last_monitor_count_)
{
    other.hinstance_ = nullptr;
    other.display_open_ = false;
    other.class_registered_ = false;
    other.last_monitor_count_ = 0;
}

WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        hinstance_ = other.hinstance_;
        display_open_ = other.display_open_;
        class_registered_ = other.class_registered_;
        last_monitor_count_ = other.last_monitor_count_;
        other.hinstance_ = nullptr;
        other.display_open_ = false;
        other.class_registered_ = false;
        other.last_monitor_count_ = 0;
    }
    return *this;
}

bool WindowBackend::ensure_class_registered() noexcept
{
    if (class_registered_)
        return true;
    hinstance_ = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            HonkordGL::Graphics::SetInternalApiError("Win32: RegisterClass failed.");
            return false;
        }
    }
    class_registered_ = true;
    return true;
}

bool WindowBackend::OpenDisplay(const char *) noexcept
{
    if (display_open_)
        return true;
    if (!ensure_class_registered())
        return false;
    display_open_ = true;
    last_monitor_count_ = GetSystemMetrics(SM_CMONITORS);
    return true;
}

void WindowBackend::CloseDisplay() noexcept
{
    display_open_ = false;
    hinstance_ = nullptr;
    class_registered_ = false;
}

bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!display_open_ && !OpenDisplay(nullptr))
        return false;
    const int w = ctx.client_rect.w;
    const int h = ctx.client_rect.z;
    if (w <= 0 || h <= 0) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: invalid size.");
        return false;
    }

    auto * const st = new Win32WindowState;
    st->ctx = &ctx;

    const std::wstring title = Utf8ToWide(ctx.window_title ? ctx.window_title : "HonkordGL");

    RECT wr{0, 0, w, h};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&wr, style, FALSE);

    HWND const hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        title.c_str(),
        style,
        ctx.client_rect.x,
        ctx.client_rect.y,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        hinstance_,
        st);

    if (!hwnd) {
        delete st;
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: CreateWindowEx failed.");
        return false;
    }

    const int dpi = WindowDpi(hwnd);
    ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>(st);
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(hinstance_);
    ctx.client_rect.w = w;
    ctx.client_rect.z = h;
    ctx.is_visible = 1;
    ctx.is_minimized = 0;
    ctx.dpi_x = dpi;
    ctx.dpi_y = dpi;
    ctx.frame_buffer_width = w;
    ctx.frame_buffer_height = h;
    ctx.needs_resize = 0;
    ctx.native_platform = static_cast<int>(NativePlatform::Win32);

    ApplyWin32WindowIcon(hwnd, ctx.window_icon_path);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st)
        return;
    if (st->hwnd && IsWindow(st->hwnd)) {
        ReleaseWin32WindowIcons(st->hwnd);
        if (st->user_cursor_owned && st->user_cursor) {
            DestroyCursor(st->user_cursor);
            st->user_cursor = nullptr;
            st->user_cursor_owned = false;
        }
        SetWindowLongPtrW(st->hwnd, GWLP_USERDATA, 0);
        ::DestroyWindow(st->hwnd);
    }
    delete st;
    ctx.window_handle = nullptr;
    if (ctx.device_context == reinterpret_cast<HonkordGL_GW_Handle>(hinstance_))
        ctx.device_context = nullptr;
    ctx.native_platform = 0;
}

bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd)
        return false;

    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
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
            st->pending.push_back(e);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == EventType::QUIT)
            return false;
    }
    return true;
}

void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd || !title)
        return;
    const std::wstring w = Utf8ToWide(title);
    SetWindowTextW(st->hwnd, w.c_str());
}

void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd)
        return;
    if (ctx.window_title && std::strlen(ctx.window_title) > 0)
        SetWindowTextW(st->hwnd, Utf8ToWide(ctx.window_title).c_str());
    const int nw = ctx.client_rect.w;
    const int nh = ctx.client_rect.z;
    if (nw > 0 && nh > 0) {
        RECT cr{0, 0, nw, nh};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(st->hwnd, GWL_STYLE));
        AdjustWindowRect(&cr, style, FALSE);
        const int outer_w = cr.right - cr.left;
        const int outer_h = cr.bottom - cr.top;
        const UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
        SetWindowPos(st->hwnd, nullptr, 0, 0, outer_w, outer_h, flags);
        ctx.frame_buffer_width = nw;
        ctx.frame_buffer_height = nh;
    }
    ApplyWin32WindowIcon(st->hwnd, ctx.window_icon_path);
}

void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(hinstance_);
}

bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!out || !hinstance_)
        return false;
    return CreateIpcHelperWindowImpl(hinstance_, message_atom_name, out);
}

void * WindowBackend::GetDisplay() const noexcept
{
    return hinstance_;
}

bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    last_monitor_count_ = GetSystemMetrics(SM_CMONITORS);
    return true;
}

bool WindowBackend::PollMonitorsChanged() noexcept
{
    const int n = GetSystemMetrics(SM_CMONITORS);
    if (n == last_monitor_count_)
        return false;
    last_monitor_count_ = n;
    return true;
}

bool WindowBackend::PresentSoftwareFramebuffer(
    ApplicationContextSettings const& ctx, std::uint8_t const * rgba_pixels, int width, int height, int stride_bytes) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd || !rgba_pixels || width <= 0 || height <= 0)
        return false;
    const int srcStride = stride_bytes > 0 ? stride_bytes : (width * 4);
    if (srcStride < width * 4)
        return false;

    std::vector<std::uint8_t> bgra(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    for (int y = 0; y < height; ++y) {
        std::uint8_t const * const srow = rgba_pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(srcStride);
        std::uint8_t * const drow = bgra.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u);
        for (int x = 0; x < width; ++x) {
            std::size_t const si = static_cast<std::size_t>(x) * 4u;
            std::size_t const di = static_cast<std::size_t>(x) * 4u;
            drow[di + 0u] = srow[si + 2u]; // B
            drow[di + 1u] = srow[si + 1u]; // G
            drow[di + 2u] = srow[si + 0u]; // R
            drow[di + 3u] = srow[si + 3u]; // A
        }
    }

    HDC const hdc = GetDC(st->hwnd);
    if (!hdc)
        return false;

    RECT cr{};
    if (!GetClientRect(st->hwnd, &cr)) {
        ReleaseDC(st->hwnd, hdc);
        return false;
    }
    int const dstW = (std::max)(1, static_cast<int>(cr.right - cr.left));
    int const dstH = (std::max)(1, static_cast<int>(cr.bottom - cr.top));

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    int const r = StretchDIBits(
        hdc,
        0,
        0,
        dstW,
        dstH,
        0,
        0,
        width,
        height,
        bgra.data(),
        &bi,
        DIB_RGB_COLORS,
        SRCCOPY);
    ReleaseDC(st->hwnd, hdc);
    return r != GDI_ERROR;
}

int WindowBackend::GetMonitorCount() const noexcept
{
    const int n = GetSystemMetrics(SM_CMONITORS);
    return n > 0 ? n : 1;
}

bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!out)
        return false;
    std::vector<RECT> rects;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&rects));
    if (rects.empty()) {
        out->x = 0;
        out->y = 0;
        out->w = GetSystemMetrics(SM_CXSCREEN);
        out->z = GetSystemMetrics(SM_CYSCREEN);
        return index == 0;
    }
    if (index < 0 || index >= static_cast<int>(rects.size()))
        return false;
    const RECT& r = rects[static_cast<size_t>(index)];
    out->x = r.left;
    out->y = r.top;
    out->w = r.right - r.left;
    out->z = r.bottom - r.top;
    return true;
}

void WindowBackend::SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd)
        return;
    if (st->user_cursor_owned && st->user_cursor) {
        DestroyCursor(st->user_cursor);
        st->user_cursor_owned = false;
    }
    st->user_cursor = LoadCursorW(nullptr, IdcForCursorKind(kind));
    st->user_cursor_owned = false;
    SetCursor(st->user_cursor);
}

bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept
{
    auto * const st = reinterpret_cast<Win32WindowState *>(ctx.window_handle);
    if (!st || !st->hwnd)
        return false;
    bool owned = false;
    HCURSOR const c = CreateArgbCursor(params, &owned);
    if (!c || !owned)
        return false;
    if (st->user_cursor_owned && st->user_cursor)
        DestroyCursor(st->user_cursor);
    st->user_cursor = c;
    st->user_cursor_owned = true;
    SetCursor(st->user_cursor);
    return true;
}

void WindowBackend::ResetCursor(ApplicationContextSettings& ctx) noexcept
{
    SetCursorKind(ctx, CursorKind::Arrow);
}

} // namespace HonkordGL::Graphics::Win32