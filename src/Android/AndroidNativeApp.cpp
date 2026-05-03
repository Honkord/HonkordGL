/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Android native app pointer
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/AndroidNativeApp.h>

#if defined(__ANDROID__)

#include <android_native_app_glue.h>

namespace HonkordGL::Graphics::Android {

namespace {
android_app * g_app{nullptr};
} // namespace

void BindNativeApp(android_app * app) noexcept
{
    g_app = app;
}

android_app * GetNativeApp() noexcept
{
    return g_app;
}

} // namespace HonkordGL::Graphics::Android

#endif