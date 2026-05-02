/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland PlatformSession
 * Copyright (C) 2026 Honkord
 */

#include "PlatformSession.hpp"

#if defined(__linux__) && !defined(__ANDROID__)

#include <HonkordGL/Video.h>

#include <cstdio>

#include <wayland-client.h>

#if defined(HONKORDGL_USE_GLFW)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#endif

namespace HonkordGL::Graphics::Wayland::Internal
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
      owns_display_(other.owns_display_),
      glfw_ok_(other.glfw_ok_)
{
    other.dpy_ = nullptr;
    other.owns_display_ = false;
    other.glfw_ok_ = false;
}
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept
{
    if (this != &other) {
        shutdown();
        dpy_ = other.dpy_;
        owns_display_ = other.owns_display_;
        glfw_ok_ = other.glfw_ok_;
        other.dpy_ = nullptr;
        other.owns_display_ = false;
        other.glfw_ok_ = false;
    }
    return *this;
}
bool PlatformSession::connect_display(const char * name) noexcept
{
    if (dpy_)
        return true;
    dpy_ = wl_display_connect(name);
    if (!dpy_) {
        HonkordGL::Graphics::SetInternalApiError("PlatformSession: wl_display_connect failed.");
        return false;
    }
    owns_display_ = true;
    return true;
}
void PlatformSession::terminate_glfw() noexcept
{
#if defined(HONKORDGL_USE_GLFW)
    if (glfw_ok_) {
        glfwTerminate();
        glfw_ok_ = false;
    }
#else
    (void)0;
#endif
}
void PlatformSession::shutdown() noexcept
{
    terminate_glfw();
    if (dpy_) {
        if (owns_display_)
            wl_display_disconnect(dpy_);
        dpy_ = nullptr;
        owns_display_ = false;
    }
}

bool PlatformSession::prepare_glfw() noexcept
{
#if !defined(HONKORDGL_USE_GLFW)
    return true;
#else
    if (glfw_ok_)
        return true;
    glfwSetErrorCallback(glfwErrorCallback);
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif
    if (glfwInit() != GLFW_TRUE) {
        HonkordGL::Graphics::SetInternalApiError("PlatformSession: glfwInit failed.");
        return false;
    }
    glfw_ok_ = true;
    return true;
#endif
}
bool PlatformSession::bootstrap(const char * name) noexcept
{
    if (!connect_display(name))
        return false;
    if (!prepare_glfw()) {
        shutdown();
        return false;
    }
    return true;
}

} // namespace HonkordGL::Graphics::Wayland::Internal

#else

namespace HonkordGL::Graphics::Wayland::Internal
{

PlatformSession::~PlatformSession() = default;
PlatformSession::PlatformSession(PlatformSession&& other) noexcept = default;
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept = default;

bool PlatformSession::connect_display(const char*) noexcept
{
    return false;
}
void PlatformSession::shutdown() noexcept {}
bool PlatformSession::prepare_glfw() noexcept
{
    return false;
}
bool PlatformSession::bootstrap(const char *) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::Wayland::Internal

#endif