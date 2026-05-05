/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — X11 / Wayland atom and connection escape hatch (Linux desktop)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_LINUXDISPLAYINTEGRATION_H
#define HONKORDGL_LINUXDISPLAYINTEGRATION_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <cstddef>

namespace HonkordGL::Graphics {

enum class LinuxDisplayIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_PLATFORM = -2,
    NO_DISPLAY = -3,
};

/**
 * X11: `display` is `Display*` from `ApplicationContextSettings::device_context` when `NativePlatform` is X11-family.
 * Interns any atom name (X11 atoms are extensible strings — this is how custom protocols are registered).
 * @return Atom numeric value, or 0 on failure.
 */
HONKORDGL_API HONKORDGL_ND unsigned long LinuxDisplayIntegrationX11InternAtom(
    HonkordGL_GW_Handle display,
    char const * atom_name,
    int only_if_exists) noexcept;

/**
 * X11: Returns the root window of the default screen for `Display*` (or 0 on failure).
 */
HONKORDGL_API HONKORDGL_ND unsigned long LinuxDisplayIntegrationX11DefaultRootWindow(HonkordGL_GW_Handle display) noexcept;

/**
 * Wayland: `window_context` is `ApplicationContextSettings::window_handle` when `NativePlatform::Wayland`.
 */
HONKORDGL_API HONKORDGL_ND HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetDisplay(
    HonkordGL_GW_Handle window_context) noexcept;

HONKORDGL_API HONKORDGL_ND HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetSurface(
    HonkordGL_GW_Handle window_context) noexcept;

} // namespace HonkordGL::Graphics

#endif
