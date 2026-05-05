/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GL state tracker + render graph
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/GpuGlStateTracker.h>
#include <HonkordGL/GpuRenderGraph.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

#include "internal/HonkordGlPassOps.hpp"

#include <algorithm>
#include <set>

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

namespace {

bool GlShaderFamilyOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
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

#if HONKORDGL_PLATFORM_WINDOWS

using PFN_glBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using PFN_glDrawBuffers = void(APIENTRY*)(GLsizei, const GLenum *);

#define DEF_PROC(name) static PFN_##name s_##name{}

DEF_PROC(glBindFramebuffer);
DEF_PROC(glDrawBuffers);

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

static bool EnsureProcs(GpuRenderer & gpu) noexcept
{
    gProcGpu = &gpu;
    if (s_glBindFramebuffer)
        return true;
#define LOAD(n) s_##n = reinterpret_cast<PFN_##n>(LoadProc(#n))
    LOAD(glBindFramebuffer);
    LOAD(glDrawBuffers);
#undef LOAD
    return s_glBindFramebuffer && s_glDrawBuffers;
}

#define HOGL(name) s_##name

#else

static bool EnsureProcs(GpuRenderer & gpu) noexcept
{
    (void)gpu;
    return true;
}

#define HOGL(name) name

#endif

} // namespace

void GpuGlStateTracker::Invalidate() noexcept
{
    fbo_known_ = false;
    draw_buffer_count_ = -2;
    vp_known_ = false;
    depth_known_ = false;
    scissor_known_ = false;
    cm_known_ = false;
}

void GpuGlStateTracker::BindFramebuffer(GpuRenderer & gpu, unsigned int framebuffer_object) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    if (!EnsureProcs(gpu))
        return;
#else
    (void)gpu;
#endif
    if (fbo_known_ && cached_fbo_ == framebuffer_object)
        return;
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, framebuffer_object);
    cached_fbo_ = framebuffer_object;
    fbo_known_ = true;
    draw_buffer_count_ = -2;
}

void GpuGlStateTracker::SetDrawBufferCount(GpuRenderer & gpu, int color_attachment_count) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    if (!EnsureProcs(gpu))
        return;
#else
    (void)gpu;
#endif
    if (color_attachment_count < -1)
        return;
    if (color_attachment_count == -1) {
        if (draw_buffer_fbo_ == cached_fbo_ && draw_buffer_count_ == -1)
            return;
        draw_buffer_count_ = -1;
        draw_buffer_fbo_ = cached_fbo_;
        return;
    }
    if (draw_buffer_fbo_ == cached_fbo_ && draw_buffer_count_ == color_attachment_count)
        return;
    GLenum bufs[8];
    const int c = (std::min)(color_attachment_count, 8);
    for (int i = 0; i < c; ++i)
        bufs[i] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
    HOGL(glDrawBuffers)(c, bufs);
    draw_buffer_count_ = color_attachment_count;
    draw_buffer_fbo_ = cached_fbo_;
}

void GpuGlStateTracker::SetViewport(int x, int y, int width, int height) noexcept
{
    if (width <= 0 || height <= 0)
        return;
    if (vp_known_ && vp_x_ == x && vp_y_ == y && vp_w_ == width && vp_h_ == height)
        return;
    glViewport(x, y, width, height);
    vp_x_ = x;
    vp_y_ = y;
    vp_w_ = width;
    vp_h_ = height;
    vp_known_ = true;
}

void GpuGlStateTracker::SetDepthState(bool test_enabled, bool depth_write, GpuDepthFunc depth_func) noexcept
{
    if (depth_known_ && depth_test_ == test_enabled && depth_write_ == depth_write && depth_func_ == depth_func)
        return;
    if (test_enabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(static_cast<GLboolean>(depth_write ? GL_TRUE : GL_FALSE));
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    glDepthFunc(MapGpuDepthFunc(depth_func));
    depth_test_ = test_enabled;
    depth_write_ = depth_write;
    depth_func_ = depth_func;
    depth_known_ = true;
}

void GpuGlStateTracker::SetScissorEnabled(bool enabled, int x, int y, int width, int height) noexcept
{
    if (!enabled) {
        if (scissor_known_ && !scissor_on_)
            return;
        glDisable(GL_SCISSOR_TEST);
        scissor_on_ = false;
        scissor_known_ = true;
        return;
    }
    if (width <= 0 || height <= 0)
        return;
    if (scissor_known_ && scissor_on_ && scissor_x_ == x && scissor_y_ == y && scissor_w_ == width && scissor_h_ == height)
        return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
    scissor_on_ = true;
    scissor_x_ = x;
    scissor_y_ = y;
    scissor_w_ = width;
    scissor_h_ = height;
    scissor_known_ = true;
}

void GpuGlStateTracker::SetColorMask(bool red, bool green, bool blue, bool alpha) noexcept
{
    if (cm_known_ && cm_r_ == red && cm_g_ == green && cm_b_ == blue && cm_a_ == alpha)
        return;
    glColorMask(
        static_cast<GLboolean>(red ? GL_TRUE : GL_FALSE),
        static_cast<GLboolean>(green ? GL_TRUE : GL_FALSE),
        static_cast<GLboolean>(blue ? GL_TRUE : GL_FALSE),
        static_cast<GLboolean>(alpha ? GL_TRUE : GL_FALSE));
    cm_r_ = red;
    cm_g_ = green;
    cm_b_ = blue;
    cm_a_ = alpha;
    cm_known_ = true;
}

int GpuGlStateTracker::BeginPass(
    GpuRenderer & gpu,
    GpuRenderTarget * target,
    GpuRenderPassBeginInfo const & info,
    int surface_width,
    int surface_height) noexcept
{
    if (!GlShaderFamilyOk(gpu.App()))
        return static_cast<int>(GpuRenderPassEncoderResult::UNSUPPORTED_BACKEND);
    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuRenderPassEncoderResult::INVALID_ARGUMENT);
#if HONKORDGL_PLATFORM_WINDOWS
    if (!EnsureProcs(gpu))
        return static_cast<int>(GpuRenderPassEncoderResult::UNSUPPORTED_BACKEND);
#endif

    if (target) {
        if (!target->FramebufferObject() || target->Width() <= 0)
            return static_cast<int>(GpuRenderPassEncoderResult::INVALID_ARGUMENT);
        BindFramebuffer(gpu, target->FramebufferObject());
        SetDrawBufferCount(gpu, target->ColorCount());
        SetViewport(0, 0, target->Width(), target->Height());
    } else {
        if (surface_width <= 0 || surface_height <= 0)
            return static_cast<int>(GpuRenderPassEncoderResult::INVALID_ARGUMENT);
        BindFramebuffer(gpu, 0);
        SetDrawBufferCount(gpu, -1);
        SetViewport(0, 0, surface_width, surface_height);
    }

    const bool has_depth = !target || target->DepthStencilFormat() != GpuDepthStencilFormat::None;
    const bool dt = info.enable_depth_test != 0 && has_depth;
    const bool dw = dt && info.depth_write != 0;
    SetDepthState(dt, dw, info.depth_func);

    return GlApplyPassClears(gpu, target, info);
}

void GpuRenderGraph::Clear() noexcept
{
    passes_.clear();
    edges_.clear();
    order_.clear();
    compiled_ = false;
}

void GpuRenderGraph::Reserve(std::size_t pass_capacity)
{
    passes_.reserve(pass_capacity);
}

void GpuRenderGraph::AddPass(GpuRenderGraphPass const & pass)
{
    passes_.push_back(pass);
    compiled_ = false;
}

void GpuRenderGraph::AddDependency(int prerequisite_pass_index, int dependent_pass_index)
{
    edges_.emplace_back(prerequisite_pass_index, dependent_pass_index);
    compiled_ = false;
}

int GpuRenderGraph::Compile() noexcept
{
    const int n = static_cast<int>(passes_.size());
    if (n == 0) {
        order_.clear();
        compiled_ = true;
        return static_cast<int>(GpuRenderGraphResult::OK);
    }
    for (auto const & e : edges_) {
        if (e.first < 0 || e.second < 0 || e.first >= n || e.second >= n || e.first == e.second)
            return static_cast<int>(GpuRenderGraphResult::INVALID_ARGUMENT);
    }

    if (edges_.empty()) {
        order_.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            order_[static_cast<std::size_t>(i)] = i;
        compiled_ = true;
        return static_cast<int>(GpuRenderGraphResult::OK);
    }

    std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
    std::vector<int> indeg(static_cast<std::size_t>(n), 0);
    for (auto const & e : edges_) {
        adj[static_cast<std::size_t>(e.first)].push_back(e.second);
        indeg[static_cast<std::size_t>(e.second)]++;
    }

    std::set<int> ready;
    for (int i = 0; i < n; ++i) {
        if (indeg[static_cast<std::size_t>(i)] == 0)
            ready.insert(i);
    }

    order_.clear();
    while (!ready.empty()) {
        int const u = *ready.begin();
        ready.erase(ready.begin());
        order_.push_back(u);
        for (int v : adj[static_cast<std::size_t>(u)]) {
            indeg[static_cast<std::size_t>(v)]--;
            if (indeg[static_cast<std::size_t>(v)] == 0)
                ready.insert(v);
        }
    }

    if (static_cast<int>(order_.size()) != n)
        return static_cast<int>(GpuRenderGraphResult::CYCLIC_DEPENDENCY);

    compiled_ = true;
    return static_cast<int>(GpuRenderGraphResult::OK);
}

int GpuRenderGraph::Execute(
    GpuRenderer & gpu,
    GpuGlStateTracker & tracker,
    void (*per_pass)(void * user_data, int pass_index, GpuRenderGraphPass const & pass),
    void * user_data) noexcept
{
    if (passes_.empty())
        return static_cast<int>(GpuRenderGraphResult::OK);
    if (!GlShaderFamilyOk(gpu.App()))
        return static_cast<int>(GpuRenderGraphResult::UNSUPPORTED_BACKEND);
    if (gpu.MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(GpuRenderGraphResult::INVALID_ARGUMENT);

    if (!compiled_) {
        int const cr = Compile();
        if (cr != static_cast<int>(GpuRenderGraphResult::OK))
            return cr;
    }

    for (int idx : order_) {
        GpuRenderGraphPass const & p = passes_[static_cast<std::size_t>(idx)];
        int const br = tracker.BeginPass(gpu, p.target, p.begin, p.surface_width, p.surface_height);
        if (br != static_cast<int>(GpuRenderPassEncoderResult::OK))
            return br;
        if (per_pass)
            per_pass(user_data, idx, p);
    }

    tracker.BindFramebuffer(gpu, 0);
    return static_cast<int>(GpuRenderGraphResult::OK);
}

} // namespace HonkordGL::Graphics
