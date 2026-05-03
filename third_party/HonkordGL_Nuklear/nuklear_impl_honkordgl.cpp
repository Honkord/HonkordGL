/**
 * Nuklear — HonkordGL GpuRenderer (OpenGL family) backend.
 */

#include "nuklear_impl_honkordgl.h"

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

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
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#ifdef Status
#undef Status
#endif
#endif

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuBufferTarget;
using HonkordGL::Graphics::GpuBufferUsage;
using HonkordGL::Graphics::GpuObjectName;
using HonkordGL::Graphics::GpuShaderCompileError;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::Renderers;
using HonkordGL::Graphics::RendererContextResult;

namespace {

struct NkHonkVertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

static struct nk_draw_vertex_layout_element const g_vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct NkHonkVertex, position)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct NkHonkVertex, uv)},
    {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct NkHonkVertex, col)},
    {NK_VERTEX_LAYOUT_END},
};

struct Nuklear_ImplHonkordGL_Data {
    ApplicationContextSettings * App{nullptr};
    GpuRenderer * Gpu{nullptr};
    nk_context * Ctx{nullptr};

    nk_font_atlas Atlas{};
    nk_font * Font{nullptr};
    GpuObjectName FontTex{};
    nk_draw_null_texture NullTex{};

    nk_convert_config ConvertCfg{};
    nk_buffer Cmds{};
    nk_buffer VBuf{};
    nk_buffer EBuf{};

    GpuObjectName Shader{};
    GpuObjectName Vao{};
    GpuObjectName Vbo{};
    GpuObjectName Ibo{};
    GLint LocProjMtx{-1};
    GLint LocTexture{-1};
};

static Nuklear_ImplHonkordGL_Data * g_bd = nullptr;

static bool GlslPathOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    int const ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}

#if HONKORDGL_PLATFORM_WINDOWS
using PFN_glDrawElementsFn =
    void(APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void * indices);
using PFN_glEnable = void(APIENTRY *)(GLenum);
using PFN_glDisable = void(APIENTRY *)(GLenum);
using PFN_glBlendEquation = void(APIENTRY *)(GLenum);
using PFN_glBlendFuncSeparate = void(APIENTRY *)(GLenum, GLenum, GLenum, GLenum);
using PFN_glViewport = void(APIENTRY *)(GLint, GLint, GLsizei, GLsizei);
using PFN_glScissor = void(APIENTRY *)(GLint, GLint, GLsizei, GLsizei);
using PFN_glActiveTexture = void(APIENTRY *)(GLenum);
using PFN_glBindTexture = void(APIENTRY *)(GLenum, GLuint);
using PFN_glUseProgram = void(APIENTRY *)(GLuint);
using PFN_glUniform1i = void(APIENTRY *)(GLint, GLint);
using PFN_glUniformMatrix4fv = void(APIENTRY *)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glBindBuffer = void(APIENTRY *)(GLenum, GLuint);
using PFN_glEnableVertexAttribArray = void(APIENTRY *)(GLuint);
using PFN_glDisableVertexAttribArray = void(APIENTRY *)(GLuint);
using PFN_glVertexAttribPointer = void(APIENTRY *)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
#endif

static bool EnsureDeviceObjects(Nuklear_ImplHonkordGL_Data * bd)
{
    if (!bd || !bd->Gpu || !GlslPathOk(bd->App))
        return false;
    return Nuklear_ImplHonkordGL_CreateDeviceObjects(bd->Gpu);
}

} // namespace

bool Nuklear_ImplHonkordGL_CreateDeviceObjects(GpuRenderer * gpu)
{
    if (!gpu || !GlslPathOk(g_bd ? g_bd->App : nullptr))
        return false;
    if (!g_bd)
        return false;

    GpuRenderer * const gr = gpu;
    if (gr->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return false;

    Nuklear_ImplHonkordGL_DestroyDeviceObjects(gr);

    static char const kVs[] = R"(#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
uniform mat4 ProjMtx;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}
)";

    static char const kFs[] = R"(#version 330 core
in vec2 Frag_UV;
in vec4 Frag_Color;
uniform sampler2D Texture;
layout (location = 0) out vec4 Out_Color;
void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
)";

    char log[2048]{};
    GpuObjectName prog{};
    if (gr->CompileShaderProgram(kVs, kFs, &prog, log, sizeof(log)) != 0 || prog == 0)
        return false;
    g_bd->Shader = prog;

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glGetUniformLocationFn =
        reinterpret_cast<GLint (*)(GLuint, const GLchar *)>(gr->GetGraphicsProc("glGetUniformLocation"));
    if (!glGetUniformLocationFn)
        return false;
    g_bd->LocProjMtx = glGetUniformLocationFn(static_cast<GLuint>(prog), "ProjMtx");
    g_bd->LocTexture = glGetUniformLocationFn(static_cast<GLuint>(prog), "Texture");
#else
    g_bd->LocProjMtx = glGetUniformLocation(static_cast<GLuint>(prog), "ProjMtx");
    g_bd->LocTexture = glGetUniformLocation(static_cast<GLuint>(prog), "Texture");
#endif

    if (gr->CreateVertexArray(&g_bd->Vao) != 0 || g_bd->Vao == 0)
        return false;
    if (gr->CreateBuffer(&g_bd->Vbo) != 0 || g_bd->Vbo == 0)
        return false;
    if (gr->CreateBuffer(&g_bd->Ibo) != 0 || g_bd->Ibo == 0)
        return false;

    return true;
}

void Nuklear_ImplHonkordGL_DestroyDeviceObjects(GpuRenderer * gpu)
{
    if (!g_bd || !gpu)
        return;
    (void)gpu->MakeCurrent();

    if (g_bd->Vao) {
        gpu->DestroyVertexArray(g_bd->Vao);
        g_bd->Vao = 0;
    }
    if (g_bd->Vbo) {
        gpu->DestroyBuffer(g_bd->Vbo);
        g_bd->Vbo = 0;
    }
    if (g_bd->Ibo) {
        gpu->DestroyBuffer(g_bd->Ibo);
        g_bd->Ibo = 0;
    }
    if (g_bd->Shader) {
        gpu->DeleteShaderProgram(g_bd->Shader);
        g_bd->Shader = 0;
    }
    g_bd->LocProjMtx = -1;
    g_bd->LocTexture = -1;
}

bool Nuklear_ImplHonkordGL_Init(
    ApplicationContextSettings * app,
    GpuRenderer * gpu,
    nk_context * ctx,
    float font_height_pixels)
{
    if (!app || !gpu || !ctx || !gpu->SupportsTextureRgbUpload() || !GlslPathOk(app))
        return false;

    if (gpu->LoadShaderCompilerProcs() != static_cast<int>(GpuShaderCompileError::OK))
        return false;

    if (g_bd)
        Nuklear_ImplHonkordGL_Shutdown(g_bd->Ctx);

    g_bd = new Nuklear_ImplHonkordGL_Data{};
    g_bd->App = app;
    g_bd->Gpu = gpu;
    g_bd->Ctx = ctx;

    nk_font_atlas_init_default(&g_bd->Atlas);
    nk_font_atlas_begin(&g_bd->Atlas);
    g_bd->Font = nk_font_atlas_add_default(&g_bd->Atlas, font_height_pixels, nullptr);

    int img_w = 0;
    int img_h = 0;
    void const * const img = nk_font_atlas_bake(&g_bd->Atlas, &img_w, &img_h, NK_FONT_ATLAS_RGBA32);
    if (!img || img_w <= 0 || img_h <= 0 || !g_bd->Font) {
        nk_font_atlas_clear(&g_bd->Atlas);
        delete g_bd;
        g_bd = nullptr;
        return false;
    }

    if (gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK)) {
        nk_font_atlas_clear(&g_bd->Atlas);
        delete g_bd;
        g_bd = nullptr;
        return false;
    }

    if (gpu->CreateTexture2DRgba8(img_w, img_h, img, &g_bd->FontTex) != 0 || g_bd->FontTex == 0) {
        nk_font_atlas_clear(&g_bd->Atlas);
        delete g_bd;
        g_bd = nullptr;
        return false;
    }

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glBindTextureFn =
        reinterpret_cast<void(APIENTRY *)(GLenum, GLuint)>(gpu->GetGraphicsProc("glBindTexture"));
    auto * const glTexParameteriFn =
        reinterpret_cast<void(APIENTRY *)(GLenum, GLenum, GLint)>(gpu->GetGraphicsProc("glTexParameteri"));
    if (glBindTextureFn && glTexParameteriFn) {
        glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(g_bd->FontTex));
        glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTextureFn(GL_TEXTURE_2D, 0);
    }
#else
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(g_bd->FontTex));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif

    nk_font_atlas_end(&g_bd->Atlas, nk_handle_id(static_cast<int>(g_bd->FontTex)), &g_bd->NullTex);
    nk_font_atlas_cleanup(&g_bd->Atlas);

    nk_buffer_init_default(&g_bd->Cmds);
    nk_buffer_init_default(&g_bd->VBuf);
    nk_buffer_init_default(&g_bd->EBuf);

    nk_convert_config & cfg = g_bd->ConvertCfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.global_alpha = 1.f;
    cfg.line_AA = NK_ANTI_ALIASING_ON;
    cfg.shape_AA = NK_ANTI_ALIASING_ON;
    cfg.circle_segment_count = 22;
    cfg.arc_segment_count = 22;
    cfg.curve_segment_count = 22;
    cfg.null = g_bd->NullTex;
    cfg.vertex_layout = g_vertex_layout;
    cfg.vertex_size = sizeof(struct NkHonkVertex);
    cfg.vertex_alignment = NK_ALIGNOF(struct NkHonkVertex);

    if (nk_init_default(ctx, &g_bd->Font->handle) == 0) {
        nk_buffer_free(&g_bd->Cmds);
        nk_buffer_free(&g_bd->VBuf);
        nk_buffer_free(&g_bd->EBuf);
        nk_font_atlas_clear(&g_bd->Atlas);
        gpu->DestroyTexture(g_bd->FontTex);
        g_bd->FontTex = 0;
        delete g_bd;
        g_bd = nullptr;
        return false;
    }

    if (!EnsureDeviceObjects(g_bd)) {
        nk_free(ctx);
        nk_buffer_free(&g_bd->Cmds);
        nk_buffer_free(&g_bd->VBuf);
        nk_buffer_free(&g_bd->EBuf);
        nk_font_atlas_clear(&g_bd->Atlas);
        gpu->DestroyTexture(g_bd->FontTex);
        g_bd->FontTex = 0;
        delete g_bd;
        g_bd = nullptr;
        return false;
    }

    return true;
}

void Nuklear_ImplHonkordGL_Shutdown(nk_context * ctx)
{
    if (!g_bd || !ctx || ctx != g_bd->Ctx)
        return;

    GpuRenderer * const gpu = g_bd->Gpu;
    if (gpu)
        Nuklear_ImplHonkordGL_DestroyDeviceObjects(gpu);

    nk_buffer_free(&g_bd->Cmds);
    nk_buffer_free(&g_bd->VBuf);
    nk_buffer_free(&g_bd->EBuf);

    nk_free(ctx);

    nk_font_atlas_clear(&g_bd->Atlas);

    if (gpu && g_bd->FontTex != 0) {
        (void)gpu->MakeCurrent();
        gpu->DestroyTexture(g_bd->FontTex);
        g_bd->FontTex = 0;
    }

    delete g_bd;
    g_bd = nullptr;
}

void Nuklear_ImplHonkordGL_Render(ApplicationContextSettings * app, nk_context * ctx, GpuRenderer * gpu)
{
    if (!g_bd || !ctx || !gpu || ctx != g_bd->Ctx || g_bd->Shader == 0)
        return;

    int const fbw = app && app->frame_buffer_width > 0 ? app->frame_buffer_width : (app ? app->client_rect.w : 0);
    int const fbh = app && app->frame_buffer_height > 0 ? app->frame_buffer_height : (app ? app->client_rect.z : 0);
    if (fbw <= 0 || fbh <= 0)
        return;

    nk_buffer_clear(&g_bd->Cmds);
    nk_buffer_clear(&g_bd->VBuf);
    nk_buffer_clear(&g_bd->EBuf);

    nk_flags const conv = nk_convert(ctx, &g_bd->Cmds, &g_bd->VBuf, &g_bd->EBuf, &g_bd->ConvertCfg);
    if (conv != static_cast<nk_flags>(NK_CONVERT_SUCCESS))
        return;

    void const * const vtx_mem = nk_buffer_memory_const(&g_bd->VBuf);
    void const * const idx_mem = nk_buffer_memory_const(&g_bd->EBuf);
    nk_size const vtx_bytes = nk_buffer_total(&g_bd->VBuf);
    nk_size const idx_bytes = nk_buffer_total(&g_bd->EBuf);

    if (!vtx_mem || vtx_bytes == 0 || !idx_mem || idx_bytes == 0)
        return;

    GpuRenderer * const gr = gpu;
    if (gr->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glEnableFn = reinterpret_cast<PFN_glEnable>(gr->GetGraphicsProc("glEnable"));
    auto * const glDisableFn = reinterpret_cast<PFN_glDisable>(gr->GetGraphicsProc("glDisable"));
    auto * const glBlendEquationFn =
        reinterpret_cast<PFN_glBlendEquation>(gr->GetGraphicsProc("glBlendEquation"));
    auto * const glBlendFuncSeparateFn =
        reinterpret_cast<PFN_glBlendFuncSeparate>(gr->GetGraphicsProc("glBlendFuncSeparate"));
    auto * const glViewportFn = reinterpret_cast<PFN_glViewport>(gr->GetGraphicsProc("glViewport"));
    auto * const glScissorFn = reinterpret_cast<PFN_glScissor>(gr->GetGraphicsProc("glScissor"));
    auto * const glActiveTextureFn =
        reinterpret_cast<PFN_glActiveTexture>(gr->GetGraphicsProc("glActiveTexture"));
    auto * const glBindTextureFn =
        reinterpret_cast<PFN_glBindTexture>(gr->GetGraphicsProc("glBindTexture"));
    auto * const glUseProgramFn = reinterpret_cast<PFN_glUseProgram>(gr->GetGraphicsProc("glUseProgram"));
    auto * const glUniform1iFn = reinterpret_cast<PFN_glUniform1i>(gr->GetGraphicsProc("glUniform1i"));
    auto * const glUniformMatrix4fvFn =
        reinterpret_cast<PFN_glUniformMatrix4fv>(gr->GetGraphicsProc("glUniformMatrix4fv"));
    auto * const glBindBufferFn = reinterpret_cast<PFN_glBindBuffer>(gr->GetGraphicsProc("glBindBuffer"));
    auto * const glEnableVertexAttribArrayFn =
        reinterpret_cast<PFN_glEnableVertexAttribArray>(gr->GetGraphicsProc("glEnableVertexAttribArray"));
    auto * const glDisableVertexAttribArrayFn =
        reinterpret_cast<PFN_glDisableVertexAttribArray>(gr->GetGraphicsProc("glDisableVertexAttribArray"));
    auto * const glVertexAttribPointerFn =
        reinterpret_cast<PFN_glVertexAttribPointer>(gr->GetGraphicsProc("glVertexAttribPointer"));
    PFN_glDrawElementsFn const glDrawElementsFn =
        reinterpret_cast<PFN_glDrawElementsFn>(gr->GetGraphicsProc("glDrawElements"));

    if (!glEnableFn || !glDisableFn || !glBlendEquationFn || !glBlendFuncSeparateFn || !glViewportFn
        || !glScissorFn || !glActiveTextureFn || !glBindTextureFn || !glUseProgramFn
        || !glUniformMatrix4fvFn || !glUniform1iFn || !glBindBufferFn || !glEnableVertexAttribArrayFn
        || !glVertexAttribPointerFn || !glDrawElementsFn)
        return;

    glEnableFn(GL_BLEND);
    glBlendEquationFn(GL_FUNC_ADD);
    glBlendFuncSeparateFn(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisableFn(GL_CULL_FACE);
    glDisableFn(GL_DEPTH_TEST);
    glDisableFn(GL_STENCIL_TEST);
    glEnableFn(GL_SCISSOR_TEST);
    glViewportFn(0, 0, fbw, fbh);

    float const L = 0.f;
    float const R = static_cast<float>(fbw);
    float const T = 0.f;
    float const B = static_cast<float>(fbh);
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgramFn(static_cast<GLuint>(g_bd->Shader));
    glUniform1iFn(g_bd->LocTexture, 0);
    glUniformMatrix4fvFn(g_bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (gr->BindVertexArray(g_bd->Vao) != 0)
        return;

    if (gr->BindBuffer(GpuBufferTarget::Vertex, g_bd->Vbo) != 0)
        return;
    if (gr->UploadBufferData(GpuBufferTarget::Vertex, vtx_mem, vtx_bytes, GpuBufferUsage::Stream) != 0)
        return;

    if (gr->BindBuffer(GpuBufferTarget::Index, g_bd->Ibo) != 0)
        return;
    if (gr->UploadBufferData(GpuBufferTarget::Index, idx_mem, idx_bytes, GpuBufferUsage::Stream) != 0)
        return;

    if (gr->BindVertexArray(g_bd->Vao) != 0)
        return;
    glBindBufferFn(GL_ARRAY_BUFFER, static_cast<GLuint>(g_bd->Vbo));
    glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(g_bd->Ibo));
    glEnableVertexAttribArrayFn(0);
    glEnableVertexAttribArrayFn(1);
    glEnableVertexAttribArrayFn(2);
    glVertexAttribPointerFn(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(struct NkHonkVertex), reinterpret_cast<void *>(offsetof(struct NkHonkVertex, position)));
    glVertexAttribPointerFn(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(struct NkHonkVertex), reinterpret_cast<void *>(offsetof(struct NkHonkVertex, uv)));
    glVertexAttribPointerFn(
        2,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(struct NkHonkVertex),
        reinterpret_cast<void *>(offsetof(struct NkHonkVertex, col)));

    GLenum const idx_gl_type = sizeof(nk_draw_index) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

    void const * idx_offset_ptr_win = nullptr;

    const struct nk_draw_command *cmd_win = nullptr;
    nk_draw_foreach(cmd_win, ctx, &g_bd->Cmds)
    {
        if (!cmd_win || cmd_win->elem_count == 0)
            continue;

        glScissorFn(
            static_cast<GLint>(cmd_win->clip_rect.x),
            static_cast<GLint>(static_cast<float>(fbh) - (cmd_win->clip_rect.y + cmd_win->clip_rect.h)),
            static_cast<GLsizei>(cmd_win->clip_rect.w),
            static_cast<GLsizei>(cmd_win->clip_rect.h));

        glActiveTextureFn(GL_TEXTURE0);
        glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(cmd_win->texture.id));

        glDrawElementsFn(GL_TRIANGLES, static_cast<GLsizei>(cmd_win->elem_count), idx_gl_type, idx_offset_ptr_win);

        idx_offset_ptr_win = reinterpret_cast<void const *>(
            reinterpret_cast<std::uintptr_t>(idx_offset_ptr_win)
            + static_cast<std::uintptr_t>(cmd_win->elem_count) * sizeof(nk_draw_index));
    }

    glDisableVertexAttribArrayFn(0);
    glDisableVertexAttribArrayFn(1);
    glDisableVertexAttribArrayFn(2);
    gr->BindShaderProgram(0);

    if (glDisableFn)
        glDisableFn(GL_SCISSOR_TEST);
    glViewportFn(0, 0, fbw, fbh);

#else
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glViewport(0, 0, fbw, fbh);

    float const L = 0.f;
    float const R = static_cast<float>(fbw);
    float const T = 0.f;
    float const B = static_cast<float>(fbh);
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgram(static_cast<GLuint>(g_bd->Shader));
    glUniform1i(g_bd->LocTexture, 0);
    glUniformMatrix4fv(g_bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (gr->BindVertexArray(g_bd->Vao) != 0)
        return;

    if (gr->BindBuffer(GpuBufferTarget::Vertex, g_bd->Vbo) != 0)
        return;
    if (gr->UploadBufferData(GpuBufferTarget::Vertex, vtx_mem, vtx_bytes, GpuBufferUsage::Stream) != 0)
        return;

    if (gr->BindBuffer(GpuBufferTarget::Index, g_bd->Ibo) != 0)
        return;
    if (gr->UploadBufferData(GpuBufferTarget::Index, idx_mem, idx_bytes, GpuBufferUsage::Stream) != 0)
        return;

    if (gr->BindVertexArray(g_bd->Vao) != 0)
        return;
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(g_bd->Vbo));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(g_bd->Ibo));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(struct NkHonkVertex), reinterpret_cast<void *>(offsetof(struct NkHonkVertex, position)));
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(struct NkHonkVertex), reinterpret_cast<void *>(offsetof(struct NkHonkVertex, uv)));
    glVertexAttribPointer(
        2,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(struct NkHonkVertex),
        reinterpret_cast<void *>(offsetof(struct NkHonkVertex, col)));

    GLenum const idx_gl_type = sizeof(nk_draw_index) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

    void const * idx_offset_ptr = nullptr;

    const struct nk_draw_command *cmd_gl = nullptr;
    nk_draw_foreach(cmd_gl, ctx, &g_bd->Cmds)
    {
        if (!cmd_gl || cmd_gl->elem_count == 0)
            continue;

        glScissor(
            static_cast<GLint>(cmd_gl->clip_rect.x),
            static_cast<GLint>(static_cast<float>(fbh) - (cmd_gl->clip_rect.y + cmd_gl->clip_rect.h)),
            static_cast<GLsizei>(cmd_gl->clip_rect.w),
            static_cast<GLsizei>(cmd_gl->clip_rect.h));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(cmd_gl->texture.id));

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd_gl->elem_count), idx_gl_type, idx_offset_ptr);

        idx_offset_ptr = reinterpret_cast<void const *>(
            reinterpret_cast<std::uintptr_t>(idx_offset_ptr)
            + static_cast<std::uintptr_t>(cmd_gl->elem_count) * sizeof(nk_draw_index));
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    gr->BindShaderProgram(0);

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, fbw, fbh);
#endif
}
