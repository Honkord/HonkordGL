/**
    * @author Honkord <https://github.com>

    HonkordGL
    Copyright (C) 2026 Honkord 
    (See LICENSE.md for what this file is licensed under)
*/

#ifndef __WindowApplication
#define __WindowApplication

#include <HonkordGL/Config.h>

typedef HONKORDGL_API struct HonkordGL_Renderer HonkordGL_Renderer; 
namespace HonkordGL 
{
namespace Graphics 
{
    //////////////////////////////////////////////////////
    typedef void * HonkordGL_GW_Handle;
    
    //////////////////////////////////////////////////////
    #ifdef HONKORDGL_REQUIRED_CXX20
    template <class T>
        concept EnableIntegral = std::is_integral_v<T>;

    template <EnableIntegral T>
        struct Rect {
            T x, y;
            T w, z;
        };
#else 
    template <class T>
        struct Rect {
            T x, y;
            T w, z;
        };
#endif
    typedef struct Rect<int> Recti;          typedef Recti GW_Rect; 
    typedef struct Rect<float> Rectf;
    typedef struct Rect<unsigned> Rectu;

    /** Selects native PollEvent path and OpenGL attach (X11/GLX vs Wayland/EGL vs Cocoa / Win32 / Android / DRM / NetBSD+X11 / FreeBSD+X11 / Raspberry Pi+X11). */
    enum class NativePlatform : int {
        Unknown = 0,
        X11 = 1,
        Wayland = 2,
        Cocoa = 3,
        Win32 = 4,
        Android = 5,
        DRM = 6,
        NetBSD = 7,
        FreeBSD = 8,
        RaspberryPi = 9,
    };

    /** X11-style clients: `device_context` = `Display *`, `window_handle` = XID (Linux X11, NetBSD, FreeBSD, Raspberry Pi driver). */
    inline bool NativePlatformUsesX11ClientHandles(NativePlatform p) noexcept
    {
        return p == NativePlatform::X11 || p == NativePlatform::NetBSD || p == NativePlatform::FreeBSD
            || p == NativePlatform::RaspberryPi;
    }

    //////////////////////////////////////////////////////
    HONKORDGL_API struct ApplicationContextSettings { /* Fill in the window fields and create your window. */
        /* Window Core */
        /** X11: Window; Wayland/Cocoa/Win32/Android: opaque backend state pointer. */
        HonkordGL_GW_Handle window_handle;                          /* Cross-platform window handling */
        /** UTF-8 title; set before WindowBackend::CreateWindow (not retained by the backend). */
        const char *        window_title{nullptr};
        /**
         * Optional UTF-8 filesystem path to a window / taskbar icon (not retained).
         * Win32: .ico via LoadImage. macOS: image file supported by NSImage (e.g. .icns, .png).
         * X11 / Wayland: ignored in this build (no embedded image decode).
         */
        const char *        window_icon_path{nullptr};
        /** @see NativePlatform — set by WindowBackend when the window is created. */
        int                 native_platform{0};                     /* static_cast<int>(NativePlatform) */
        /** Initial placement: x, y, width (w), height (z) — set before CreateWindow; updated when the window is created. */
        GW_Rect             client_rect;                            /* video mode area */
        int                 is_visible;                             /* TRUE = visible */
        int                 is_minimized;                           /* TRUE = minimized */
        int                 dpi_x;                                  /* Horizontal DPI */
        int                 dpi_y;                                  /* Vertical DPI */

        /* Rendering Surface */
        HonkordGL_GW_Handle device;                                 /* Primary native graphics object (opaque). */
        HonkordGL_GW_Handle device_context;                         /* Secondary handle (e.g. display / device context). */
        HonkordGL_GW_Handle surface;                                /* Drawable / swapchain object (opaque). */
        /** Native display connection when required by the active backend. Cleared by DetachRendererContext. */
        HonkordGL_GW_Handle egl_display{nullptr};
        /**
         * Implementation-private state for the active backend. Cleared by `DetachRendererContext`.
         */
        HonkordGL_GW_Handle renderer_private{nullptr};
        /** Set after successful attach: `static_cast<int>(Renderers::OPENGL)`, `EGL`, `VULKAN`, etc.; 0 when none. */
        int active_renderer{0};

        /* Pixel Format */
        int                 color_bits;                             /* e.g., 24 or 32 */
        int                 depth_bits;                             /* e.g., 24 */
        int                 stencil_bits;                           /* e.g., 8 */
        int                 msaa_samples;                           /* e.g., 1, 2, 4, 8 */
        int                 vsync_enabled;                          /* 1 = vsync on */ 

        /* Resize Handling */
        int                 frame_buffer_width;                     /* Render target width */
        int                 frame_buffer_height;                    /* Render target height */
        int                 needs_resize;                           /* TRUE = resize event occured */

        /* fullscreen/borderless */
        int                 is_fullscreen;                          /* Exclusive fullscreen */
        int                 borderless;                             /* Borderless fullscreen */
        HonkordGL_GW_Handle window_rect_backup;                     /* Restore position after fullscreen */

        /* Thread Affinity */
        unsigned long       creation_thread_ID;
        unsigned long       render_thread_ID;

        /* Callback */
        void (*ResizeCallback)(struct ApplicationContextSettings * ctx, int w, int h);
        void (*PresentCallback)(struct ApplicationContextSettings * ctx);
        void (*DestroyCallback)(struct ApplicationContextSettings * ctx);

        ApplicationContextSettings() noexcept = default;
        ApplicationContextSettings(const ApplicationContextSettings& other) noexcept = default;
        ~ApplicationContextSettings() = default;
        ApplicationContextSettings& operator=(const ApplicationContextSettings& other) noexcept = default;
    };

    /** Attach OpenGL (GLX / EGL / NSOpenGL / WGL) or future backends: HonkordGL/RendererContext.h */
    //////////////////////////////////////////////////////
    HONKORDGL_API enum class Renderers : unsigned int {
        NONE = 0,
        OPENGL,
        EGL, 
        METAL,
        WEBGPU, 
        WEBGL,
        VULKAN,
        /** Direct3D 11 (Windows): device + swap chain attach via `AttachRendererContext`. */
        DIRECT3D
    };

    HONKORDGL_API HONKORDGL_ND extern HonkordGL_Renderer * CreateRenderer(Renderers renderer);

} /* namespace Graphics */

} /* namespace HonkordGL */

#endif