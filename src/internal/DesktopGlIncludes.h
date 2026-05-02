/**
 * HonkordGL — internal: desktop OpenGL + platform GL loader headers (WGL / GLX).
 * Not part of the public API; include only from implementation TUs under `src/`.
 */

#pragma once

#include <HonkordGL/Config.h>

#if HONKORDGL_PLATFORM_APPLE
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#else
#if HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#if !HONKORDGL_PLATFORM_WINDOWS
#include <GL/glx.h>
#endif
#endif