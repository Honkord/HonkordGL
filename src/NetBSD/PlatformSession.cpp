/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — NetBSD PlatformSession (X11 + optional GLFW)
 * Copyright (C) 2026 Honkord
 */

#include "PlatformSession.hpp"

#if defined(__NetBSD__) && defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <HonkordGL/Video.h>

#include <cstdio>

#if defined(HONKORDGL_USE_GLFW)
#include <GLFW/glfw3.h>
#endif

namespace HonkordGL::Graphics::NetBSD::Internal
{

#if defined(HONKORDGL_USE_GLFW)
static void glfwErrorCallback(int code, const char * description) noexcept
{
    if (description) {
        std::fprintf(stderr, "[HonkordGL] GLFW error %i: %s\n", code, description);
        HonkordGL::Graphics::SetInternalApiError(description);
    }
}
#endif

PlatformSession::~PlatformSession()
{
    shutdown();
}
PlatformSession::PlatformSession(PlatformSession&& other) noexcept
    : dpy_(other.dpy_),
      screen_(other.screen_),
      owns_display_(other.owns_display_),
      wm_protocols_(other.wm_protocols_),
      wm_delete_(other.wm_delete_),
      glfw_ok_(other.glfw_ok_)
{
    other.dpy_ = nullptr;
    other.screen_ = 0;
    other.owns_display_ = false;
    other.wm_protocols_ = None;
    other.wm_delete_ = None;
    other.glfw_ok_ = false;
}
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept
{
    if (this != &other) {
        shutdown();
        dpy_ = other.dpy_;
        screen_ = other.screen_;
        owns_display_ = other.owns_display_;
        wm_protocols_ = other.wm_protocols_;
        wm_delete_ = other.wm_delete_;
        glfw_ok_ = other.glfw_ok_;
        other.dpy_ = nullptr;
        other.screen_ = 0;
        other.owns_display_ = false;
        other.wm_protocols_ = None;
        other.wm_delete_ = None;
        other.glfw_ok_ = false;
    }
    return *this;
}
bool PlatformSession::connect_display(const char * display_name) noexcept
{
    if (dpy_)
        return true;
    dpy_ = XOpenDisplay(display_name);
    if (!dpy_) {
        HonkordGL::Graphics::SetInternalApiError("NetBSD PlatformSession: XOpenDisplay failed.");
        return false;
    }
    screen_ = DefaultScreen(dpy_);
    owns_display_ = true;
    return true;
}
void PlatformSession::setup_core_x11() noexcept
{
    if (!dpy_)
        return;
    if (wm_protocols_ == None)
        wm_protocols_ = XInternAtom(dpy_, "WM_PROTOCOLS", False);
    if (wm_delete_ == None)
        wm_delete_ = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XFlush(dpy_);
}
void PlatformSession::terminate_glfw() noexcept
{
#if defined(HONKORDGL_USE_GLFW)
    if (glfw_ok_) {
        glfwTerminate();
        glfw_ok_ = false;
    }
#endif
}
bool PlatformSession::prepare_glfw() noexcept
{
#if !defined(HONKORDGL_USE_GLFW)
    return true;
#else
    if (glfw_ok_)
        return true;
    glfwSetErrorCallback(glfwErrorCallback);
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    if (glfwInit() != GLFW_TRUE) {
        HonkordGL::Graphics::SetInternalApiError("NetBSD PlatformSession: glfwInit failed.");
        return false;
    }
    glfw_ok_ = true;
    return true;
#endif
}
bool PlatformSession::bootstrap(const char * display_name) noexcept
{
    if (!connect_display(display_name))
        return false;
    setup_core_x11();
    if (!prepare_glfw()) {
        shutdown();
        return false;
    }
    return true;
}
void PlatformSession::shutdown() noexcept
{
    terminate_glfw();
    if (dpy_) {
        if (owns_display_)
            XCloseDisplay(dpy_);
        dpy_ = nullptr;
        screen_ = 0;
        owns_display_ = false;
    }
    wm_protocols_ = None;
    wm_delete_ = None;
}

} // namespace HonkordGL::Graphics::NetBSD::Internal

#elif defined(__NetBSD__)

namespace HonkordGL::Graphics::NetBSD::Internal
{

PlatformSession::~PlatformSession() = default;
PlatformSession::PlatformSession(PlatformSession&& other) noexcept = default;
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept = default;

bool PlatformSession::connect_display(const char *) noexcept
{
    return false;
}
void PlatformSession::setup_core_x11() noexcept {}
bool PlatformSession::prepare_glfw() noexcept
{
    return false;
}
bool PlatformSession::bootstrap(const char *) noexcept
{
    return false;
}

void PlatformSession::shutdown() noexcept {}
} // namespace HonkordGL::Graphics::NetBSD::Internal

#else

namespace HonkordGL::Graphics::NetBSD::Internal
{
void PlatformSessionCompileUnitPlaceholder() noexcept {}
}

#endif