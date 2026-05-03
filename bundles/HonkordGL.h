/**
 * Simplified **C** interface to HonkordGL (windowing, events, optional GPU + CPU surface).
 *
 * Compile the implementation as C++17:
 *   c++ -std=c++17 -c bundles/HonkordGL.cpp -I include -o bundles/HonkordGL.o
 * Link:
 *   cc ... bundles/HonkordGL.o -L path/to/lib -lHonkordGL ...
 */

#ifndef HONKORDGL_BUNDLE_H
#define HONKORDGL_BUNDLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct HonkordGL_App HonkordGL_App;

#ifdef __cplusplus
extern "C" {
#endif

/** Matches `HonkordGL::Graphics::WindowBackendKind` (underlying `unsigned char`). */
typedef enum HonkordGL_BackendKind {
    HONKORDGL_BACKEND_UNKNOWN = 0,
    HONKORDGL_BACKEND_X11,
    HONKORDGL_BACKEND_WAYLAND,
    HONKORDGL_BACKEND_COCOA,
    HONKORDGL_BACKEND_WIN32,
    HONKORDGL_BACKEND_ANDROID,
    HONKORDGL_BACKEND_DRM,
    HONKORDGL_BACKEND_NETBSD,
    HONKORDGL_BACKEND_FREEBSD,
    HONKORDGL_BACKEND_RASPBERRY_PI,
} HonkordGL_BackendKind;

/** Matches `HonkordGL::Events::EventType` order (starts at 0). */
typedef enum HonkordGL_EventType {
    HONKORDGL_EVENT_QUIT = 0,
    HONKORDGL_EVENT_RESIZE,
    HONKORDGL_EVENT_FOCUS,
    HONKORDGL_EVENT_MOUSE_ENTER,
    HONKORDGL_EVENT_MOUSE_LEAVE,
    HONKORDGL_EVENT_MOUSE_MOVE,
    HONKORDGL_EVENT_MOUSE_BUTTON_PRESS,
    HONKORDGL_EVENT_MOUSE_BUTTON_RELEASE,
    HONKORDGL_EVENT_KEY_PRESS,
    HONKORDGL_EVENT_KEY_RELEASE,
} HonkordGL_EventType;

typedef struct HonkordGL_Event {
    uint32_t type; /**< `HonkordGL_EventType` */
    uint32_t key; /**< Matches internal `KeyCode` ordinal; treat as opaque unless you mirror `Events.h`. */
    uint32_t mouse_button;
    int x;
    int y;
    int width;   /**< RESIZE: new drawable width */
    int height;  /**< RESIZE: new drawable height */
    int focused; /**< FOCUS: 1 = gained */
    uint32_t modifiers;
    uint32_t character; /**< UTF-32 when available */
} HonkordGL_Event;

/** @return New app object or NULL on allocation failure. */
HonkordGL_App *HonkordGL_App_Create(void);

/** Destroys GPU context (if any), window (if any), and closes the display subsystem if opened. */
void HonkordGL_App_Destroy(HonkordGL_App *app);

/**
 * Connect to a display / session (`WindowBackend::Initialize`).
 * @param display_or_null X11 display string, Wayland app id override, or NULL for defaults.
 * @return 1 on success, 0 on failure.
 */
int HonkordGL_InitSubsystem(HonkordGL_App *app, const char *display_or_null);

/** @return Current backend driver kind (meaningful after successful `HonkordGL_InitSubsystem`). */
HonkordGL_BackendKind HonkordGL_GetBackendKind(const HonkordGL_App *app);

/**
 * Opens a window (`client_rect`: x, y stored as first two ints; width = `w`, height = `z` in engine layout).
 * @return 1 on success, 0 on failure.
 */
int HonkordGL_Window_Create(HonkordGL_App *app, const char *title, int x, int y, int width, int height);

void HonkordGL_Window_Destroy(HonkordGL_App *app);

/**
 * Non-blocking event poll. Writes one event when returning 1.
 * @return 1 if an event was returned, 0 if the queue was empty.
 */
int HonkordGL_PollEvent(HonkordGL_App *app, HonkordGL_Event *out_event);

void HonkordGL_SetTargetFrameRate(HonkordGL_App *app, int fps);
void HonkordGL_FrameDelay(HonkordGL_App *app);

/** Effective framebuffer width (uses `frame_buffer_width` or falls back to client size). */
int HonkordGL_GetFramebufferWidth(const HonkordGL_App *app);
/** Effective framebuffer height. */
int HonkordGL_GetFramebufferHeight(const HonkordGL_App *app);

/** --- Optional GPU path (`GpuRenderer`) --- */

/**
 * Attaches a shader-capable OpenGL-family context (`CreateBestAvailableShaderBackend`).
 * @return 0 on success, non-zero HonkordGL renderer error code on failure.
 */
int HonkordGL_Gpu_CreateShaderBackend(HonkordGL_App *app);

void HonkordGL_Gpu_Destroy(HonkordGL_App *app);

void HonkordGL_Gpu_MakeCurrent(HonkordGL_App *app);
void HonkordGL_Gpu_SetDefaultViewport(HonkordGL_App *app);
void HonkordGL_Gpu_ClearColor(HonkordGL_App *app, float r, float g, float b, float a);
void HonkordGL_Gpu_SwapBuffers(HonkordGL_App *app);

/** Short label e.g. "OPENGL", "EGL", or empty if unavailable; valid after successful GPU create. */
const char *HonkordGL_Gpu_BackendLabel(const HonkordGL_App *app);

/** --- Optional CPU RGBA surface (`SoftwareRenderer` → `Present` without GPU attach) --- */

int HonkordGL_Software_EnsureSurface(HonkordGL_App *app);
uint8_t *HonkordGL_Software_Pixels(HonkordGL_App *app);
int HonkordGL_Software_Width(const HonkordGL_App *app);
int HonkordGL_Software_Height(const HonkordGL_App *app);
int HonkordGL_Software_StrideBytes(const HonkordGL_App *app);

void HonkordGL_Software_Clear(HonkordGL_App *app, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/** CPU present (`SoftwareRenderer::Present`). @return 1 on success, 0 on failure. */
int HonkordGL_Software_Present(HonkordGL_App *app);

/** Present raw RGBA bytes without using the internal `SoftwareRenderer` surface (same as `PresentSoftwareFramebuffer`). */
int HonkordGL_PresentSoftwareFramebuffer(
    HonkordGL_App *app,
    const uint8_t *rgba_pixels,
    int width,
    int height,
    int stride_bytes);

#ifdef __cplusplus
}
#endif

#endif /* HONKORDGL_BUNDLE_H */
