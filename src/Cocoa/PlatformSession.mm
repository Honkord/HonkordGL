/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — internal Cocoa + GLFW bootstrap
    * Copyright (C) 2026 Honkord
    */

#include "PlatformSession.hpp"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include <HonkordGL/Video.h>

#include <cstdio>

#if defined(HONKORDGL_USE_GLFW)
#include <GLFW/glfw3.h>
#endif

namespace HonkordGL::Graphics::Cocoa::Internal
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
    : app_ready_(other.app_ready_),
      screen_(other.screen_),
      glfw_ok_(other.glfw_ok_)
{
    other.app_ready_ = false;
    other.screen_ = 0;
    other.glfw_ok_ = false;
}
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept
{
    if (this != &other) {
        shutdown();
        app_ready_ = other.app_ready_;
        screen_ = other.screen_;
        glfw_ok_ = other.glfw_ok_;
        other.app_ready_ = false;
        other.screen_ = 0;
        other.glfw_ok_ = false;
    }
    return *this;
}
bool PlatformSession::connect_display() noexcept
{
    if (app_ready_)
        return true;
    @autoreleasepool {
        [NSApplication sharedApplication];
        if ([NSApp setActivationPolicy:NSApplicationActivationPolicyRegular] != YES) {
            HonkordGL::Graphics::SetInternalApiError("PlatformSession: NSApplication setActivationPolicy failed.");
            return false;
        }
        app_ready_ = true;
        screen_ = 0;
        return true;
    }
}
void PlatformSession::setup_core_cocoa() noexcept
{
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
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_COCOA)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_COCOA);
#endif
    if (glfwInit() != GLFW_TRUE) {
        HonkordGL::Graphics::SetInternalApiError("PlatformSession: glfwInit failed.");
        return false;
    }
    glfw_ok_ = true;
    return true;
#endif
}
bool PlatformSession::bootstrap() noexcept
{
    if (!connect_display())
        return false;
    setup_core_cocoa();
    if (!prepare_glfw()) {
        shutdown();
        return false;
    }
    return true;
}
void PlatformSession::shutdown() noexcept
{
    terminate_glfw();
    app_ready_ = false;
    screen_ = 0;
}
void * PlatformSession::application() const noexcept
{
    if (!app_ready_)
        return nullptr;
    return (__bridge void *)[NSApplication sharedApplication];
}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#else

namespace HonkordGL::Graphics::Cocoa::Internal
{

PlatformSession::~PlatformSession() = default;
PlatformSession::PlatformSession(PlatformSession&& other) noexcept = default;
PlatformSession& PlatformSession::operator=(PlatformSession&& other) noexcept = default;

bool PlatformSession::connect_display() noexcept
{
    return false;
}
void PlatformSession::setup_core_cocoa() noexcept {}
bool PlatformSession::prepare_glfw() noexcept
{
    return false;
}
bool PlatformSession::bootstrap() noexcept
{
    return false;
}
void PlatformSession::shutdown() noexcept {}
void * PlatformSession::application() const noexcept
{
    return nullptr;
}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif