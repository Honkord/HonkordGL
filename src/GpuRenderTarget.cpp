/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GpuRenderTarget + GL pass encoder
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/GpuRenderTarget.h>
#include <HonkordGL/GpuRenderPassEncoder.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

#include "internal/HonkordGlPassOps.hpp"

#include <cstdlib>
#include <cstring>
#include <algorithm>

#if HONKORDGL_PLATFORM_APPLE
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#elif HONKORDGL_GPU_RENDERER_OPENGL_ES
#include <GLES3/gl3.h>
#elif HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#ifdef None
#undef None
#endif
#endif

namespace HonkordGL::Graphics {

namespace detail {

bool GlShaderFamilyOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}

void NormalizeCreateInfo(GpuRenderTargetCreateInfo * out) noexcept
{
    for (int i = 0; i < out->color_count; ++i) {
        if (static_cast<unsigned char>(out->color_formats[i]) == 0u)
            out->color_formats[i] = GpuColorFormat::Rgba8Unorm;
    }
}

void MapColorFormat(
    GpuColorFormat f,
    GLint * internal,
    GLenum * format,
    GLenum * type) noexcept
{
    switch (f) {
    case GpuColorFormat::Rgba8Unorm:
    default:
        *internal = GL_RGBA8;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        break;
    case GpuColorFormat::Rgba16Float:
        *internal = GL_RGBA16F;
        *format = GL_RGBA;
        *type = GL_FLOAT;
        break;
    case GpuColorFormat::R11G11B10Float:
        *internal = GL_R11F_G11F_B10F;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_INT_10F_11F_11F_REV;
        break;
    }
}

#if HONKORDGL_PLATFORM_WINDOWS

using PFN_glGenTextures = void(APIENTRY*)(GLsizei, GLuint *);
using PFN_glBindTexture = void(APIENTRY*)(GLenum, GLuint);
using PFN_glTexParameteri = void(APIENTRY*)(GLenum, GLenum, GLint);
using PFN_glTexImage2D = void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
using PFN_glDeleteTextures = void(APIENTRY*)(GLsizei, const GLuint *);
using PFN_glGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint *);
using PFN_glDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint *);
using PFN_glBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using PFN_glCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using PFN_glFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFN_glDrawBuffers = void(APIENTRY*)(GLsizei, const GLenum *);
using PFN_glClearBufferfv = void(APIENTRY*)(GLenum, GLint, const GLfloat *);
using PFN_glClearBufferfi = void(APIENTRY*)(GLenum, GLint, GLfloat, GLint);
using PFN_glClearBufferiv = void(APIENTRY*)(GLenum, GLint, const GLint *);

#define DEF_PROC(name) static PFN_##name s_##name{}

DEF_PROC(glGenTextures);
DEF_PROC(glBindTexture);
DEF_PROC(glTexParameteri);
DEF_PROC(glTexImage2D);
DEF_PROC(glDeleteTextures);
DEF_PROC(glGenFramebuffers);
DEF_PROC(glDeleteFramebuffers);
DEF_PROC(glBindFramebuffer);
DEF_PROC(glCheckFramebufferStatus);
DEF_PROC(glFramebufferTexture2D);
DEF_PROC(glDrawBuffers);
DEF_PROC(glClearBufferfv);
DEF_PROC(glClearBufferfi);
DEF_PROC(glClearBufferiv);

#undef DEF_PROC

static GpuRenderer * gProcGpu = nullptr;

static void * LoadProc(char const * name) noexcept
{
    if (gProcGpu)
        return gProcGpu->GetGraphicsProc(name);
    void * p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p)
        return p;
    return reinterpret_cast<void *>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));
}

bool EnsureProcs(GpuRenderer & gpu) noexcept
{
    gProcGpu = &gpu;
    if (s_glGenFramebuffers)
        return true;
#define LOAD(n) s_##n = reinterpret_cast<PFN_##n>(LoadProc(#n))
    LOAD(glGenTextures);
    LOAD(glBindTexture);
    LOAD(glTexParameteri);
    LOAD(glTexImage2D);
    LOAD(glDeleteTextures);
    LOAD(glGenFramebuffers);
    LOAD(glDeleteFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glCheckFramebufferStatus);
    LOAD(glFramebufferTexture2D);
    LOAD(glDrawBuffers);
    LOAD(glClearBufferfv);
    LOAD(glClearBufferfi);
    LOAD(glClearBufferiv);
#undef LOAD
    return s_glGenFramebuffers && s_glBindFramebuffer && s_glFramebufferTexture2D && s_glDrawBuffers && s_glClearBufferfv
        && s_glGenTextures && s_glTexImage2D;
}

#define HOGL(name) s_##name

#else

bool EnsureProcs(GpuRenderer & gpu) noexcept
{
    (void)gpu;
    return true;
}

#define HOGL(name) name

#endif

GLenum DepthStencilAttachEnum(GpuDepthStencilFormat ds) noexcept
{
    return ds == GpuDepthStencilFormat::Depth24Stencil8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
}

void ApplyDepthStencilTexParams() noexcept
{
    HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GLenum MapGpuDepthFunc(GpuDepthFunc f) noexcept
{
    switch (f) {
    case GpuDepthFunc::Never:
        return GL_NEVER;
    case GpuDepthFunc::Less:
        return GL_LESS;
    case GpuDepthFunc::Equal:
        return GL_EQUAL;
    case GpuDepthFunc::Greater:
        return GL_GREATER;
    case GpuDepthFunc::NotEqual:
        return GL_NOTEQUAL;
    case GpuDepthFunc::GreaterOrEqual:
        return GL_GEQUAL;
    case GpuDepthFunc::DepthAlways:
        return GL_ALWAYS;
    case GpuDepthFunc::LessOrEqual:
    default:
        return GL_LEQUAL;
    }
}

int ApplyPassClearsBody(GpuRenderTarget const * target, GpuRenderPassBeginInfo const & info) noexcept
{
    const int ncolor = target ? target->ColorCount() : 1;
    const GpuDepthStencilFormat ds = target ? target->DepthStencilFormat() : GpuDepthStencilFormat::Depth24Unorm;
    const bool has_depth = target ? (ds != GpuDepthStencilFormat::None) : true;

    const std::uint32_t mask = info.clear_mask;
    for (int i = 0; i < ncolor; ++i) {
        const std::uint32_t bit = GPU_CLEAR_COLOR0 << static_cast<unsigned>(i);
        if (mask & bit) {
            const GpuRenderPassClearColor & c = info.color_clears[i];
            const GLfloat v[4] = { c.r, c.g, c.b, c.a };
            HOGL(glClearBufferfv)(GL_COLOR, static_cast<GLint>(i), v);
        }
    }

    const bool clr_depth = (mask & GPU_CLEAR_DEPTH) != 0u;
    const bool clr_stencil = (mask & GPU_CLEAR_STENCIL) != 0u;

    if (!target) {
        if (clr_stencil || clr_depth) {
            if (clr_stencil)
                HOGL(glClearBufferfi)(GL_DEPTH_STENCIL, 0, info.depth_clear, info.stencil_clear);
            else {
                const GLfloat d = info.depth_clear;
                HOGL(glClearBufferfv)(GL_DEPTH, 0, &d);
            }
        }
        return static_cast<int>(GpuRenderPassEncoderResult::OK);
    }

    if (has_depth) {
        if (ds == GpuDepthStencilFormat::Depth24Stencil8) {
            if (clr_depth || clr_stencil)
                HOGL(glClearBufferfi)(GL_DEPTH_STENCIL, 0, info.depth_clear, info.stencil_clear);
        } else {
            if (clr_depth) {
                const GLfloat d = info.depth_clear;
                HOGL(glClearBufferfv)(GL_DEPTH, 0, &d);
            }
        }
    }

    return static_cast<int>(GpuRenderPassEncoderResult::OK);
}

int BeginPassGl(
    GpuRenderer & gpu,
    GpuRenderTarget & target,
    GpuRenderPassBeginInfo const & info) noexcept
{
    if (!GlShaderFamilyOk(gpu.App()))
        return static_cast<int>(GpuRenderPassEncoderResult::UNSUPPORTED_BACKEND);
    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuRenderPassEncoderResult::INVALID_ARGUMENT);
#if HONKORDGL_PLATFORM_WINDOWS
    if (!EnsureProcs(gpu))
        return static_cast<int>(GpuRenderPassEncoderResult::UNSUPPORTED_BACKEND);
#endif
    if (!target.FramebufferObject() || target.Width() <= 0)
        return static_cast<int>(GpuRenderPassEncoderResult::INVALID_ARGUMENT);

    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, target.FramebufferObject());

    const int ncolor = target.ColorCount();
    GLenum bufs[8];
    const int count = (std::min)(ncolor, 8);
    for (int i = 0; i < count; ++i)
        bufs[i] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
    if (count > 0)
        HOGL(glDrawBuffers)(count, bufs);

    glViewport(0, 0, target.Width(), target.Height());

    const GpuDepthStencilFormat ds = target.DepthStencilFormat();
    const bool has_depth = ds != GpuDepthStencilFormat::None;

    if (info.enable_depth_test && has_depth) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(static_cast<GLboolean>(info.depth_write ? GL_TRUE : GL_FALSE));
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    glDepthFunc(MapGpuDepthFunc(info.depth_func));

    return ApplyPassClearsBody(&target, info);
}

} // namespace detail

HONKORDGL_API int GlApplyPassClears(
    GpuRenderer & gpu,
    GpuRenderTarget const * target,
    GpuRenderPassBeginInfo const & info) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(gpu))
        return static_cast<int>(GpuRenderPassEncoderResult::UNSUPPORTED_BACKEND);
#else
    (void)gpu;
#endif
    return detail::ApplyPassClearsBody(target, info);
}

GpuRenderTarget::GpuRenderTarget(GpuRenderTarget && o) noexcept
    : width_(o.width_)
    , height_(o.height_)
    , color_count_(o.color_count_)
    , desc_(o.desc_)
    , fbo_(o.fbo_)
    , depth_tex_(o.depth_tex_)
{
    for (int i = 0; i < 8; ++i)
        colors_[i] = o.colors_[i];
    o.width_ = o.height_ = o.color_count_ = 0;
    o.fbo_ = 0;
    o.depth_tex_ = 0;
    for (int i = 0; i < 8; ++i)
        o.colors_[i] = 0;
    std::memset(&o.desc_, 0, sizeof(o.desc_));
}

GpuRenderTarget & GpuRenderTarget::operator=(GpuRenderTarget && o) noexcept
{
    if (this == &o)
        return *this;
    width_ = o.width_;
    height_ = o.height_;
    color_count_ = o.color_count_;
    desc_ = o.desc_;
    fbo_ = o.fbo_;
    depth_tex_ = o.depth_tex_;
    for (int i = 0; i < 8; ++i)
        colors_[i] = o.colors_[i];
    o.width_ = o.height_ = o.color_count_ = 0;
    o.fbo_ = 0;
    o.depth_tex_ = 0;
    for (int i = 0; i < 8; ++i)
        o.colors_[i] = 0;
    std::memset(&o.desc_, 0, sizeof(o.desc_));
    return *this;
}

GpuRenderTarget::~GpuRenderTarget() noexcept
{
    DestroyGl();
}

void GpuRenderTarget::DestroyGl() noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    if (!s_glDeleteFramebuffers || !s_glDeleteTextures)
        return;
#endif
    if (fbo_) {
        HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);
        HOGL(glDeleteFramebuffers)(1, &fbo_);
        fbo_ = 0;
    }
    for (int i = 0; i < 8; ++i) {
        if (colors_[i]) {
            HOGL(glDeleteTextures)(1, &colors_[i]);
            colors_[i] = 0;
        }
    }
    if (depth_tex_) {
        HOGL(glDeleteTextures)(1, &depth_tex_);
        depth_tex_ = 0;
    }
    width_ = height_ = color_count_ = 0;
    std::memset(&desc_, 0, sizeof(desc_));
}

void GpuRenderTarget::Destroy(GpuRenderer & gpu) noexcept
{
    if (!fbo_)
        return;
    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(gpu))
        return;
#endif
    DestroyGl();
}

int GpuRenderTarget::BuildGl(GpuRenderer & gpu, GpuRenderTargetCreateInfo const & inf) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(gpu))
        return static_cast<int>(GpuRenderTargetResult::PROC_LOAD_FAILED);
#endif

    GpuRenderTargetCreateInfo info = inf;
    detail::NormalizeCreateInfo(&info);

    HOGL(glGenFramebuffers)(1, &fbo_);
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, fbo_);

    for (int i = 0; i < info.color_count; ++i) {
        GLint internal{};
        GLenum fmt{};
        GLenum typ{};
        detail::MapColorFormat(info.color_formats[i], &internal, &fmt, &typ);
        HOGL(glGenTextures)(1, &colors_[i]);
        HOGL(glBindTexture)(GL_TEXTURE_2D, colors_[i]);
        HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        HOGL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        HOGL(glTexImage2D)(
            GL_TEXTURE_2D,
            0,
            internal,
            info.width,
            info.height,
            0,
            fmt,
            typ,
            nullptr);
        HOGL(glFramebufferTexture2D)(
            GL_FRAMEBUFFER,
            static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
            GL_TEXTURE_2D,
            colors_[i],
            0);
    }
    HOGL(glBindTexture)(GL_TEXTURE_2D, 0);

    if (info.depth_stencil != GpuDepthStencilFormat::None) {
        HOGL(glGenTextures)(1, &depth_tex_);
        HOGL(glBindTexture)(GL_TEXTURE_2D, depth_tex_);
        detail::ApplyDepthStencilTexParams();
        GLint din{};
        GLenum dfmt{};
        GLenum dtyp{};
        switch (info.depth_stencil) {
        case GpuDepthStencilFormat::Depth32Float:
            din = GL_DEPTH_COMPONENT32F;
            dfmt = GL_DEPTH_COMPONENT;
            dtyp = GL_FLOAT;
            break;
        case GpuDepthStencilFormat::Depth24Unorm:
            din = GL_DEPTH_COMPONENT24;
            dfmt = GL_DEPTH_COMPONENT;
            dtyp = GL_UNSIGNED_INT;
            break;
        case GpuDepthStencilFormat::Depth24Stencil8:
        default:
            din = GL_DEPTH24_STENCIL8;
            dfmt = GL_DEPTH_STENCIL;
            dtyp = GL_UNSIGNED_INT_24_8;
            break;
        }
        HOGL(glTexImage2D)(GL_TEXTURE_2D, 0, din, info.width, info.height, 0, dfmt, dtyp, nullptr);
        HOGL(glFramebufferTexture2D)(
            GL_FRAMEBUFFER,
            detail::DepthStencilAttachEnum(info.depth_stencil),
            GL_TEXTURE_2D,
            depth_tex_,
            0);
        HOGL(glBindTexture)(GL_TEXTURE_2D, 0);
    }

    if (info.color_count > 0) {
        GLenum bufs[8];
        for (int i = 0; i < info.color_count; ++i)
            bufs[i] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        HOGL(glDrawBuffers)(info.color_count, bufs);
    }

    const GLenum st = HOGL(glCheckFramebufferStatus)(GL_FRAMEBUFFER);
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);

    if (st != GL_FRAMEBUFFER_COMPLETE) {
        DestroyGl();
        return static_cast<int>(GpuRenderTargetResult::INCOMPLETE_TARGET);
    }

    width_ = info.width;
    height_ = info.height;
    color_count_ = info.color_count;
    desc_ = info;
    return static_cast<int>(GpuRenderTargetResult::OK);
}

int GpuRenderTarget::Create(
    GpuRenderer & gpu,
    GpuRenderTargetCreateInfo const & info,
    std::unique_ptr<GpuRenderTarget> * out) noexcept
{
    if (!out)
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);
    out->reset();

    if (!gpu.App())
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);
    if (!detail::GlShaderFamilyOk(gpu.App()))
        return static_cast<int>(GpuRenderTargetResult::UNSUPPORTED_BACKEND);
    if (info.width <= 0 || info.height <= 0 || info.color_count < 1 || info.color_count > 8)
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);

    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);

    auto p = std::unique_ptr<GpuRenderTarget>(new GpuRenderTarget());
    const int br = p->BuildGl(gpu, info);
    if (br != static_cast<int>(GpuRenderTargetResult::OK))
        return br;

    *out = std::move(p);
    return static_cast<int>(GpuRenderTargetResult::OK);
}

GpuObjectName GpuRenderTarget::ColorTexture(int index) const noexcept
{
    if (index < 0 || index >= color_count_)
        return 0;
    return colors_[index];
}

GpuObjectName GpuRenderTarget::DepthStencilTexture() const noexcept
{
    return depth_tex_;
}

int GpuRenderTarget::Resize(GpuRenderer & gpu, int w, int h) noexcept
{
    if (w <= 0 || h <= 0)
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);
    if (!detail::GlShaderFamilyOk(gpu.App()))
        return static_cast<int>(GpuRenderTargetResult::UNSUPPORTED_BACKEND);
    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuRenderTargetResult::INVALID_ARGUMENT);
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(gpu))
        return static_cast<int>(GpuRenderTargetResult::PROC_LOAD_FAILED);
#endif

    GpuRenderTargetCreateInfo inf = desc_;
    inf.width = w;
    inf.height = h;
    DestroyGl();
    return BuildGl(gpu, inf);
}

GpuRenderPassEncoderGl::GpuRenderPassEncoderGl(
    GpuRenderer & gpu,
    GpuRenderTarget & target,
    GpuRenderPassBeginInfo const & info) noexcept
    : gpu_(&gpu)
{
    last_result_ = detail::BeginPassGl(gpu, target, info);
    active_ = (last_result_ == static_cast<int>(GpuRenderPassEncoderResult::OK));
}

GpuRenderPassEncoderGl::GpuRenderPassEncoderGl(GpuRenderPassEncoderGl && other) noexcept
    : gpu_(other.gpu_)
    , active_(other.active_)
    , scissor_enabled_(other.scissor_enabled_)
    , last_result_(other.last_result_)
{
    other.gpu_ = nullptr;
    other.active_ = false;
    other.scissor_enabled_ = false;
}

GpuRenderPassEncoderGl & GpuRenderPassEncoderGl::operator=(GpuRenderPassEncoderGl && other) noexcept
{
    if (this == &other)
        return *this;
    End();
    gpu_ = other.gpu_;
    active_ = other.active_;
    scissor_enabled_ = other.scissor_enabled_;
    last_result_ = other.last_result_;
    other.gpu_ = nullptr;
    other.active_ = false;
    other.scissor_enabled_ = false;
    return *this;
}

GpuRenderPassEncoderGl::~GpuRenderPassEncoderGl() noexcept
{
    End();
}

void GpuRenderPassEncoderGl::End() noexcept
{
    if (!active_ || !gpu_)
        return;
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(*gpu_)) {
        active_ = false;
        gpu_ = nullptr;
        return;
    }
#endif
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);
    if (scissor_enabled_) {
        glDisable(GL_SCISSOR_TEST);
        scissor_enabled_ = false;
    }
    active_ = false;
    gpu_ = nullptr;
}

void GpuRenderPassEncoderGl::SetScissor(int x, int y, int width, int height) noexcept
{
    if (!active_ || !gpu_)
        return;
#if HONKORDGL_PLATFORM_WINDOWS
    if (!detail::EnsureProcs(*gpu_))
        return;
#endif
    if (width <= 0 || height <= 0) {
        if (scissor_enabled_) {
            glDisable(GL_SCISSOR_TEST);
            scissor_enabled_ = false;
        }
        return;
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
    scissor_enabled_ = true;
}

} // namespace HonkordGL::Graphics
