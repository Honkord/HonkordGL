/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Android window backend
 * Copyright (C) 2026 Honkord
 */

#include "WindowBackend.hpp"
#include "AndroidWindowContext.hpp"

#include <HonkordGL/AndroidNativeApp.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/Video.h>

#if defined(__ANDROID__)

#include <android/keycodes.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

namespace HonkordGL::Graphics::Android {

namespace {

using HonkordGL::Events::Event;
using HonkordGL::Events::EventType;
using HonkordGL::Events::KeyCode;
using Internal::AndroidWindowState;

AndroidWindowState * g_focus{nullptr};

void EnqueueResize(AndroidWindowState * st, int w, int h) noexcept
{
    if (!st || !st->ctx)
        return;
    st->last_w = w;
    st->last_h = h;
    st->ctx->client_rect.w = w;
    st->ctx->client_rect.z = h;
    st->ctx->frame_buffer_width = w;
    st->ctx->frame_buffer_height = h;
    st->ctx->needs_resize = 1;
    Event e{};
    e.type = EventType::RESIZE;
    e.key = KeyCode::NULL_KEY;
    e.mouse_button = 0;
    e.x = 0;
    e.y = 0;
    e.width = w;
    e.height = h;
    e.focused = 0;
    e.modifiers = 0;
    e.character = 0;
    st->pending.push_back(e);
}
void EnqueueQuit(AndroidWindowState * st) noexcept
{
    if (!st)
        return;
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
}
KeyCode MapKeyCode(int32_t k) noexcept
{
    switch (k) {
    case AKEYCODE_A:
        return KeyCode::A;
    case AKEYCODE_B:
        return KeyCode::B;
    case AKEYCODE_C:
        return KeyCode::C;
    case AKEYCODE_D:
        return KeyCode::D;
    case AKEYCODE_E:
        return KeyCode::E;
    case AKEYCODE_F:
        return KeyCode::F;
    case AKEYCODE_G:
        return KeyCode::G;
    case AKEYCODE_H:
        return KeyCode::H;
    case AKEYCODE_I:
        return KeyCode::I;
    case AKEYCODE_J:
        return KeyCode::J;
    case AKEYCODE_K:
        return KeyCode::K;
    case AKEYCODE_L:
        return KeyCode::L;
    case AKEYCODE_M:
        return KeyCode::M;
    case AKEYCODE_N:
        return KeyCode::N;
    case AKEYCODE_O:
        return KeyCode::O;
    case AKEYCODE_P:
        return KeyCode::P;
    case AKEYCODE_Q:
        return KeyCode::Q;
    case AKEYCODE_R:
        return KeyCode::R;
    case AKEYCODE_S:
        return KeyCode::S;
    case AKEYCODE_T:
        return KeyCode::T;
    case AKEYCODE_U:
        return KeyCode::U;
    case AKEYCODE_V:
        return KeyCode::V;
    case AKEYCODE_W:
        return KeyCode::W;
    case AKEYCODE_X:
        return KeyCode::X;
    case AKEYCODE_Y:
        return KeyCode::Y;
    case AKEYCODE_Z:
        return KeyCode::Z;
    case AKEYCODE_0:
        return KeyCode::NUMBER_0;
    case AKEYCODE_1:
        return KeyCode::NUMBER_1;
    case AKEYCODE_2:
        return KeyCode::NUMBER_2;
    case AKEYCODE_3:
        return KeyCode::NUMBER_3;
    case AKEYCODE_4:
        return KeyCode::NUMBER_4;
    case AKEYCODE_5:
        return KeyCode::NUMBER_5;
    case AKEYCODE_6:
        return KeyCode::NUMBER_6;
    case AKEYCODE_7:
        return KeyCode::NUMBER_7;
    case AKEYCODE_8:
        return KeyCode::NUMBER_8;
    case AKEYCODE_9:
        return KeyCode::NUMBER_9;
    case AKEYCODE_ESCAPE:
        return KeyCode::ESCAPE;
    case AKEYCODE_ENTER:
        return KeyCode::ENTER;
    case AKEYCODE_SPACE:
        return KeyCode::SPACE;
    case AKEYCODE_DPAD_UP:
        return KeyCode::UP;
    case AKEYCODE_DPAD_DOWN:
        return KeyCode::DOWN;
    case AKEYCODE_DPAD_LEFT:
        return KeyCode::LEFT;
    case AKEYCODE_DPAD_RIGHT:
        return KeyCode::RIGHT;
    case AKEYCODE_BACK:
        return KeyCode::ESCAPE;
    default:
        return KeyCode::NULL_KEY;
    }
}
void PumpLooper(android_app * app) noexcept
{
    if (!app)
        return;
    for (int i = 0; i < 32; ++i) {
        int ident = 0;
        int events = 0;
        android_poll_source * source = nullptr;
        ident = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void **>(&source));
        if (ident < 0)
            break;
        if (source)
            source->process(app, source);
    }
}

void OnAppCmdForward(android_app * app, int32_t cmd);

void (*g_user_app_cmd)(android_app *, int32_t){nullptr};

void HonkordAppCmdGlue(android_app * app, int32_t cmd)
{
    if (g_user_app_cmd)
        g_user_app_cmd(app, cmd);
    OnAppCmdForward(app, cmd);
}

int32_t (*g_user_input)(android_app *, AInputEvent *){nullptr};

void OnInputForward(android_app * app, AInputEvent * ev);

int32_t HonkordInputGlue(android_app * app, AInputEvent * ev)
{
    OnInputForward(app, ev);
    if (g_user_input)
        return g_user_input(app, ev);
    return 0;
}
void OnAppCmdForward(android_app * app, int32_t cmd) noexcept
{
    AndroidWindowState * st = g_focus;
    if (!st || st->app != app)
        return;

    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            const int nw = ANativeWindow_getWidth(app->window);
            const int nh = ANativeWindow_getHeight(app->window);
            st->native_window = app->window;
            if (nw > 0 && nh > 0 && (nw != st->last_w || nh != st->last_h))
                EnqueueResize(st, nw, nh);
        }
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONTENT_RECT_CHANGED:
        if (app->window) {
            const int nw = ANativeWindow_getWidth(app->window);
            const int nh = ANativeWindow_getHeight(app->window);
            if (nw > 0 && nh > 0 && (nw != st->last_w || nh != st->last_h))
                EnqueueResize(st, nw, nh);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        st->native_window = nullptr;
        break;
    case APP_CMD_DESTROY:
        EnqueueQuit(st);
        break;
    default:
        break;
    }
}
void OnInputForward(android_app * app, AInputEvent * ev) noexcept
{
    AndroidWindowState * st = g_focus;
    if (!st || st->app != app || !ev)
        return;

    const int type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int action = AKeyEvent_getAction(ev);
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP)
            return;
        Event e{};
        e.type = action == AKEY_EVENT_ACTION_DOWN ? EventType::KEY_PRESS : EventType::KEY_RELEASE;
        e.key = MapKeyCode(AKeyEvent_getKeyCode(ev));
        e.mouse_button = 0;
        e.x = 0;
        e.y = 0;
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = static_cast<unsigned int>(AKeyEvent_getMetaState(ev));
        e.character = 0;
        st->pending.push_back(e);
        return;
    }

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        const float x = AMotionEvent_getX(ev, 0);
        const float y = AMotionEvent_getY(ev, 0);
        Event e{};
        e.key = KeyCode::NULL_KEY;
        e.x = static_cast<int>(x);
        e.y = static_cast<int>(y);
        e.width = 0;
        e.height = 0;
        e.focused = 0;
        e.modifiers = 0;
        e.character = 0;
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            e.type = EventType::MOUSE_BUTTON_PRESS;
            e.mouse_button = 1;
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            e.type = EventType::MOUSE_BUTTON_RELEASE;
            e.mouse_button = 1;
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            e.type = EventType::MOUSE_MOVE;
            e.mouse_button = 0;
        } else {
            return;
        }
        st->pending.push_back(e);
    }
}

} // namespace

WindowBackend::~WindowBackend()
{
    CloseDisplay();
}
WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : display_open_(other.display_open_)
    , app_(other.app_)
    , hooks_installed_(other.hooks_installed_)
    , saved_on_app_cmd_(other.saved_on_app_cmd_)
    , saved_on_input_event_(other.saved_on_input_event_)
{
    other.display_open_ = false;
    other.app_ = nullptr;
    other.hooks_installed_ = false;
    other.saved_on_app_cmd_ = nullptr;
    other.saved_on_input_event_ = nullptr;
}
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        display_open_ = other.display_open_;
        app_ = other.app_;
        hooks_installed_ = other.hooks_installed_;
        saved_on_app_cmd_ = other.saved_on_app_cmd_;
        saved_on_input_event_ = other.saved_on_input_event_;
        other.display_open_ = false;
        other.app_ = nullptr;
        other.hooks_installed_ = false;
        other.saved_on_app_cmd_ = nullptr;
        other.saved_on_input_event_ = nullptr;
    }
    return *this;
}
bool WindowBackend::AttachApp(android_app * app) noexcept
{
    if (!app)
        return false;
    HonkordGL::Graphics::Android::BindNativeApp(app);
    app_ = app;
    if (!hooks_installed_) {
        saved_on_app_cmd_ = app->onAppCmd;
        saved_on_input_event_ = app->onInputEvent;
        g_user_app_cmd = saved_on_app_cmd_;
        g_user_input = saved_on_input_event_;
        app->onAppCmd = HonkordAppCmdGlue;
        app->onInputEvent = HonkordInputGlue;
        hooks_installed_ = true;
    }
    display_open_ = true;
    return true;
}
bool WindowBackend::OpenDisplay(const char *) noexcept
{
    android_app * const a = HonkordGL::Graphics::Android::GetNativeApp();
    if (!a) {
        HonkordGL::Graphics::SetInternalApiError(
            "Android: call HonkordGL::Graphics::Android::BindNativeApp(android_app*) before WindowBackend::Initialize.");
        return false;
    }
    return AttachApp(a);
}
void WindowBackend::CloseDisplay() noexcept
{
    if (app_ && hooks_installed_) {
        app_->onAppCmd = saved_on_app_cmd_;
        app_->onInputEvent = saved_on_input_event_;
        hooks_installed_ = false;
    }
    saved_on_app_cmd_ = nullptr;
    saved_on_input_event_ = nullptr;
    g_user_app_cmd = nullptr;
    g_user_input = nullptr;
    app_ = nullptr;
    display_open_ = false;
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!display_open_ && !OpenDisplay(nullptr))
        return false;
    android_app * const app = app_;
    if (!app || !app->window) {
        HonkordGL::Graphics::SetInternalApiError(
            "Android: native window not ready; create the window from onAppCmd after APP_CMD_INIT_WINDOW.");
        return false;
    }

    ANativeWindow * const nw = app->window;
    int w = ANativeWindow_getWidth(nw);
    int h = ANativeWindow_getHeight(nw);
    if (ctx.client_rect.w > 0)
        w = ctx.client_rect.w;
    if (ctx.client_rect.z > 0)
        h = ctx.client_rect.z;
    if (w <= 0 || h <= 0) {
        HonkordGL::Graphics::SetInternalApiError("Android: invalid surface size.");
        return false;
    }

    auto * const st = new AndroidWindowState;
    st->native_window = nw;
    st->app = app;
    st->ctx = &ctx;
    st->last_w = w;
    st->last_h = h;

    g_focus = st;

    ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>(st);
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(app);
    ctx.client_rect.x = 0;
    ctx.client_rect.y = 0;
    ctx.client_rect.w = w;
    ctx.client_rect.z = h;
    ctx.is_visible = 1;
    ctx.is_minimized = 0;
    ctx.dpi_x = 160;
    ctx.dpi_y = 160;
    ctx.frame_buffer_width = w;
    ctx.frame_buffer_height = h;
    ctx.needs_resize = 0;
    ctx.native_platform = static_cast<int>(HonkordGL::Graphics::NativePlatform::Android);
    return true;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<AndroidWindowState *>(ctx.window_handle);
    if (!st)
        return;
    if (g_focus == st)
        g_focus = nullptr;
    delete st;
    ctx.window_handle = nullptr;
    if (ctx.device_context == reinterpret_cast<HonkordGL_GW_Handle>(app_))
        ctx.device_context = nullptr;
    ctx.native_platform = 0;
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<AndroidWindowState *>(ctx.window_handle);
    if (!st || !st->app)
        return false;

    PumpLooper(st->app);

    if (st->app->destroyRequested) {
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
    }

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == HonkordGL::Events::EventType::QUIT)
            return false;
    }
    return true;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings&, const char * title) noexcept
{
    (void)title;
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    auto * const st = reinterpret_cast<AndroidWindowState *>(ctx.window_handle);
    if (!st || !st->app || !st->app->window)
        return;
    const int nw = ANativeWindow_getWidth(st->app->window);
    const int nh = ANativeWindow_getHeight(st->app->window);
    if (nw > 0 && nh > 0) {
        st->ctx = &ctx;
        EnqueueResize(st, nw, nh);
    }
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    if (app_)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(app_);
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow *, const char *) const noexcept
{
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    return app_;
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
    return app_ ? 1 : 0;
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (index != 0 || !out || !app_)
        return false;
    if (app_->window) {
        out->x = 0;
        out->y = 0;
        out->w = ANativeWindow_getWidth(app_->window);
        out->z = ANativeWindow_getHeight(app_->window);
        return true;
    }
    out->x = app_->contentRect.left;
    out->y = app_->contentRect.top;
    out->w = app_->contentRect.right - app_->contentRect.left;
    out->z = app_->contentRect.bottom - app_->contentRect.top;
    return true;
}

} // namespace HonkordGL::Graphics::Android

#endif // __ANDROID__