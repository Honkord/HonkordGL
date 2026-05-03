/**
    * @author Honkord <https://github.com>

    HonkordGL
    Copyright (C) 2026 Honkord 
    (See LICENSE.md for what this file is licensed under)

    TO DO:
        * define HONKORDGL_TERMINAL_DEBUGGER                    See the overall backend process.
        * define HONKORDGL_TERMINAL_ONLY_ERRORS                 See the overall backend process, ONLY THE ERRORS.
*/

#pragma once

#include <HonkordGL/Config.h>
/* `Config.h` includes `PlatformAdapter.h` (`HONKORDGL_PLATFORM_*`, `HonkordGL::CurrentBuildPlatform()`). */
#include <HonkordGL/Cursor.h>
#include <HonkordGL/Joystick.h>
#include <HonkordGL/Audio.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/GpuApiBoundary.h>
#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/GpuCapabilities.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/OpenGlIntegration.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/SoftwareRenderer.h>
#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/SoftwareBlitCollector.h>
#include <HonkordGL/SoftwareBlitCollectorAlt.h>
#include <HonkordGL/Sprite.h>
#include <HonkordGL/Lines.h>
#include <HonkordGL/Eclipse.h>
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