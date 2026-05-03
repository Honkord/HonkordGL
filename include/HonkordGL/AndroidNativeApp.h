/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — bind NDK android_app for WindowBackend (Android only)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_ANDROIDNATIVEAPP_H
#define HONKORDGL_ANDROIDNATIVEAPP_H

#include <HonkordGL/Config.h>

#if HONKORDGL_PLATFORM_ANDROID

struct android_app;

namespace HonkordGL::Graphics::Android {

/** Call once from `android_main` (or before `WindowBackend::Initialize`) with the glue `android_app *`. */
HONKORDGL_API void BindNativeApp(struct android_app * app) noexcept;

HONKORDGL_API HONKORDGL_ND struct android_app * GetNativeApp() noexcept;

} // namespace HonkordGL::Graphics::Android

#endif // HONKORDGL_PLATFORM_ANDROID

#endif