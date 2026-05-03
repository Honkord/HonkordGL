/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GPU limits / optional features / attach validation (implementation side)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/GpuCapabilities.h>
#include <HonkordGL/GpuApiBoundary.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/PlatformAdapter.h>

#include <cstdio>
#include <cstring>

#if HONKORDGL_PLATFORM_APPLE
#include <dlfcn.h>
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#elif HONKORDGL_GPU_RENDERER_OPENGL_ES
#include <GLES2/gl2.h>
#elif HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace HonkordGL::Graphics {

namespace {

bool GlShaderBackendOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}

void ZeroLimits(GpuLimits * out) noexcept
{
    if (!out)
        return;
    std::memset(out, 0, sizeof(GpuLimits));
}

void GetGlVersionInts(int * major, int * minor) noexcept
{
    *major = 0;
    *minor = 0;
#ifdef GL_MAJOR_VERSION
    GLint maj = 0;
    GLint mn = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &mn);
    if (maj > 0) {
        *major = maj;
        *minor = mn;
        return;
    }
#endif
    const GLubyte * const ver = glGetString(GL_VERSION);
    if (!ver)
        return;
    int mj = 0;
    int n = 0;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
    if (std::sscanf(reinterpret_cast<const char *>(ver), "OpenGL ES %d.%d", &mj, &n) == 2) {
        *major = mj;
        *minor = n;
        return;
    }
#endif
    if (std::sscanf(reinterpret_cast<const char *>(ver), "%d.%d", &mj, &n) >= 2) {
        *major = mj;
        *minor = n;
    }
}

#if HONKORDGL_PLATFORM_WINDOWS
using PFN_glGetIntegerv = void(APIENTRY*)(GLenum, GLint *);
#endif

static void GlGetIntegervPort(GLenum pname, GLint * params) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    static PFN_glGetIntegerv gi = nullptr;
    if (!gi) {
        gi = reinterpret_cast<PFN_glGetIntegerv>(wglGetProcAddress("glGetIntegerv"));
        if (!gi)
            gi = glGetIntegerv;
    }
    gi(pname, params);
#else
    glGetIntegerv(pname, params);
#endif
}

void QueryGlLimitsInto(GpuLimits * out) noexcept
{
    GLint v[2]{};

    GlGetIntegervPort(GL_MAX_TEXTURE_SIZE, &out->max_texture_2d_size);
    GlGetIntegervPort(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &out->max_cube_map_texture_size);
#ifndef GL_MAX_RENDERBUFFER_SIZE
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#endif
    GlGetIntegervPort(GL_MAX_RENDERBUFFER_SIZE, &out->max_renderbuffer_size);
    GlGetIntegervPort(GL_MAX_VIEWPORT_DIMS, v);
    out->max_viewport_width = v[0];
    out->max_viewport_height = v[1];
    GlGetIntegervPort(GL_MAX_VERTEX_ATTRIBS, &out->max_vertex_attribs);
#ifndef GL_MAX_VERTEX_UNIFORM_VECTORS
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#endif
#ifndef GL_MAX_FRAGMENT_UNIFORM_VECTORS
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD
#endif
#ifndef GL_MAX_VARYING_VECTORS
#define GL_MAX_VARYING_VECTORS 0x8DFC
#endif
    GlGetIntegervPort(GL_MAX_VERTEX_UNIFORM_VECTORS, &out->max_vertex_uniform_vectors);
    GlGetIntegervPort(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &out->max_fragment_uniform_vectors);
    GlGetIntegervPort(GL_MAX_VARYING_VECTORS, &out->max_varying_vectors);
#ifndef GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#endif
#if !HONKORDGL_GPU_RENDERER_OPENGL_ES
    GlGetIntegervPort(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &out->uniform_buffer_offset_alignment);
#endif
#ifndef GL_MAX_COLOR_ATTACHMENTS
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#endif
#if !HONKORDGL_GPU_RENDERER_OPENGL_ES
    GlGetIntegervPort(GL_MAX_COLOR_ATTACHMENTS, &out->max_framebuffer_color_attachments);
#endif
#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif
#if !HONKORDGL_GPU_RENDERER_OPENGL_ES
    GlGetIntegervPort(GL_MAX_SAMPLES, &out->max_msaa_samples);
#elif defined(GL_MAX_SAMPLES)
    GlGetIntegervPort(GL_MAX_SAMPLES, &out->max_msaa_samples);
#endif
#if HONKORDGL_GPU_RENDERER_OPENGL_ES && defined(GL_ES_VERSION_3_0)
    GlGetIntegervPort(GL_MAX_COLOR_ATTACHMENTS, &out->max_framebuffer_color_attachments);
#endif
}

} // namespace

int ValidateRendererDeviceRequest(const RendererContextSettings& spec) noexcept
{
    if (spec.minimum_honkord_gl_api_level != 0 && !HonkordGlApiLevelCompatible(spec.minimum_honkord_gl_api_level)) {
        SetInternalApiError(
            "ValidateRendererDeviceRequest: application minimum HonkordGL API level exceeds this library build.");
        return static_cast<int>(RendererContextResult::API_VERSION_INCOMPATIBLE);
    }
    if (spec.requested_gpu_device != static_cast<GpuDeviceId>(0)) {
        SetInternalApiError(
            "ValidateRendererDeviceRequest: non-default GPU device selection is not implemented in this build.");
        return static_cast<int>(RendererContextResult::ADAPTER_SELECTION_UNSUPPORTED);
    }
    return static_cast<int>(RendererContextResult::OK);
}

int QueryGpuLimits(ApplicationContextSettings * ctx, GpuLimits * out_limits) noexcept
{
    if (!ctx || !out_limits)
        return static_cast<int>(GpuQueryResult::INVALID_ARGUMENT);
    ZeroLimits(out_limits);
    if (!GlShaderBackendOk(ctx))
        return static_cast<int>(GpuQueryResult::UNSUPPORTED_BACKEND);
    const int mc = RendererContextMakeCurrent(*ctx);
    if (mc != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuQueryResult::MAKE_CURRENT_FAILED);
    QueryGlLimitsInto(out_limits);
    return static_cast<int>(GpuQueryResult::OK);
}

int TryEnableGpuOptionalFeature(ApplicationContextSettings * ctx, GpuOptionalFeature feature) noexcept
{
    if (!ctx)
        return static_cast<int>(GpuFeatureEnableResult::INVALID_ARGUMENT);
    const std::uint64_t bit = static_cast<std::uint64_t>(feature);
    if (bit == 0)
        return static_cast<int>(GpuFeatureEnableResult::INVALID_ARGUMENT);
    if ((ctx->gpu_optional_features_enabled_mask & bit) != 0)
        return static_cast<int>(GpuFeatureEnableResult::ALREADY_SATISFIED);
    if (!GlShaderBackendOk(ctx))
        return static_cast<int>(GpuFeatureEnableResult::UNSUPPORTED_BACKEND);
    if (ctx->active_renderer == 0 && ctx->device == nullptr)
        return static_cast<int>(GpuFeatureEnableResult::NO_DEVICE);

    const int mc = RendererContextMakeCurrent(*ctx);
    if (mc != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuFeatureEnableResult::NO_DEVICE);

    int major = 0;
    int minor = 0;
    GetGlVersionInts(&major, &minor);

    bool ok = false;
    switch (feature) {
    case GpuOptionalFeature::GeometryShaderPipeline:
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
        ok = false;
#else
        ok = (major > 3) || (major == 3 && minor >= 2);
#endif
        break;
    case GpuOptionalFeature::DepthTextureSampling:
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
        ok = (major >= 3);
#else
        ok = (major >= 3);
#endif
        break;
    case GpuOptionalFeature::MultisampleRenderTargets: {
        GLint ms = 0;
#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
# ifdef GL_MAX_SAMPLES
        glGetIntegerv(GL_MAX_SAMPLES, &ms);
# endif
#else
        glGetIntegerv(GL_MAX_SAMPLES, &ms);
#endif
        ok = ms > 1;
        break;
    }
    default:
        return static_cast<int>(GpuFeatureEnableResult::INVALID_ARGUMENT);
    }

    if (!ok)
        return static_cast<int>(GpuFeatureEnableResult::UNSUPPORTED);
    ctx->gpu_optional_features_enabled_mask |= bit;
    return static_cast<int>(GpuFeatureEnableResult::OK);
}

bool GpuOptionalFeatureIsEnabled(ApplicationContextSettings const * ctx, GpuOptionalFeature feature) noexcept
{
    if (!ctx)
        return false;
    const std::uint64_t bit = static_cast<std::uint64_t>(feature);
    return (ctx->gpu_optional_features_enabled_mask & bit) != 0;
}

} // namespace HonkordGL::Graphics
