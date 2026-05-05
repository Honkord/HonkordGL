/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Optional multimedia and higher-level helpers (not required for “core” GPU + window apps)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_EXTRAS_H
#define HONKORDGL_EXTRAS_H

/**
 * @file Extras.h
 * @brief Optional layers on top of `Core.h`: input/audio, CPU raster paths, 2D game helpers, samples.
 *        This header includes `Core.h` first. For core-only translation units, include `Core.h` alone.
 */

#include <HonkordGL/Core.h>

#include <HonkordGL/Joystick.h>
#include <HonkordGL/Audio.h>
#include <HonkordGL/SoftwareRenderer.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/SoftwareBlitCollector.h>
#include <HonkordGL/SoftwareBlitCollectorAlt.h>
#include <HonkordGL/Sprite.h>
#include <HonkordGL/Lines.h>
#include <HonkordGL/Eclipse.h>
#include <HonkordGL/DeferredRendererSample.h>
#include <HonkordGL/Camera.h>
#include <HonkordGL/TextUI.h>

#include <HonkordGL/X11IpcWindow.h>
#if HONKORDGL_PLATFORM_LINUX
#include <HonkordGL/WaylandIpcWindow.h>
#endif
#if HONKORDGL_PLATFORM_APPLE
#include <HonkordGL/CocoaIpcWindow.h>
#endif
#if HONKORDGL_PLATFORM_ANDROID
#include <HonkordGL/AndroidNativeApp.h>
#endif

#endif
