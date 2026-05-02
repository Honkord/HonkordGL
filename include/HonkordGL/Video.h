/**
    * @author Honkord <https://github.com>

    HonkordGL
    Copyright (C) 2026 Honkord 
    (See LICENSE.md for what this file is licensed under)
*/

#ifndef __HonkordGL_Video
#define __HonkordGL_Video

/**
 * @file Video.h
 * @brief High-level `Window` wrapper, `GpuRenderer`, `SoftwareRenderer` (CPU present path), `Software2DContext` / `SoftwareBlitCollector`, and diagnostics (`SetInternalApiError`, etc.).
 */

#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdint>
#include <memory>

namespace HonkordGL
{
namespace Graphics 
{
    class WindowBackend;
    //////////////////////////////////////////////////////
    using namespace HonkordGL;

    //////////////////////////////////////////////////////
    //      Window Flags and Window
    //////////////////////////////////////////////////////
    /** Bit-style window capability tags (reserved for future window policy APIs). */
    enum class WindowFlags : unsigned int {
        FULLSCREEN,          
        OPENGL,              
        OCCLUDED,             
        HIDDEN,              
        BORDERLESS,           
        RESIZABLE = 0,            
        MINIMIZED,            
        MAXIMIZED,           
        MOUSE_GRABBED,        
        INPUT_FOCUS,        
        MOUSE_FOCUS,         
        EXTERNAL,             
        MODAL,               
        HIGH_PIXEL_DENSITY,   
        MOUSE_CAPTURE,       
        MOUSE_RELATIVE_MODE, 
        ALWAYS_ON_TOP,       
        UTILITY,             
        TOOLTIP,            
        POPUP_MENU,          
        KEYBOARD_GRABBED,    
        FILL_DOCUMENT,        
        VULKAN,             
        METAL,              
        TRANSPARENT,        
        NOT_FOCUSABLE    
    };

    /**
     * @brief Owns a native window through `WindowBackend`: `ApplicationContextSettings`, display init, and teardown.
     */
    HONKORDGL_API class Window 
    {
    public:
        /**
         * @brief Constructs a window object with empty settings; call `SetWindowSettings` / `CreateWindow` or use `InitializeWindowSubsystem()`.
         */
        Window() noexcept;

        /**
         * @brief Copies the given settings into this window’s internal state (does not open a native window until `CreateWindow`).
         */
        explicit Window(struct Graphics::ApplicationContextSettings& settings) noexcept;

        /**
         * @brief Uses an existing, already-initialized `WindowBackend` (shared display connection) for multiple top-level windows.
         * This `Window` does not own the backend: `TerminateWindow` only destroys this window’s native surface, not `Shutdown` on the backend.
         */
        Window(Graphics::WindowBackend& shared_backend, struct Graphics::ApplicationContextSettings& settings) noexcept;

        Window(const Window&) = delete;
        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;

        /**
         * @brief Destroys the native window and shuts down the bound display backend if this instance owned them.
         */
        virtual ~Window() noexcept;

        /**
         * @brief Returns whether a native window handle is currently set (`window_handle` non-null).
         */
        virtual HONKORDGL_ND bool WindowIsOpen() const noexcept;

        /**
         * @brief Replaces this window’s stored `ApplicationContextSettings` with `settings` (no native update until `CreateWindow` / `ResetApplication`).
         */
        virtual void SetWindowSettings(struct Graphics::ApplicationContextSettings& settings) noexcept;

        /**
         * @brief Ensures the display backend is initialized, then creates the native window from `RetrieveCurrentSettings()` (`window_title`, `client_rect` w/z as size).
         */
        virtual void CreateWindow() noexcept;

        /**
         * @brief Sets a built-in cursor for this window (maps to the active backend).
         */
        virtual void SetCursorKind(Graphics::CursorKind kind) noexcept;

        /**
         * @brief Sets a cursor from RGBA8 pixel data (`CursorImageParams`: size, stride, hotspot, optional display size).
         * Returns false only for invalid parameters; otherwise the backend applies the bitmap or falls back to the default arrow (see `WindowBackend::SetCursorFromPixels`).
         */
        virtual HONKORDGL_ND bool SetCursorFromPixels(const Graphics::CursorImageParams& params) noexcept;

        /**
         * @brief Loads an image file (stb_image) and sets it as the cursor; hotspot / display size come from `params`.
         */
        virtual HONKORDGL_ND bool SetCursorFromImageFile(const char * path, const Graphics::CursorImageParams& params) noexcept;

        /** @brief Restores the default arrow cursor. */
        virtual void ResetCursor() noexcept;

        /**
         * @brief Applies new application settings at runtime.
         *
         * If a window already exists: merges `settings` into the live context while preserving native handles
         * (`window_handle`, device, surface), then updates title and geometry via the backend without destroy/recreate.
         * If no window exists yet: stores the settings and creates a window when `client_rect` width/height are set.
         */
        virtual void ResetApplication(struct Graphics::ApplicationContextSettings& settings) noexcept;

        /**
         * @brief Mutable reference to this window’s live `ApplicationContextSettings` (title, rects, handles, callbacks).
         */
        virtual HONKORDGL_ND Graphics::ApplicationContextSettings& RetrieveCurrentSettings() const noexcept;

        /**
         * @brief Closes the native window, disconnects the display, and resets the backend so `Initialize` can run again.
         */
        virtual void TerminateWindow() noexcept;

        /**
         * @brief Sets target FPS for `DelayFrame()` / `DelayFPS()` (<= 0 disables frame pacing sleep).
         */
        virtual void SetTargetFPS(int fps) noexcept;

        /**
         * @brief Sleeps the remainder of the target frame budget (same as `DelayFPS`; preferred name).
         */
        virtual void DelayFrame() noexcept;

        /**
         * @brief Sleeps the remainder of the target frame budget set by `SetTargetFPS`.
         */
        virtual void DelayFPS() noexcept;

        /**
         * @brief Frame tick counter from the backend (incremented each paced DelayFrame; see WindowBackend::GetTicks).
         */
        virtual HONKORDGL_ND std::uint64_t GetTicks() const noexcept;

        /**
         * @brief Sets the backend frame tick counter (see GetTicks).
         */
        virtual void SetTicks(std::uint64_t ticks) noexcept;

        /**
         * @brief Clears this window's software framebuffer to RGBA and resizes it to current framebuffer size when needed.
         * @return false if allocation fails or framebuffer size is invalid.
         */
        virtual HONKORDGL_ND bool ClearFrame(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

        /**
         * @brief Presents the window's software framebuffer through the active backend adapter.
         * @return true when the current backend presented the frame.
         */
        virtual HONKORDGL_ND bool DisplayFrame() noexcept;

        /**
         * @brief Copies another window’s settings into this one; clears native handles in the copy to avoid duplicate ownership.
         */
        virtual Window& operator=(const Window& other);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        static void EnsureSoftwareBound(Impl& impl) noexcept;
    };
    //////////////////////////////////////////////////////

    /**
     * @brief Sets the last diagnostic string from internal window/graphics code (UTF-8).
     * Pass nullptr or "" to clear.
     */
    HONKORDGL_API void SetInternalApiError(const char * message) noexcept;

    /**
     * @brief Returns a pointer to a thread-local copy of the last diagnostic message.
     * Valid until the next SetInternalApiError, ClearInternalApiError, GetInternalApiError, or ErrorCallback on this thread.
     */
    HONKORDGL_API HONKORDGL_ND const char * GetInternalApiError() noexcept;

    /** @brief Clears the stored diagnostic message (same as SetInternalApiError(nullptr)). */
    HONKORDGL_API void ClearInternalApiError() noexcept;

    /**
     * @brief Copies the last diagnostic into `log` (at most `size - 1` characters, always null-terminated).
     * If `status` is non-null: 0 when no message, non-zero when a message is present.
     */
    HONKORDGL_API void ErrorCallback(int * status, char * log, unsigned int size) noexcept;

    /**
     * @brief Returns the process-wide default `Window` instance (lazy-initialized).
     *
     * Use it to drive `ApplicationContextSettings` and native window creation without managing your own `Window` object.
     */
    HONKORDGL_API HONKORDGL_ND Window& InitializeWindowSubsystem();

    /**
     * @brief Creates a `Window` that shares one display connection (`WindowBackend`) for multiple top-level surfaces.
     *
     * Call `Initialize` (or `OpenDisplay`) on `shared_backend` once, then `CreateWindow` on each `Window` in any order.
     * Each window keeps its own `ApplicationContextSettings` and `window_handle`; only one `Window` should own the backend.
     */
    HONKORDGL_API HONKORDGL_ND std::unique_ptr<Window> CreateWindowOnSharedBackend(
        WindowBackend& shared_backend,
        struct ApplicationContextSettings& settings);

} /* namespace Graphics */

} /* namespace HonkordGL */

#endif