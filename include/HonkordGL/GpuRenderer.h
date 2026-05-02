/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GPU renderer bound to an application / window context
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPURENDERER_H
#define HONKORDGL_GPURENDERER_H

#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/RendererContext.h>

#include <cstddef>

namespace HonkordGL::Graphics {

/** Short ASCII label for the given backend (`"OPENGL"`, `"VULKAN"`, …). */
HONKORDGL_API HONKORDGL_ND const char * RendererBackendLabel(Renderers backend) noexcept;

/**
 * High-level renderer: attaches a graphics backend to an existing `ApplicationContextSettings`
 * (typically after `WindowBackend::CreateWindow`).
 *
 * Implementation uses the host platform’s native graphics APIs internally; this header does not
 * expose vendor graphics SDK types or entry points.
 *
 * Does not own the window context; `app` must outlive this object.
 */
class HONKORDGL_API GpuRenderer {
public:
    GpuRenderer() noexcept = default;
    explicit GpuRenderer(ApplicationContextSettings& app) noexcept;

    GpuRenderer(const GpuRenderer&) = delete;
    GpuRenderer& operator=(const GpuRenderer&) = delete;
    GpuRenderer(GpuRenderer&& other) noexcept;
    GpuRenderer& operator=(GpuRenderer&& other) noexcept;
    ~GpuRenderer() noexcept;

    /** Target context for subsequent `Create` / `MakeCurrent` / `SwapBuffers`. Replaces binding; destroys any active GPU context first. */
    void Bind(ApplicationContextSettings& app) noexcept;

    /** Clears binding without destroying the native context (context remains attached to `app`). */
    void Unbind() noexcept;

    /** Bound window/application context, or nullptr after `Unbind()`. */
    HONKORDGL_ND ApplicationContextSettings * App() const noexcept { return app_; }

    /**
     * Attaches a rendering API into the bound `ApplicationContextSettings`.
     * @return `RendererContextResult::OK` (0) on success; otherwise same values as `AttachRendererContext`.
     */
    HONKORDGL_ND int Create(const RendererContextSettings& spec = RendererContextSettings{}) noexcept;

    /**
     * Picks a working attach without manual `RendererContextSettings` tuning.
     * On X11-style platforms (`NativePlatformUsesX11ClientHandles`): tries `OPENGL` (GLX) first, then `EGL` if that fails;
     * if `HONKORDGL_USE_EGL=1` is set, tries EGL first, then GLX. Other platforms: same as `Create()` with default settings.
     */
    HONKORDGL_ND int CreateAutomatic() noexcept;

    /**
     * Probes built-in GPU backends in a platform-specific order until one attaches successfully.
     * @return `RendererContextResult::OK` (0) when a backend is attached; otherwise the last error from the final attempt.
     */
    HONKORDGL_ND int CreateBestAvailable() noexcept;

    /**
     * Attaches a shading-capable raster backend (`Renderers::OPENGL` or `Renderers::EGL`), skipping low-level APIs
     * that do not run user shader code in this build (e.g. skips Vulkan / Metal fallback paths).
     * If attach succeeds but the backend cannot run shaders from this API, detaches and returns `UNSUPPORTED_BACKEND`.
     */
    HONKORDGL_ND int CreateBestAvailableShaderBackend() noexcept;

    /**
     * Attaches the Vulkan swapchain path (`Renderers::VULKAN`). Linux desktop (X11 / Wayland) only in this build.
     */
    HONKORDGL_ND int CreateVulkan() noexcept;

    /**
     * Attaches the Metal swapchain path (`Renderers::METAL`). macOS (Cocoa) only.
     */
    HONKORDGL_ND int CreateMetal() noexcept;

    /**
     * Probes raster backends in a fixed cross-platform order until one attaches successfully.
     * Order (simplified): Linux desktop — Vulkan, then OpenGL (GLX), then EGL; macOS — Metal, then OpenGL;
     * Windows — Direct3D 11, then OpenGL (WGL); others — `CreateAutomatic()`.
     * The active backend may not support `CreateTexture2DRgba8` (e.g. Vulkan/Metal); use
     * `CreateBestAvailableForCpuFramebufferPresent` when you need OpenGL-family texture upload.
     */
    HONKORDGL_ND int CreateBestAvailableExhaustive() noexcept;

    /**
     * Attaches an OpenGL / OpenGL ES / EGL context suitable for texture upload and blit (`CreateTexture2DRgba8`).
     * Tries platform defaults (GLX/WGL/NSOpenGL, then EGL on X11/Wayland where applicable). Skips Vulkan and Metal.
     */
    HONKORDGL_ND int CreateBestAvailableForCpuFramebufferPresent() noexcept;

    /** True when the active backend supports OpenGL-family texture APIs (`CreateTexture2DRgba8`, etc.). */
    HONKORDGL_ND bool SupportsTextureRgbUpload() const noexcept;

    /** Detaches the renderer from the bound context if this instance had a context attached. Idempotent. */
    void Destroy() noexcept;

    HONKORDGL_ND bool IsCreated() const noexcept;

    /** Backend selected by the last successful `Create*` call, or `Renderers::NONE` if not created / unbound. */
    HONKORDGL_ND Renderers ActiveBackend() const noexcept;

    /** Same as `RendererBackendLabel(ActiveBackend())`. */
    HONKORDGL_ND const char * ActiveBackendLabel() const noexcept;

    /** Active rendering API. Same as `ActiveBackend()`. */
    HONKORDGL_ND Renderers CurrentRenderer() const noexcept { return ActiveBackend(); }

    /** Native graphics device / context object from the bound context (`ApplicationContextSettings::device`). */
    HONKORDGL_ND HonkordGL_GW_Handle NativeGpuDevice() const noexcept { return Device(); }

    /** Native handle stored on `ApplicationContextSettings::device`. Null if unbound or not yet attached. */
    HONKORDGL_ND HonkordGL_GW_Handle Device() const noexcept { return app_ ? app_->device : nullptr; }

    /**
     * Human-readable adapter / renderer description (e.g. hardware model or software rasterizer name).
     * Not available for every backend in this build (`UNSUPPORTED_BACKEND` when unsupported).
     */
    HONKORDGL_ND int GetAdapterRendererDescription(char * buf, std::size_t bufBytes) noexcept;

    /**
     * Ensures runtime-resolved compiler entry points are available on platforms that load them dynamically.
     * @return `GpuShaderCompileError` as int (`OK` or `PROC_LOAD_FAILED`).
     */
    HONKORDGL_ND int LoadShaderCompilerProcs() noexcept;

    /** Resolves a graphics entry point by name for the current context. */
    HONKORDGL_ND void * GetGraphicsProc(const char * name) noexcept;

    /**
     * Copies an adapter information string into `buf` (null-terminated when `bufBytes > 0`).
     * @return 0 on success; `GpuShaderCompileError::UNSUPPORTED_BACKEND` when not applicable;
     *         `RendererContextResult::INVALID_ARGUMENT` if unavailable; or `RendererContextMakeCurrent` error.
     */
    HONKORDGL_ND int GetAdapterString(GpuAdapterStringId which, char * buf, std::size_t bufBytes) noexcept;

    HONKORDGL_ND int GetAdapterVendor(char * buf, std::size_t bufBytes) noexcept;
    HONKORDGL_ND int GetAdapterVersion(char * buf, std::size_t bufBytes) noexcept;

    /**
     * Allocates an RGBA8 2D texture; uploads `pixels` when non-null (`w*h*4` bytes).
     * @return 0 on success; error codes from `GpuShaderCompileError` / `RendererContextResult` on failure.
     */
    HONKORDGL_ND int CreateTexture2DRgba8(
        int width,
        int height,
        void const * pixels,
        GpuObjectName * outTexture) noexcept;

    /**
     * Updates an existing RGBA8 texture (same dimensions as prior upload). Requires OpenGL-family backend.
     * @return Same error codes as `CreateTexture2DRgba8`.
     */
    HONKORDGL_ND int UpdateTexture2DRgba8(
        GpuObjectName texture,
        int width,
        int height,
        void const * pixels) noexcept;

    /** Binds a 2D texture to texture unit `textureUnit` (0-based) for drawing. */
    void BindTexture2D(unsigned int textureUnit, GpuObjectName textureName) noexcept;

    /** Releases GPU storage for a texture (no-op if `textureName` is 0). */
    void DestroyTexture(GpuObjectName textureName) noexcept;

    /** Installs `program` for subsequent draws (use 0 to unbind). */
    void BindShaderProgram(GpuObjectName program) noexcept;

    HONKORDGL_ND int CreateShaderProgram(
        const char * vertexSource,
        const char * fragmentSource,
        GpuObjectName * outProgram,
        char * infoLog,
        std::size_t infoLogBytes) noexcept
    {
        return CompileShaderProgram(vertexSource, fragmentSource, outProgram, infoLog, infoLogBytes);
    }

    HONKORDGL_ND int CreateShaderProgram(
        const char * vertexSource,
        const char * fragmentSource,
        GpuObjectName * outProgram,
        char * infoLog,
        std::size_t infoLogBytes,
        const int * attribLocations,
        const char * const * attribNames,
        int attribCount) noexcept
    {
        return CompileShaderProgram(
            vertexSource,
            fragmentSource,
            outProgram,
            infoLog,
            infoLogBytes,
            attribLocations,
            attribNames,
            attribCount);
    }

    /** @return Same as `RendererContextMakeCurrent` (0 = success). */
    HONKORDGL_ND int MakeCurrent() noexcept;

    void SwapBuffers() noexcept;

    /** Clears the color buffer to `rgba` after `MakeCurrent`. No-op for backends without a color buffer here. */
    void ClearColorBuffer(float r, float g, float b, float a) noexcept;

    /**
     * Sets the drawable viewport from `frame_buffer_width` / `frame_buffer_height`, falling back to `client_rect` size.
     * No-op if dimensions are non-positive. Requires the renderer context to be current (`MakeCurrent`).
     */
    void SetDefaultViewport() noexcept;

    /**
     * Sets the viewport rectangle. No-op for backends that do not use a 2D viewport here, or if `width` / `height` are not positive.
     */
    void SetViewport(int x, int y, int width, int height) noexcept;

    /**
     * Compiles and links shader sources into a program object.
     * Calls `MakeCurrent`, then `GpuShaderCompileProgram`.
     */
    HONKORDGL_ND int CompileShaderProgram(
        const char * vertexSource,
        const char * fragmentSource,
        GpuObjectName * outProgram,
        char * infoLog,
        std::size_t infoLogBytes) noexcept;

    /** Optional attribute location bindings before link (`attribCount` may be 0). */
    HONKORDGL_ND int CompileShaderProgram(
        const char * vertexSource,
        const char * fragmentSource,
        GpuObjectName * outProgram,
        char * infoLog,
        std::size_t infoLogBytes,
        const int * attribLocations,
        const char * const * attribNames,
        int attribCount) noexcept;

    /** Destroys a program object created by `CompileShaderProgram`. */
    void DeleteShaderProgram(GpuObjectName program) noexcept;

    /**
     * Low-level GLSL pipeline (OpenGL / GLES): create shader object → set source → compile → create program →
     * attach → optional `BindAttribLocation` → link. Requires `LoadShaderCompilerProcs` / current context on
     * platforms that resolve entry points at runtime. Use `BindShaderProgram` to activate a linked program for drawing.
     */
    HONKORDGL_ND int CreateShaderObject(GpuShaderPipelineStage stage, GpuObjectName * outShader) noexcept;
    HONKORDGL_ND int SetShaderSource(GpuObjectName shader, const char * source) noexcept;
    HONKORDGL_ND int CompileShaderObject(GpuObjectName shader, char * infoLog, std::size_t infoLogBytes) noexcept;
    HONKORDGL_ND int CreateProgramObject(GpuObjectName * outProgram) noexcept;
    void AttachShader(GpuObjectName program, GpuObjectName shader) noexcept;
    /** Maps a vertex attribute name to a fixed location before `LinkShaderObjects` (`glBindAttribLocation`). */
    HONKORDGL_ND int BindAttribLocation(GpuObjectName program, unsigned int location, const char * name) noexcept;
    HONKORDGL_ND int LinkShaderObjects(GpuObjectName program, char * infoLog, std::size_t infoLogBytes) noexcept;
    void DeleteShaderObject(GpuObjectName shader) noexcept;

    /** Creates a generic GPU buffer object (`Vertex` / `Index`). */
    HONKORDGL_ND int CreateBuffer(GpuObjectName * outBuffer) noexcept;

    /** Deletes a buffer object created by `CreateBuffer`. */
    void DestroyBuffer(GpuObjectName buffer) noexcept;

    /** Binds buffer to the given target (`Vertex`/`Index`), or 0 to unbind. */
    HONKORDGL_ND int BindBuffer(GpuBufferTarget target, GpuObjectName buffer) noexcept;

    /** Uploads raw bytes to currently bound buffer target. */
    HONKORDGL_ND int UploadBufferData(
        GpuBufferTarget target,
        const void * data,
        std::size_t byteSize,
        GpuBufferUsage usage = GpuBufferUsage::Static) noexcept;

    /** Creates a vertex array object used to bind vertex/index stream layout. */
    HONKORDGL_ND int CreateVertexArray(GpuObjectName * outVertexArray) noexcept;

    /** Deletes a vertex array object created by `CreateVertexArray`. */
    void DestroyVertexArray(GpuObjectName vertexArray) noexcept;

    /** Binds a vertex array object, or 0 to unbind. */
    HONKORDGL_ND int BindVertexArray(GpuObjectName vertexArray) noexcept;

    /** Enables a vertex attribute slot on the currently bound vertex array. */
    HONKORDGL_ND int EnableVertexAttribute(unsigned int location) noexcept;

    /** Defines float attribute layout for the currently bound VAO and vertex buffer. */
    HONKORDGL_ND int SetVertexAttributeFloat(
        unsigned int location,
        int components,
        int strideBytes,
        std::size_t offsetBytes,
        bool normalized = false) noexcept;

    /** Enables/disables depth testing (`glEnable(GL_DEPTH_TEST)` equivalent). */
    void SetDepthTestEnabled(bool enabled) noexcept;

    /** Draws non-indexed primitives from currently bound vertex array/program. */
    HONKORDGL_ND int DrawArrays(GpuPrimitive primitive, int firstVertex, int vertexCount) noexcept;

    /** Draws indexed primitives from currently bound VAO/index buffer/program. */
    HONKORDGL_ND int DrawIndexed(
        GpuPrimitive primitive,
        int indexCount,
        GpuIndexType indexType,
        std::size_t indexOffsetBytes = 0) noexcept;

private:
    ApplicationContextSettings * app_{nullptr};
    bool owns_attachment_{false};
};

} // namespace HonkordGL::Graphics

#endif