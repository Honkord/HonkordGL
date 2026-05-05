/**
    * @author Honkord <https://github.com>

    HonkordGL
    Copyright (C) 2026 Honkord 
    (See LICENSE.md for what this file is licensed under)

    TO DO:
        * define HONKORDGL_TERMINAL_DEBUGGER                    See the overall backend process.
        * define HONKORDGL_TERMINAL_ONLY_ERRORS                 See the overall backend process, ONLY THE ERRORS.
*/

#ifndef __Honkord_Config
#define __Honkord_Config

#include <HonkordGL/PlatformAdapter.h>

//////////////////////////////////////////////////////
//      Version
//////////////////////////////////////////////////////

/* Library Major Version */
#define HONKORDGL_VERSION__MAJOR 1

/* Library Minor Version */
#define HONKORDGL_VERSION__MINOR 0

/* Library Version */
#define HONKORDGL 1.0

//////////////////////////////////////////////////////
//      ISO C++ 
#ifdef HONKORDGL_REQUIRED_CXX20
# if defined(__cplusplus) && __cplusplus <= 202002L
#   pragma warning "Required C++ compiler: ISO C++ 20 (reconfigure with -std=c++20)"
# else 
#   include <concepts>
#   include <type_traits>
# endif
#endif
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      API dll declaration macros
//////////////////////////////////////////////////////
#ifndef HONKORDGL_API
# if defined(_WIN32)
#   if defined(HONKORDGL_BUILDING_DLL)
#     define HONKORDGL_API __declspec(dllexport)
#   elif defined(HONKORDGL_USING_DLL)
#     define HONKORDGL_API __declspec(dllimport)
#   else
#     define HONKORDGL_API
#   endif
# else
#   if defined(HONKORDGL_BUILDING_DLL) && defined(__GNUC__)
#     define HONKORDGL_API __attribute__((visibility("default")))
#   else
#     define HONKORDGL_API
#   endif
# endif
#endif
//////////////////////////////////////////////////////

#ifndef HONKORDGL_AUDIO_DISABLED
#define HONKORDGL_AUDIO_DISABLED 0
#endif

//////////////////////////////////////////////////////
//      Enable Warnings
//////////////////////////////////////////////////////
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 26495)    // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a member variable (type.6).
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant
#pragma clang diagnostic ignored "-Wreserved-identifier"            // warning: identifier '_Xxx' is reserved because it starts with '_' followed by a capital letter
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"            // warning: 'xxx' is an unsafe pointer used for buffer access
#pragma clang diagnostic ignored "-Wnontrivial-memaccess"           // warning: first argument in call to 'memset' is a pointer to non-trivially copyable type
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wfloat-equal"                      // warning: comparing floating-point with '==' or '!=' is unsafe
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
// Disable Debug
//////////////////////////////////////////////////////
#if defined(_MSC_VER) && !defined(__clang__)  && !defined(__INTEL_COMPILER) 
#define IM_MSVC_RUNTIME_CHECKS_OFF      __pragma(runtime_checks("",off))     __pragma(check_stack(off)) __pragma(strict_gs_check(push,off))
#define IM_MSVC_RUNTIME_CHECKS_RESTORE  __pragma(runtime_checks("",restore)) __pragma(check_stack())    __pragma(strict_gs_check(pop))
#else
#define IM_MSVC_RUNTIME_CHECKS_OFF
#define IM_MSVC_RUNTIME_CHECKS_RESTORE
#endif
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      Linux audio backend (PipeWire vs ALSA)
//      When 1, compile/link `src/Audio/PipeWire.cpp` and not the ALSA implementation in `AudioAlsa.cpp`.
//      Define to 1 before including headers (e.g. -DHONKORDGL_AUDIO_USE_PIPEWIRE=1). Link libpipewire-0.3.
//////////////////////////////////////////////////////
#ifndef HONKORDGL_AUDIO_USE_PIPEWIRE
#define HONKORDGL_AUDIO_USE_PIPEWIRE 0
#endif

//////////////////////////////////////////////////////
//      Raspberry Pi: dedicated X11 window driver (optional; excludes desktop Linux X11/Wayland chain)
//      #define HONKORDGL_PLATFORM_RASPBERRY_PI
//      Use when building for Raspberry Pi OS with X11; link the same X11/GLX sources as desktop Linux.
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      Joystick: force virtual-only implementation in `Joystick.cpp` (no OS enumeration).
//      Use for headless / Khronos-only / CI builds. Window backend (X11, Wayland, GDK, Cocoa, Win32, …)
//      is orthogonal: the same `HonkordGL::Joystick` TU selects the OS path via preprocessor, not via
//      `WindowBackendKind`. Define before compiling `src/Joystick/Joystick.cpp`.
//      #define HONKORDGL_JOYSTICK_DUMMY
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      X11-based implementation sources — `HONKORDGL_PLATFORM_USES_X11_SOURCES` (see PlatformAdapter.h)
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      Optional Linux KMS/DRM backend (HonkordGL/WindowBackend)
//      Define when compiling src/DRM/*.cpp and link libdrm.
//      #define HONKORDGL_ENABLE_DRM
//////////////////////////////////////////////////////

//////////////////////////////////////////////////////
//      Software blit collector: alternate view-only API (`SoftwareBlitCollectorAlt`)
//      Set to 0 before including headers to indicate a build without the alternate (omit `SoftwareBlitCollectorAlt.cpp`).
//      `SoftwareBlitCollector::HasAlternateCollectorVersion()` reflects this value.
//////////////////////////////////////////////////////
#ifndef HONKORDGL_SOFTWARE_BLIT_COLLECTOR_ALT_AVAILABLE
# define HONKORDGL_SOFTWARE_BLIT_COLLECTOR_ALT_AVAILABLE 1
#endif

//////////////////////////////////////////////////////
//      Nodiscard
//////////////////////////////////////////////////////
#if defined(__cplusplus) && __cplusplus >= 202011L
# define HONKORDGL_ND [[nodiscard]]
#else 
# define HONKORDGL_ND
#endif
//////////////////////////////////////////////////////

#endif