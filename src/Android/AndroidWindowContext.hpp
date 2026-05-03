/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — per-window Android state (implementation detail)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_ANDROID_WINDOWCONTEXT_HPP
#define HONKORDGL_ANDROID_WINDOWCONTEXT_HPP

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <deque>

namespace HonkordGL::Graphics::Android::Internal {

struct AndroidWindowState {
    ANativeWindow * native_window{nullptr};
    android_app * app{nullptr};
    ApplicationContextSettings * ctx{nullptr};
    std::deque<HonkordGL::Events::Event> pending;
    int last_w{0};
    int last_h{0};
};

} // namespace HonkordGL::Graphics::Android::Internal

#endif