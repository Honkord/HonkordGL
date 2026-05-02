/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — compile-time platform identification (single source of truth)
 * Copyright (C) 2026 Honkord
 *
 * Include this (or any header that pulls `Config.h`) instead of branching on `__linux__`, `_WIN32`,
 * `__APPLE__`, etc. in portable HonkordGL API code. All toolchain probes are centralized here.
 *
 * Macros are 0 or 1. `HONKORDGL_PLATFORM_RASPBERRY_PI` remains an opt-in user define (see Config.h).
 */

#ifndef HONKORDGL_PLATFORM_ADAPTER_H
#define HONKORDGL_PLATFORM_ADAPTER_H

/* --- Host OS / environment (mutually overlapping where noted) --- */

#if defined(__ANDROID__)
#define HONKORDGL_PLATFORM_ANDROID 1
#else
#define HONKORDGL_PLATFORM_ANDROID 0
#endif

#if defined(_WIN32)
#define HONKORDGL_PLATFORM_WINDOWS 1
#else
#define HONKORDGL_PLATFORM_WINDOWS 0
#endif

#if defined(__APPLE__)
#define HONKORDGL_PLATFORM_APPLE 1
#else
#define HONKORDGL_PLATFORM_APPLE 0
#endif

#if defined(__linux__)
#define HONKORDGL_PLATFORM_LINUX_KERNEL 1
#else
#define HONKORDGL_PLATFORM_LINUX_KERNEL 0
#endif

#if HONKORDGL_PLATFORM_LINUX_KERNEL && !HONKORDGL_PLATFORM_ANDROID
#define HONKORDGL_PLATFORM_LINUX 1
#else
#define HONKORDGL_PLATFORM_LINUX 0
#endif

#if defined(__NetBSD__)
#define HONKORDGL_PLATFORM_NETBSD 1
#else
#define HONKORDGL_PLATFORM_NETBSD 0
#endif

#if defined(__FreeBSD__)
#define HONKORDGL_PLATFORM_FREEBSD 1
#else
#define HONKORDGL_PLATFORM_FREEBSD 0
#endif

#if defined(__EMSCRIPTEN__)
#define HONKORDGL_PLATFORM_EMSCRIPTEN 1
#else
#define HONKORDGL_PLATFORM_EMSCRIPTEN 0
#endif

#ifdef HONKORDGL_PLATFORM_RASPBERRY_PI
#define HONKORDGL_PLATFORM_RASPBERRY 1
#else
#define HONKORDGL_PLATFORM_RASPBERRY 0
#endif

/** Linux desktop driver chain (X11 / Wayland / optional DRM), excluding Android and Raspberry Pi X11-only builds. */
#if HONKORDGL_PLATFORM_LINUX && !HONKORDGL_PLATFORM_RASPBERRY
#define HONKORDGL_PLATFORM_LINUX_DESKTOP 1
#else
#define HONKORDGL_PLATFORM_LINUX_DESKTOP 0
#endif

/** Sources that implement X11 clients (Linux desktop, NetBSD, FreeBSD, Raspberry Pi X11). Only defined when true (legacy `#if defined(...)`). */
#if HONKORDGL_PLATFORM_LINUX || HONKORDGL_PLATFORM_NETBSD || HONKORDGL_PLATFORM_FREEBSD
#define HONKORDGL_PLATFORM_USES_X11_SOURCES 1
#endif

/** Non-Android `WindowBackend` implementations (desktop Linux, BSD, Pi, Cocoa, Win32). */
#if HONKORDGL_PLATFORM_LINUX || HONKORDGL_PLATFORM_APPLE || HONKORDGL_PLATFORM_WINDOWS || HONKORDGL_PLATFORM_NETBSD \
    || HONKORDGL_PLATFORM_FREEBSD || HONKORDGL_PLATFORM_RASPBERRY
#define HONKORDGL_PLATFORM_WINDOW_BACKEND_DESKTOP 1
#else
#define HONKORDGL_PLATFORM_WINDOW_BACKEND_DESKTOP 0
#endif

/** Vulkan swapchain path in this tree targets Linux desktop (non-Android). */
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
#define HONKORDGL_PLATFORM_VULKAN_LINUX_DESKTOP 1
#else
#define HONKORDGL_PLATFORM_VULKAN_LINUX_DESKTOP 0
#endif

/**
 * Embedded / mobile-style GPU build of `GpuRenderer` (narrower host link set than desktop).
 * True on Android. For embedded Linux targets, define `HONKORDGL_FORCE_OPENGL_ES` to `1` before including HonkordGL
 * headers and link the EGL + GLES libraries required by your SDK.
 */
#if HONKORDGL_PLATFORM_ANDROID || (defined(HONKORDGL_FORCE_OPENGL_ES) && (HONKORDGL_FORCE_OPENGL_ES + 0) == 1)
#define HONKORDGL_GPU_RENDERER_OPENGL_ES 1
#else
#define HONKORDGL_GPU_RENDERER_OPENGL_ES 0
#endif

#if defined(__cplusplus)

namespace HonkordGL {

/** Build-time host class for `CurrentBuildPlatform()`. */
enum class BuildPlatform : unsigned char {
    Unknown = 0,
    Windows,
    Apple,
    LinuxDesktop,
    LinuxAndroid,
    LinuxRaspberryPi,
    NetBSD,
    FreeBSD,
    Emscripten,
};

constexpr BuildPlatform CurrentBuildPlatform() noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    return BuildPlatform::Windows;
#elif HONKORDGL_PLATFORM_APPLE
    return BuildPlatform::Apple;
#elif HONKORDGL_PLATFORM_ANDROID
    return BuildPlatform::LinuxAndroid;
#elif HONKORDGL_PLATFORM_RASPBERRY
    return BuildPlatform::LinuxRaspberryPi;
#elif HONKORDGL_PLATFORM_NETBSD
    return BuildPlatform::NetBSD;
#elif HONKORDGL_PLATFORM_FREEBSD
    return BuildPlatform::FreeBSD;
#elif HONKORDGL_PLATFORM_EMSCRIPTEN
    return BuildPlatform::Emscripten;
#elif HONKORDGL_PLATFORM_LINUX_DESKTOP
    return BuildPlatform::LinuxDesktop;
#else
    return BuildPlatform::Unknown;
#endif
}

constexpr bool IsBuildPlatform(BuildPlatform p) noexcept
{
    return CurrentBuildPlatform() == p;
}

} // namespace HonkordGL

#endif /* __cplusplus */

#endif /* HONKORDGL_PLATFORM_ADAPTER_H */