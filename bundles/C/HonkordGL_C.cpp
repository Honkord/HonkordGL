#include "HonkordGL_C.h"

#include <HonkordGL/Core.h>
#include <HonkordGL/Audio.h>
#include <HonkordGL/GpuCapabilities.h>
#include <HonkordGL/GpuGlStateTracker.h>
#include <HonkordGL/GpuRenderGraph.h>
#include <HonkordGL/GpuRenderPassEncoder.h>
#include <HonkordGL/GpuRenderTarget.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/Joystick.h>

#include <cstring>
#include <memory>
#include <new>
#include <string>

struct HKGL_C_App {
    HonkordGL::Graphics::ApplicationContextSettings app;
    std::string title_storage;
};

struct HKGL_C_WindowBackend {
    HonkordGL::Graphics::WindowBackend backend;
};

struct HKGL_C_GpuRenderer {
    HonkordGL::Graphics::GpuRenderer renderer;
};

struct HKGL_C_GpuRenderTarget {
    std::unique_ptr<HonkordGL::Graphics::GpuRenderTarget> target;
};

struct HKGL_C_GpuRenderPassEncoder {
    HonkordGL::Graphics::GpuRenderPassEncoderGl encoder;
};

struct HKGL_C_GpuGlStateTracker {
    HonkordGL::Graphics::GpuGlStateTracker tracker;
};

struct HKGL_C_GpuRenderGraph {
    HonkordGL::Graphics::GpuRenderGraph graph;
};

struct HKGL_C_AudioBuffer {
    HonkordGL::Audio::AudioBuffer buffer;
};

struct HKGL_C_AudioStream {
    HonkordGL::Audio::AudioStream stream;
};

struct HKGL_C_AudioSource {
    HonkordGL::Audio::AudioSource source;
    explicit HKGL_C_AudioSource(HonkordGL::Audio::AudioMixer& mixer) noexcept
        : source(mixer) {}
};

struct HKGL_C_AudioTrackGroup {
    HonkordGL::Audio::AudioTrackGroup group;
    explicit HKGL_C_AudioTrackGroup(HonkordGL::Audio::AudioMixer& mixer) noexcept
        : group(mixer) {}
};

namespace {

HonkordGL::Audio::AudioFormat ToCppAudioFormat(const HKGL_C_AudioFormat& in) noexcept
{
    HonkordGL::Audio::AudioFormat out{};
    out.sampleRateHz = in.sample_rate_hz;
    out.channelCount = in.channel_count;
    out.sampleFormat = static_cast<HonkordGL::Audio::AudioSampleFormat>(in.sample_format);
    return out;
}

HKGL_C_AudioFormat ToCAudioFormat(const HonkordGL::Audio::AudioFormat& in) noexcept
{
    HKGL_C_AudioFormat out{};
    out.sample_rate_hz = in.sampleRateHz;
    out.channel_count = in.channelCount;
    out.sample_format = static_cast<unsigned int>(in.sampleFormat);
    return out;
}

HonkordGL::Joystick::JoystickId ToCppJoystickId(HKGL_C_JoystickId id) noexcept
{
    HonkordGL::Joystick::JoystickId out{};
    out.value = id.value;
    return out;
}

HKGL_C_JoystickId ToCJoystickId(HonkordGL::Joystick::JoystickId id) noexcept
{
    HKGL_C_JoystickId out{};
    out.value = id.value;
    return out;
}

HonkordGL::Joystick::JoystickDescriptor ToCppJoystickDescriptor(const HKGL_C_JoystickDescriptor& in) noexcept
{
    HonkordGL::Joystick::JoystickDescriptor out{};
    out.type = static_cast<HonkordGL::Joystick::JoystickType>(in.type);
    out.vendorId = in.vendor_id;
    out.productId = in.product_id;
    out.versionBcd = in.version_bcd;
    out.buttonCount = in.button_count;
    out.axisCount = in.axis_count;
    out.hatCount = in.hat_count;
    out.touchpadCount = in.touchpad_count;
    out.sensorCount = in.sensor_count;
    return out;
}

HKGL_C_JoystickDescriptor ToCJoystickDescriptor(const HonkordGL::Joystick::JoystickDescriptor& in) noexcept
{
    HKGL_C_JoystickDescriptor out{};
    out.type = static_cast<unsigned int>(in.type);
    out.vendor_id = in.vendorId;
    out.product_id = in.productId;
    out.version_bcd = in.versionBcd;
    out.button_count = in.buttonCount;
    out.axis_count = in.axisCount;
    out.hat_count = in.hatCount;
    out.touchpad_count = in.touchpadCount;
    out.sensor_count = in.sensorCount;
    return out;
}

HonkordGL::Joystick::JoystickMapping ToCppJoystickMapping(const HKGL_C_JoystickMapping& in) noexcept
{
    HonkordGL::Joystick::JoystickMapping out{};
    out.entry_count = in.entry_count;
    for (int i = 0; i < 64; ++i) {
        out.entries[i].logical = static_cast<HonkordGL::Joystick::JoystickLogicalInput>(in.entries[i].logical);
        out.entries[i].source_kind = static_cast<HonkordGL::Joystick::JoystickMappingInputKind>(in.entries[i].source_kind);
        out.entries[i].source_index = in.entries[i].source_index;
        out.entries[i].scale = in.entries[i].scale;
        out.entries[i].deadzone = in.entries[i].deadzone;
    }
    return out;
}

HKGL_C_JoystickMapping ToCJoystickMapping(const HonkordGL::Joystick::JoystickMapping& in) noexcept
{
    HKGL_C_JoystickMapping out{};
    out.entry_count = in.entry_count;
    for (int i = 0; i < 64; ++i) {
        out.entries[i].logical = static_cast<unsigned int>(in.entries[i].logical);
        out.entries[i].source_kind = static_cast<unsigned int>(in.entries[i].source_kind);
        out.entries[i].source_index = in.entries[i].source_index;
        out.entries[i].scale = in.entries[i].scale;
        out.entries[i].deadzone = in.entries[i].deadzone;
    }
    return out;
}

HKGL_C_GpuLimits ToCGpuLimits(const HonkordGL::Graphics::GpuLimits& in) noexcept
{
    HKGL_C_GpuLimits out{};
    out.max_texture_2d_size = in.max_texture_2d_size;
    out.max_cube_map_texture_size = in.max_cube_map_texture_size;
    out.max_renderbuffer_size = in.max_renderbuffer_size;
    out.max_viewport_width = in.max_viewport_width;
    out.max_viewport_height = in.max_viewport_height;
    out.max_vertex_attribs = in.max_vertex_attribs;
    out.max_vertex_uniform_vectors = in.max_vertex_uniform_vectors;
    out.max_fragment_uniform_vectors = in.max_fragment_uniform_vectors;
    out.max_varying_vectors = in.max_varying_vectors;
    out.uniform_buffer_offset_alignment = in.uniform_buffer_offset_alignment;
    out.max_framebuffer_color_attachments = in.max_framebuffer_color_attachments;
    out.max_msaa_samples = in.max_msaa_samples;
    out.gpu_timestamp_ns_per_tick = in.gpu_timestamp_ns_per_tick;
    return out;
}

HonkordGL::Graphics::GpuRenderTargetCreateInfo ToCppRenderTargetInfo(const HKGL_C_GpuRenderTargetCreateInfo& in) noexcept
{
    HonkordGL::Graphics::GpuRenderTargetCreateInfo out{};
    out.width = in.width;
    out.height = in.height;
    out.color_count = in.color_count;
    for (int i = 0; i < 8; ++i) {
        out.color_formats[i] = static_cast<HonkordGL::Graphics::GpuColorFormat>(in.color_formats[i]);
    }
    out.depth_stencil = static_cast<HonkordGL::Graphics::GpuDepthStencilFormat>(in.depth_stencil);
    return out;
}

HonkordGL::Graphics::GpuRenderPassBeginInfo ToCppPassBegin(const HKGL_C_GpuRenderPassBeginInfo& in) noexcept
{
    HonkordGL::Graphics::GpuRenderPassBeginInfo out{};
    out.clear_mask = in.clear_mask;
    for (int i = 0; i < 8; ++i) {
        out.color_clears[i].r = in.color_clears[i].r;
        out.color_clears[i].g = in.color_clears[i].g;
        out.color_clears[i].b = in.color_clears[i].b;
        out.color_clears[i].a = in.color_clears[i].a;
    }
    out.depth_clear = in.depth_clear;
    out.stencil_clear = in.stencil_clear;
    out.enable_depth_test = in.enable_depth_test;
    out.depth_write = in.depth_write;
    out.depth_func = static_cast<HonkordGL::Graphics::GpuDepthFunc>(in.depth_func);
    return out;
}

HKGL_C_GpuRenderGraphPass ToCRenderGraphPass(const HonkordGL::Graphics::GpuRenderGraphPass& in) noexcept
{
    HKGL_C_GpuRenderGraphPass out{};
    out.target = nullptr;
    out.surface_width = in.surface_width;
    out.surface_height = in.surface_height;
    out.begin.clear_mask = in.begin.clear_mask;
    for (int i = 0; i < 8; ++i) {
        out.begin.color_clears[i].r = in.begin.color_clears[i].r;
        out.begin.color_clears[i].g = in.begin.color_clears[i].g;
        out.begin.color_clears[i].b = in.begin.color_clears[i].b;
        out.begin.color_clears[i].a = in.begin.color_clears[i].a;
    }
    out.begin.depth_clear = in.begin.depth_clear;
    out.begin.stencil_clear = in.begin.stencil_clear;
    out.begin.enable_depth_test = in.begin.enable_depth_test;
    out.begin.depth_write = in.begin.depth_write;
    out.begin.depth_func = static_cast<unsigned int>(in.begin.depth_func);
    out.debug_name = in.debug_name;
    return out;
}

HKGL_C_AudioDeviceChangeCallback g_audio_device_change_cb = nullptr;
void* g_audio_device_change_user = nullptr;

void AudioDeviceChangeThunk(HonkordGL::Audio::AudioDeviceChangeEvent event, char const * deviceId, void * user) noexcept
{
    (void)user;
    if (g_audio_device_change_cb) {
        g_audio_device_change_cb(static_cast<unsigned int>(event), deviceId, g_audio_device_change_user);
    }
}

HKGL_C_JoystickHotplugCallback g_joystick_hotplug_cb = nullptr;
void* g_joystick_hotplug_user = nullptr;

void JoystickHotplugThunk(HonkordGL::Joystick::JoystickHotplugEvent event, HonkordGL::Joystick::JoystickId id, int index_hint, void * user) noexcept
{
    (void)user;
    if (g_joystick_hotplug_cb) {
        g_joystick_hotplug_cb(static_cast<unsigned int>(event), ToCJoystickId(id), index_hint, g_joystick_hotplug_user);
    }
}

} // namespace

extern "C" {

HKGL_C_App * hkgl_c_app_create(void)
{
    auto * out = new (std::nothrow) HKGL_C_App();
    if (!out) {
        return nullptr;
    }
    out->app.client_rect.x = 100;
    out->app.client_rect.y = 100;
    out->app.client_rect.w = 1280;
    out->app.client_rect.z = 720;
    out->app.is_visible = 1;
    out->title_storage = "HonkordGL C";
    out->app.window_title = out->title_storage.c_str();
    return out;
}

void hkgl_c_app_destroy(HKGL_C_App * app)
{
    delete app;
}

void hkgl_c_app_set_title(HKGL_C_App * app, const char * title_utf8)
{
    if (!app) {
        return;
    }
    app->title_storage = title_utf8 ? title_utf8 : "";
    app->app.window_title = app->title_storage.c_str();
}

void hkgl_c_app_set_client_rect(HKGL_C_App * app, int x, int y, int width, int height)
{
    if (!app) {
        return;
    }
    app->app.client_rect.x = x;
    app->app.client_rect.y = y;
    app->app.client_rect.w = width;
    app->app.client_rect.z = height;
}

void hkgl_c_app_set_visible(HKGL_C_App * app, int visible)
{
    if (!app) {
        return;
    }
    app->app.is_visible = visible ? 1 : 0;
}

HKGL_C_WindowBackend * hkgl_c_backend_create(void)
{
    return new (std::nothrow) HKGL_C_WindowBackend();
}

void hkgl_c_backend_destroy(HKGL_C_WindowBackend * backend)
{
    delete backend;
}

bool hkgl_c_backend_initialize(HKGL_C_WindowBackend * backend, const char * display_or_app_name)
{
    if (!backend) {
        return false;
    }
    return backend->backend.Initialize(display_or_app_name);
}

void hkgl_c_backend_shutdown(HKGL_C_WindowBackend * backend)
{
    if (!backend) {
        return;
    }
    backend->backend.Shutdown();
}

bool hkgl_c_backend_create_window(HKGL_C_WindowBackend * backend, HKGL_C_App * app)
{
    if (!backend || !app) {
        return false;
    }
    return backend->backend.CreateWindow(app->app);
}

void hkgl_c_backend_destroy_window(HKGL_C_WindowBackend * backend, HKGL_C_App * app)
{
    if (!backend || !app) {
        return;
    }
    backend->backend.DestroyWindow(app->app);
}

bool hkgl_c_backend_process_events(HKGL_C_WindowBackend * backend, HKGL_C_App * app)
{
    if (!backend || !app) {
        return false;
    }
    return backend->backend.ProcessEvents(app->app);
}

int hkgl_c_poll_event(HKGL_C_App * app, HKGL_C_Event * out_event)
{
    if (!app || !out_event) {
        return 0;
    }
    HonkordGL::Events::Event ev{};
    if (!HonkordGL::Events::PollEvent(app->app, ev)) {
        return 0;
    }
    out_event->type = static_cast<unsigned int>(ev.type);
    out_event->key = static_cast<unsigned int>(ev.key);
    out_event->mouse_button = ev.mouse_button;
    out_event->x = ev.x;
    out_event->y = ev.y;
    out_event->width = ev.width;
    out_event->height = ev.height;
    out_event->focused = ev.focused;
    out_event->modifiers = ev.modifiers;
    out_event->character = ev.character;
    return 1;
}

int hkgl_c_backend_kind(HKGL_C_WindowBackend * backend)
{
    if (!backend) {
        return 0;
    }
    return static_cast<int>(backend->backend.GetKind());
}

int hkgl_c_video_driver_count(void)
{
    return HonkordGL::Graphics::GetVideoDriverCount();
}

int hkgl_c_build_platform(void)
{
    switch (HonkordGL::CurrentBuildPlatform()) {
        case HonkordGL::BuildPlatform::Windows:
            return HKGL_C_PLATFORM_WINDOWS;
        case HonkordGL::BuildPlatform::Apple:
            return HKGL_C_PLATFORM_APPLE;
        case HonkordGL::BuildPlatform::LinuxDesktop:
            return HKGL_C_PLATFORM_LINUX_DESKTOP;
        case HonkordGL::BuildPlatform::LinuxAndroid:
            return HKGL_C_PLATFORM_LINUX_ANDROID;
        case HonkordGL::BuildPlatform::LinuxRaspberryPi:
            return HKGL_C_PLATFORM_LINUX_RASPBERRY_PI;
        case HonkordGL::BuildPlatform::NetBSD:
            return HKGL_C_PLATFORM_NETBSD;
        case HonkordGL::BuildPlatform::FreeBSD:
            return HKGL_C_PLATFORM_FREEBSD;
        case HonkordGL::BuildPlatform::Emscripten:
            return HKGL_C_PLATFORM_EMSCRIPTEN;
        default:
            return HKGL_C_PLATFORM_UNKNOWN;
    }
}

HKGL_C_GpuRenderer * hkgl_c_renderer_create(void)
{
    return new (std::nothrow) HKGL_C_GpuRenderer();
}

void hkgl_c_renderer_destroy(HKGL_C_GpuRenderer * renderer)
{
    delete renderer;
}

void hkgl_c_renderer_bind(HKGL_C_GpuRenderer * renderer, HKGL_C_App * app)
{
    if (!renderer || !app) {
        return;
    }
    renderer->renderer.Bind(app->app);
}

void hkgl_c_renderer_set_mode(HKGL_C_GpuRenderer * renderer, int mode)
{
    if (!renderer) {
        return;
    }
    auto target_mode = mode == HKGL_C_RENDERER_MODE_MINIMAL
        ? HonkordGL::Graphics::GpuRendererMode::Minimal
        : HonkordGL::Graphics::GpuRendererMode::Assisted;
    renderer->renderer.SetMode(target_mode);
}

int hkgl_c_renderer_create_best_available(HKGL_C_GpuRenderer * renderer)
{
    if (!renderer) {
        return static_cast<int>(HonkordGL::Graphics::RendererContextResult::INVALID_ARGUMENT);
    }
    return renderer->renderer.CreateBestAvailable();
}

int hkgl_c_renderer_create_vulkan(HKGL_C_GpuRenderer * renderer)
{
    if (!renderer) {
        return static_cast<int>(HonkordGL::Graphics::RendererContextResult::INVALID_ARGUMENT);
    }
    return renderer->renderer.CreateVulkan();
}

int hkgl_c_renderer_make_current(HKGL_C_GpuRenderer * renderer)
{
    if (!renderer) {
        return static_cast<int>(HonkordGL::Graphics::RendererContextResult::INVALID_ARGUMENT);
    }
    return renderer->renderer.MakeCurrent();
}

void hkgl_c_renderer_clear_color(HKGL_C_GpuRenderer * renderer, float r, float g, float b, float a)
{
    if (!renderer) {
        return;
    }
    renderer->renderer.ClearColorBuffer(r, g, b, a);
}

void hkgl_c_renderer_swap_buffers(HKGL_C_GpuRenderer * renderer)
{
    if (!renderer) {
        return;
    }
    renderer->renderer.SwapBuffers();
}

void hkgl_c_renderer_detach_context(HKGL_C_GpuRenderer * renderer)
{
    if (!renderer) {
        return;
    }
    renderer->renderer.Destroy();
}

int hkgl_c_renderer_query_hardware_limits(HKGL_C_GpuRenderer * renderer, HKGL_C_GpuLimits * out_limits)
{
    if (!renderer || !out_limits) {
        return -1;
    }
    HonkordGL::Graphics::GpuLimits limits{};
    int r = renderer->renderer.QueryHardwareLimits(&limits);
    *out_limits = ToCGpuLimits(limits);
    return r;
}

int hkgl_c_renderer_try_enable_optional_feature(HKGL_C_GpuRenderer * renderer, unsigned int feature)
{
    if (!renderer) {
        return -1;
    }
    return renderer->renderer.TryEnableOptionalFeature(static_cast<HonkordGL::Graphics::GpuOptionalFeature>(feature));
}

int hkgl_c_renderer_is_optional_feature_enabled(HKGL_C_GpuRenderer * renderer, unsigned int feature)
{
    if (!renderer) {
        return 0;
    }
    return renderer->renderer.IsOptionalFeatureEnabled(static_cast<HonkordGL::Graphics::GpuOptionalFeature>(feature)) ? 1 : 0;
}

void hkgl_c_renderer_set_default_viewport(HKGL_C_GpuRenderer * renderer)
{
    if (renderer) {
        renderer->renderer.SetDefaultViewport();
    }
}

void hkgl_c_renderer_set_viewport(HKGL_C_GpuRenderer * renderer, int x, int y, int width, int height)
{
    if (renderer) {
        renderer->renderer.SetViewport(x, y, width, height);
    }
}

int hkgl_c_renderer_compile_shader_program(HKGL_C_GpuRenderer * renderer, const char * vertex_source, const char * fragment_source,
    unsigned int * out_program, char * info_log, size_t info_log_bytes)
{
    if (!renderer || !out_program) {
        return -1;
    }
    HonkordGL::Graphics::GpuObjectName program{};
    int r = renderer->renderer.CompileShaderProgram(vertex_source, fragment_source, &program, info_log, info_log_bytes);
    *out_program = program;
    return r;
}

void hkgl_c_renderer_delete_shader_program(HKGL_C_GpuRenderer * renderer, unsigned int program)
{
    if (renderer) {
        renderer->renderer.DeleteShaderProgram(program);
    }
}

int hkgl_c_renderer_create_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int stage, unsigned int * out_shader)
{
    if (!renderer || !out_shader) {
        return -1;
    }
    HonkordGL::Graphics::GpuObjectName shader{};
    int r = renderer->renderer.CreateShaderObject(static_cast<HonkordGL::Graphics::GpuShaderPipelineStage>(stage), &shader);
    *out_shader = shader;
    return r;
}

int hkgl_c_renderer_set_shader_source(HKGL_C_GpuRenderer * renderer, unsigned int shader, const char * source)
{
    return renderer ? renderer->renderer.SetShaderSource(shader, source) : -1;
}

int hkgl_c_renderer_compile_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int shader, char * info_log, size_t info_log_bytes)
{
    return renderer ? renderer->renderer.CompileShaderObject(shader, info_log, info_log_bytes) : -1;
}

int hkgl_c_renderer_create_program_object(HKGL_C_GpuRenderer * renderer, unsigned int * out_program)
{
    if (!renderer || !out_program) {
        return -1;
    }
    HonkordGL::Graphics::GpuObjectName program{};
    int r = renderer->renderer.CreateProgramObject(&program);
    *out_program = program;
    return r;
}

void hkgl_c_renderer_attach_shader(HKGL_C_GpuRenderer * renderer, unsigned int program, unsigned int shader)
{
    if (renderer) {
        renderer->renderer.AttachShader(program, shader);
    }
}

int hkgl_c_renderer_bind_attrib_location(HKGL_C_GpuRenderer * renderer, unsigned int program, unsigned int location, const char * name)
{
    return renderer ? renderer->renderer.BindAttribLocation(program, location, name) : -1;
}

int hkgl_c_renderer_link_shader_objects(HKGL_C_GpuRenderer * renderer, unsigned int program, char * info_log, size_t info_log_bytes)
{
    return renderer ? renderer->renderer.LinkShaderObjects(program, info_log, info_log_bytes) : -1;
}

void hkgl_c_renderer_delete_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int shader)
{
    if (renderer) {
        renderer->renderer.DeleteShaderObject(shader);
    }
}

int hkgl_c_renderer_create_buffer(HKGL_C_GpuRenderer * renderer, unsigned int * out_buffer)
{
    if (!renderer || !out_buffer) {
        return -1;
    }
    HonkordGL::Graphics::GpuObjectName b{};
    int r = renderer->renderer.CreateBuffer(&b);
    *out_buffer = b;
    return r;
}

void hkgl_c_renderer_destroy_buffer(HKGL_C_GpuRenderer * renderer, unsigned int buffer)
{
    if (renderer) {
        renderer->renderer.DestroyBuffer(buffer);
    }
}

int hkgl_c_renderer_bind_buffer(HKGL_C_GpuRenderer * renderer, unsigned int target, unsigned int buffer)
{
    return renderer ? renderer->renderer.BindBuffer(static_cast<HonkordGL::Graphics::GpuBufferTarget>(target), buffer) : -1;
}

int hkgl_c_renderer_upload_buffer_data(HKGL_C_GpuRenderer * renderer, unsigned int target, const void * data, size_t byte_size, unsigned int usage)
{
    return renderer
        ? renderer->renderer.UploadBufferData(
              static_cast<HonkordGL::Graphics::GpuBufferTarget>(target), data, byte_size, static_cast<HonkordGL::Graphics::GpuBufferUsage>(usage))
        : -1;
}

int hkgl_c_renderer_create_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int * out_vao)
{
    if (!renderer || !out_vao) {
        return -1;
    }
    HonkordGL::Graphics::GpuObjectName vao{};
    int r = renderer->renderer.CreateVertexArray(&vao);
    *out_vao = vao;
    return r;
}

void hkgl_c_renderer_destroy_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int vao)
{
    if (renderer) {
        renderer->renderer.DestroyVertexArray(vao);
    }
}

int hkgl_c_renderer_bind_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int vao)
{
    return renderer ? renderer->renderer.BindVertexArray(vao) : -1;
}

int hkgl_c_renderer_enable_vertex_attribute(HKGL_C_GpuRenderer * renderer, unsigned int location)
{
    return renderer ? renderer->renderer.EnableVertexAttribute(location) : -1;
}

int hkgl_c_renderer_set_vertex_attribute_float(
    HKGL_C_GpuRenderer * renderer, unsigned int location, int components, int stride_bytes, size_t offset_bytes, int normalized)
{
    return renderer ? renderer->renderer.SetVertexAttributeFloat(location, components, stride_bytes, offset_bytes, normalized != 0) : -1;
}

void hkgl_c_renderer_set_depth_test_enabled(HKGL_C_GpuRenderer * renderer, int enabled)
{
    if (renderer) {
        renderer->renderer.SetDepthTestEnabled(enabled != 0);
    }
}

int hkgl_c_renderer_draw_arrays(HKGL_C_GpuRenderer * renderer, unsigned int primitive, int first_vertex, int vertex_count)
{
    return renderer ? renderer->renderer.DrawArrays(static_cast<HonkordGL::Graphics::GpuPrimitive>(primitive), first_vertex, vertex_count) : -1;
}

int hkgl_c_renderer_draw_indexed(
    HKGL_C_GpuRenderer * renderer, unsigned int primitive, int index_count, unsigned int index_type, size_t index_offset_bytes)
{
    return renderer
        ? renderer->renderer.DrawIndexed(static_cast<HonkordGL::Graphics::GpuPrimitive>(primitive), index_count,
              static_cast<HonkordGL::Graphics::GpuIndexType>(index_type), index_offset_bytes)
        : -1;
}

HKGL_C_GpuRenderTarget * hkgl_c_gpu_render_target_create(HKGL_C_GpuRenderer * renderer, const HKGL_C_GpuRenderTargetCreateInfo * info)
{
    if (!renderer || !info) {
        return nullptr;
    }
    auto * out = new (std::nothrow) HKGL_C_GpuRenderTarget();
    if (!out) {
        return nullptr;
    }
    std::unique_ptr<HonkordGL::Graphics::GpuRenderTarget> created;
    int r = HonkordGL::Graphics::GpuRenderTarget::Create(renderer->renderer, ToCppRenderTargetInfo(*info), &created);
    if (r != 0 || !created) {
        delete out;
        return nullptr;
    }
    out->target = std::move(created);
    return out;
}

void hkgl_c_gpu_render_target_destroy(HKGL_C_GpuRenderTarget * target, HKGL_C_GpuRenderer * renderer)
{
    if (!target) {
        return;
    }
    if (renderer && target->target) {
        target->target->Destroy(renderer->renderer);
    }
    delete target;
}

int hkgl_c_gpu_render_target_resize(HKGL_C_GpuRenderTarget * target, HKGL_C_GpuRenderer * renderer, int width, int height)
{
    if (!target || !renderer || !target->target) {
        return -1;
    }
    return target->target->Resize(renderer->renderer, width, height);
}

int hkgl_c_gpu_render_target_width(HKGL_C_GpuRenderTarget * target) { return (target && target->target) ? target->target->Width() : 0; }
int hkgl_c_gpu_render_target_height(HKGL_C_GpuRenderTarget * target) { return (target && target->target) ? target->target->Height() : 0; }
int hkgl_c_gpu_render_target_color_count(HKGL_C_GpuRenderTarget * target) { return (target && target->target) ? target->target->ColorCount() : 0; }
unsigned int hkgl_c_gpu_render_target_color_texture(HKGL_C_GpuRenderTarget * target, int index)
{
    return (target && target->target) ? target->target->ColorTexture(index) : 0;
}
unsigned int hkgl_c_gpu_render_target_depth_stencil_texture(HKGL_C_GpuRenderTarget * target)
{
    return (target && target->target) ? target->target->DepthStencilTexture() : 0;
}
unsigned int hkgl_c_gpu_render_target_fbo(HKGL_C_GpuRenderTarget * target) { return (target && target->target) ? target->target->FramebufferObject() : 0; }

HKGL_C_GpuRenderPassEncoder * hkgl_c_gpu_render_pass_begin(
    HKGL_C_GpuRenderer * renderer, HKGL_C_GpuRenderTarget * target, const HKGL_C_GpuRenderPassBeginInfo * info)
{
    if (!renderer || !target || !target->target || !info) {
        return nullptr;
    }
    auto * out = new (std::nothrow) HKGL_C_GpuRenderPassEncoder();
    if (!out) {
        return nullptr;
    }
    out->encoder = HonkordGL::Graphics::GpuRenderPassEncoderGl(renderer->renderer, *target->target, ToCppPassBegin(*info));
    return out;
}

void hkgl_c_gpu_render_pass_end(HKGL_C_GpuRenderPassEncoder * encoder)
{
    if (encoder) {
        encoder->encoder.End();
        delete encoder;
    }
}

void hkgl_c_gpu_render_pass_set_scissor(HKGL_C_GpuRenderPassEncoder * encoder, int x, int y, int width, int height)
{
    if (encoder) {
        encoder->encoder.SetScissor(x, y, width, height);
    }
}

int hkgl_c_gpu_render_pass_last_result(HKGL_C_GpuRenderPassEncoder * encoder)
{
    return encoder ? encoder->encoder.LastResult() : -1;
}

HKGL_C_GpuGlStateTracker * hkgl_c_gpu_state_tracker_create(void)
{
    return new (std::nothrow) HKGL_C_GpuGlStateTracker();
}

void hkgl_c_gpu_state_tracker_destroy(HKGL_C_GpuGlStateTracker * tracker) { delete tracker; }
void hkgl_c_gpu_state_tracker_invalidate(HKGL_C_GpuGlStateTracker * tracker) { if (tracker) tracker->tracker.Invalidate(); }
void hkgl_c_gpu_state_tracker_bind_framebuffer(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, unsigned int fbo)
{
    if (tracker && renderer) tracker->tracker.BindFramebuffer(renderer->renderer, fbo);
}
void hkgl_c_gpu_state_tracker_set_draw_buffer_count(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, int color_attachment_count)
{
    if (tracker && renderer) tracker->tracker.SetDrawBufferCount(renderer->renderer, color_attachment_count);
}
void hkgl_c_gpu_state_tracker_set_viewport(HKGL_C_GpuGlStateTracker * tracker, int x, int y, int width, int height)
{
    if (tracker) tracker->tracker.SetViewport(x, y, width, height);
}
void hkgl_c_gpu_state_tracker_set_depth_state(
    HKGL_C_GpuGlStateTracker * tracker, int test_enabled, int depth_write, unsigned int depth_func)
{
    if (tracker) tracker->tracker.SetDepthState(test_enabled != 0, depth_write != 0, static_cast<HonkordGL::Graphics::GpuDepthFunc>(depth_func));
}
void hkgl_c_gpu_state_tracker_set_scissor_enabled(
    HKGL_C_GpuGlStateTracker * tracker, int enabled, int x, int y, int width, int height)
{
    if (tracker) tracker->tracker.SetScissorEnabled(enabled != 0, x, y, width, height);
}
void hkgl_c_gpu_state_tracker_set_color_mask(HKGL_C_GpuGlStateTracker * tracker, int r, int g, int b, int a)
{
    if (tracker) tracker->tracker.SetColorMask(r != 0, g != 0, b != 0, a != 0);
}
int hkgl_c_gpu_state_tracker_begin_pass(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, HKGL_C_GpuRenderTarget * target,
    const HKGL_C_GpuRenderPassBeginInfo * info, int surface_width, int surface_height)
{
    if (!tracker || !renderer || !info) return -1;
    return tracker->tracker.BeginPass(
        renderer->renderer, (target && target->target) ? target->target.get() : nullptr, ToCppPassBegin(*info), surface_width, surface_height);
}
unsigned int hkgl_c_gpu_state_tracker_cached_fbo(HKGL_C_GpuGlStateTracker * tracker) { return tracker ? tracker->tracker.CachedFramebuffer() : 0; }

HKGL_C_GpuRenderGraph * hkgl_c_gpu_render_graph_create(void) { return new (std::nothrow) HKGL_C_GpuRenderGraph(); }
void hkgl_c_gpu_render_graph_destroy(HKGL_C_GpuRenderGraph * graph) { delete graph; }
void hkgl_c_gpu_render_graph_clear(HKGL_C_GpuRenderGraph * graph) { if (graph) graph->graph.Clear(); }
void hkgl_c_gpu_render_graph_reserve(HKGL_C_GpuRenderGraph * graph, size_t pass_capacity) { if (graph) graph->graph.Reserve(pass_capacity); }
void hkgl_c_gpu_render_graph_add_pass(HKGL_C_GpuRenderGraph * graph, const HKGL_C_GpuRenderGraphPass * pass)
{
    if (!graph || !pass) return;
    HonkordGL::Graphics::GpuRenderGraphPass p{};
    p.target = (pass->target && pass->target->target) ? pass->target->target.get() : nullptr;
    p.surface_width = pass->surface_width;
    p.surface_height = pass->surface_height;
    p.begin = ToCppPassBegin(pass->begin);
    p.debug_name = pass->debug_name;
    graph->graph.AddPass(p);
}
void hkgl_c_gpu_render_graph_add_dependency(HKGL_C_GpuRenderGraph * graph, int prerequisite_pass_index, int dependent_pass_index)
{
    if (graph) graph->graph.AddDependency(prerequisite_pass_index, dependent_pass_index);
}
int hkgl_c_gpu_render_graph_compile(HKGL_C_GpuRenderGraph * graph) { return graph ? graph->graph.Compile() : -1; }

namespace {
struct GraphExecBridge {
    void (*cb)(void*, int, HKGL_C_GpuRenderGraphPass const *);
    void* user;
};
void GraphExecThunk(void * user_data, int pass_index, HonkordGL::Graphics::GpuRenderGraphPass const & pass)
{
    auto * bridge = reinterpret_cast<GraphExecBridge *>(user_data);
    if (!bridge || !bridge->cb) return;
    HKGL_C_GpuRenderGraphPass cpass = ToCRenderGraphPass(pass);
    bridge->cb(bridge->user, pass_index, &cpass);
}
}

int hkgl_c_gpu_render_graph_execute(HKGL_C_GpuRenderGraph * graph, HKGL_C_GpuRenderer * renderer, HKGL_C_GpuGlStateTracker * tracker,
    void (*per_pass)(void * user_data, int pass_index, const HKGL_C_GpuRenderGraphPass * pass), void * user_data)
{
    if (!graph || !renderer || !tracker) return -1;
    GraphExecBridge bridge{per_pass, user_data};
    return graph->graph.Execute(renderer->renderer, tracker->tracker, per_pass ? &GraphExecThunk : nullptr, per_pass ? &bridge : nullptr);
}

int hkgl_c_gpu_query_limits(HKGL_C_App * app, HKGL_C_GpuLimits * out_limits)
{
    if (!app || !out_limits) return -1;
    HonkordGL::Graphics::GpuLimits limits{};
    int r = HonkordGL::Graphics::QueryGpuLimits(&app->app, &limits);
    *out_limits = ToCGpuLimits(limits);
    return r;
}
int hkgl_c_gpu_try_enable_optional_feature(HKGL_C_App * app, unsigned int feature)
{
    return app ? HonkordGL::Graphics::TryEnableGpuOptionalFeature(&app->app, static_cast<HonkordGL::Graphics::GpuOptionalFeature>(feature)) : -1;
}
int hkgl_c_gpu_optional_feature_is_enabled(HKGL_C_App * app, unsigned int feature)
{
    return app && HonkordGL::Graphics::GpuOptionalFeatureIsEnabled(&app->app, static_cast<HonkordGL::Graphics::GpuOptionalFeature>(feature)) ? 1 : 0;
}
int hkgl_c_gpu_shader_load_compiler_procs(void) { return HonkordGL::Graphics::GpuShaderLoadCompilerProcs(); }
int hkgl_c_gpu_shader_compile_program(HKGL_C_App * app, const char * vertex_source, const char * fragment_source,
    unsigned int * out_program, char * info_log, size_t info_log_cap, const int * attrib_locations, const char * const * attrib_names,
    int attrib_count)
{
    return HonkordGL::Graphics::GpuShaderCompileProgram(
        app ? &app->app : nullptr, vertex_source, fragment_source, out_program, info_log, info_log_cap, attrib_locations, attrib_names, attrib_count);
}
void hkgl_c_gpu_shader_delete_program(unsigned int program) { HonkordGL::Graphics::GpuShaderDeleteProgram(program); }
int hkgl_c_gpu_shader_create_object(HKGL_C_App * app, unsigned int stage, unsigned int * out_shader)
{
    return HonkordGL::Graphics::GpuShaderCreateObject(
        app ? &app->app : nullptr, static_cast<HonkordGL::Graphics::GpuShaderPipelineStage>(stage), out_shader);
}
int hkgl_c_gpu_shader_set_source(HKGL_C_App * app, unsigned int shader, const char * source)
{
    return HonkordGL::Graphics::GpuShaderSetShaderSource(app ? &app->app : nullptr, shader, source);
}
int hkgl_c_gpu_shader_compile_object(HKGL_C_App * app, unsigned int shader, char * info_log, size_t info_log_cap)
{
    return HonkordGL::Graphics::GpuShaderCompileShaderObject(app ? &app->app : nullptr, shader, info_log, info_log_cap);
}
int hkgl_c_gpu_shader_create_program_object(HKGL_C_App * app, unsigned int * out_program)
{
    return HonkordGL::Graphics::GpuShaderCreateProgramObject(app ? &app->app : nullptr, out_program);
}
void hkgl_c_gpu_shader_attach(HKGL_C_App * app, unsigned int program, unsigned int shader)
{
    HonkordGL::Graphics::GpuShaderAttachShader(app ? &app->app : nullptr, program, shader);
}
int hkgl_c_gpu_shader_bind_attrib(HKGL_C_App * app, unsigned int program, unsigned int location, const char * name)
{
    return HonkordGL::Graphics::GpuShaderBindAttribLocation(app ? &app->app : nullptr, program, location, name);
}
int hkgl_c_gpu_shader_link_program(HKGL_C_App * app, unsigned int program, char * info_log, size_t info_log_cap)
{
    return HonkordGL::Graphics::GpuShaderLinkProgramObject(app ? &app->app : nullptr, program, info_log, info_log_cap);
}
void hkgl_c_gpu_shader_delete_object(unsigned int shader) { HonkordGL::Graphics::GpuShaderDeleteShaderObject(shader); }

int hkgl_c_audio_initialize(unsigned int flags)
{
    return HonkordGL::Audio::InitializeAudioSubsystem(flags);
}

void hkgl_c_audio_shutdown(void)
{
    HonkordGL::Audio::ShutdownAudioSubsystem();
}

int hkgl_c_audio_decoder_count(void)
{
    return HonkordGL::Audio::GetAudioDecoderCount();
}

int hkgl_c_audio_decoder_name(int index, char * buf, size_t buf_bytes)
{
    return HonkordGL::Audio::GetAudioDecoderName(index, buf, buf_bytes);
}

unsigned int hkgl_c_audio_backend_kind(void)
{
    return static_cast<unsigned int>(HonkordGL::Audio::GetAudioBackendKind());
}

HKGL_C_AudioCapabilities hkgl_c_audio_capabilities(void)
{
    HKGL_C_AudioCapabilities out{};
    auto caps = HonkordGL::Audio::GetAudioCapabilities();
    out.backend = static_cast<unsigned int>(caps.backend);
    out.supports_streaming = caps.supportsStreaming ? 1 : 0;
    out.supports_device_change_events = caps.supportsDeviceChangeEvents ? 1 : 0;
    out.supports_mix_pipeline_callback = caps.supportsMixPipelineCallback ? 1 : 0;
    out.supports_track_pitch = caps.supportsTrackPitch ? 1 : 0;
    out.supports_track_loop = caps.supportsTrackLoop ? 1 : 0;
    out.decoder_count = caps.decoderCount;
    return out;
}

int hkgl_c_audio_device_count(void)
{
    return HonkordGL::Audio::GetAudioDeviceCount();
}

int hkgl_c_audio_device_info_by_index(int index, HKGL_C_AudioDeviceInfo * out_info)
{
    if (!out_info) {
        return 0;
    }
    HonkordGL::Audio::AudioDeviceInfo info{};
    if (HonkordGL::Audio::GetAudioDeviceInfoByIndex(index, &info) != 0) {
        return 0;
    }
    std::memcpy(out_info->id, info.id, sizeof(info.id));
    std::memcpy(out_info->name, info.name, sizeof(info.name));
    out_info->sample_rate_hz = info.sampleRateHz;
    out_info->channel_count = info.channelCount;
    out_info->is_default_output = info.isDefaultOutput;
    return 1;
}

void hkgl_c_audio_set_device_change_callback(HKGL_C_AudioDeviceChangeCallback fn, void * user)
{
    g_audio_device_change_cb = fn;
    g_audio_device_change_user = user;
    if (fn) {
        HonkordGL::Audio::SetAudioDeviceChangeCallback(&AudioDeviceChangeThunk, nullptr);
    } else {
        HonkordGL::Audio::ClearAudioDeviceChangeCallback();
    }
}

void hkgl_c_audio_clear_device_change_callback(void)
{
    g_audio_device_change_cb = nullptr;
    g_audio_device_change_user = nullptr;
    HonkordGL::Audio::ClearAudioDeviceChangeCallback();
}

int hkgl_c_audio_play_mixer_by_name(const char * name_tag)
{
    return HonkordGL::Audio::PlayAudioMixerByName(name_tag);
}

void * hkgl_c_audio_create_mixer_device(const char * name_tag, const HKGL_C_AudioFormat * request_or_null)
{
    HonkordGL::Audio::AudioFormat req = request_or_null ? ToCppAudioFormat(*request_or_null) : HonkordGL::Audio::AudioFormat{};
    return HonkordGL::Audio::CreateAudioMixerDevice(name_tag, request_or_null ? &req : nullptr);
}

void * hkgl_c_audio_create_mixer_buffer(const char * name_tag, const HKGL_C_AudioFormat * format_or_null, void * buffer, size_t buffer_bytes)
{
    HonkordGL::Audio::AudioFormat fmt = format_or_null ? ToCppAudioFormat(*format_or_null) : HonkordGL::Audio::AudioFormat{};
    return HonkordGL::Audio::CreateAudioMixerBuffer(name_tag, format_or_null ? &fmt : nullptr, buffer, buffer_bytes);
}

void hkgl_c_audio_destroy_mixer(void * mixer)
{
    HonkordGL::Audio::ObliterateAudioMixer(reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer));
}

int hkgl_c_audio_mixer_format(void * mixer, HKGL_C_AudioFormat * out_format)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    if (!m || !out_format) {
        return 0;
    }
    HonkordGL::Audio::AudioFormat fmt{};
    if (HonkordGL::Audio::GetAudioMixerFormat(m, &fmt) != 0) {
        return 0;
    }
    *out_format = ToCAudioFormat(fmt);
    return 1;
}

void * hkgl_c_audio_mixer_lock(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    return m ? m->Lock() : nullptr;
}

void hkgl_c_audio_mixer_unlock(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    if (m) {
        m->Unlock();
    }
}

int hkgl_c_audio_mixer_play(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    if (!m) {
        return -1;
    }
    return m->Play();
}

void hkgl_c_audio_mixer_stop(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    if (!m) {
        return;
    }
    m->Stop();
}

void * hkgl_c_audio_mixer_add_track(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    if (!m) {
        return nullptr;
    }
    return m->AddTrack();
}

void hkgl_c_audio_mixer_remove_track(void * mixer, void * track)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (m) {
        m->RemoveTrack(t);
    }
}

int hkgl_c_audio_mixer_track_count(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    return m ? m->GetTrackCount() : 0;
}

void * hkgl_c_audio_mixer_track_at(void * mixer, int index)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    return m ? m->GetTrack(index) : nullptr;
}

int hkgl_c_audio_track_play(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t) {
        return -1;
    }
    return t->Play();
}

void hkgl_c_audio_track_pause(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t) {
        return;
    }
    t->Pause();
}

void hkgl_c_audio_track_stop(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t) {
        return;
    }
    t->Stop();
}

void hkgl_c_audio_track_resume(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (t) {
        t->Resume();
    }
}

void hkgl_c_audio_track_set_volume(void * track, float linear_0_to_1)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t) {
        return;
    }
    t->SetVolume(linear_0_to_1);
}

float hkgl_c_audio_track_get_volume(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    return t ? t->GetVolume() : 0.0f;
}

void hkgl_c_audio_track_set_pitch(void * track, float ratio)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (t) {
        t->SetPitch(ratio);
    }
}

float hkgl_c_audio_track_get_pitch(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    return t ? t->GetPitch() : 0.0f;
}

int hkgl_c_audio_track_set_loop(void * track, int loop_enabled)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t) {
        return 0;
    }
    t->SetLoop(loop_enabled != 0);
    return 1;
}

int hkgl_c_audio_track_get_loop(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    return t && t->GetLoop() ? 1 : 0;
}

int hkgl_c_audio_track_is_playing(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    return t && t->IsPlaying() ? 1 : 0;
}

int hkgl_c_audio_track_is_paused(void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    return t && t->IsPaused() ? 1 : 0;
}

HKGL_C_AudioBuffer * hkgl_c_audio_buffer_create(void)
{
    return new (std::nothrow) HKGL_C_AudioBuffer();
}

void hkgl_c_audio_buffer_destroy(HKGL_C_AudioBuffer * buffer)
{
    delete buffer;
}

int hkgl_c_audio_buffer_load_cache_raw(HKGL_C_AudioBuffer * buffer, const void * data, size_t data_bytes, const HKGL_C_AudioFormat * hint)
{
    if (!buffer || !hint) {
        return -1;
    }
    return buffer->buffer.LoadCacheRaw(data, data_bytes, ToCppAudioFormat(*hint));
}

int hkgl_c_audio_buffer_load_file(HKGL_C_AudioBuffer * buffer, const char * path)
{
    if (!buffer) {
        return -1;
    }
    return buffer->buffer.LoadFromFile(path);
}

int hkgl_c_audio_buffer_generate_sine(HKGL_C_AudioBuffer * buffer, double frequency_hz, double amplitude, double duration_seconds,
    const HKGL_C_AudioFormat * format)
{
    if (!buffer || !format) {
        return -1;
    }
    return buffer->buffer.GenerateSineWave(frequency_hz, amplitude, duration_seconds, ToCppAudioFormat(*format));
}

uint64_t hkgl_c_audio_buffer_length_frames(HKGL_C_AudioBuffer * buffer)
{
    return buffer ? buffer->buffer.GetLengthFrames() : 0;
}

double hkgl_c_audio_buffer_length_seconds(HKGL_C_AudioBuffer * buffer)
{
    return buffer ? buffer->buffer.GetLengthSeconds() : 0.0;
}

void hkgl_c_audio_buffer_obliterate(HKGL_C_AudioBuffer * buffer)
{
    if (buffer) {
        buffer->buffer.Obliterate();
    }
}

int hkgl_c_audio_buffer_play(HKGL_C_AudioBuffer * buffer, void * mixer, void * track)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!buffer || !m || !t) {
        return -1;
    }
    return buffer->buffer.Play(*m, *t);
}

int hkgl_c_audio_track_set_buffer(void * track, HKGL_C_AudioBuffer * buffer)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!t || !buffer) {
        return -1;
    }
    return t->SetAudioBuffer(&buffer->buffer);
}

HKGL_C_AudioStream * hkgl_c_audio_stream_create(void)
{
    return new (std::nothrow) HKGL_C_AudioStream();
}

void hkgl_c_audio_stream_destroy(HKGL_C_AudioStream * stream)
{
    delete stream;
}

int hkgl_c_audio_stream_open_file(HKGL_C_AudioStream * stream, const char * path)
{
    return stream ? stream->stream.OpenFromFile(path) : -1;
}

void hkgl_c_audio_stream_close(HKGL_C_AudioStream * stream)
{
    if (stream) {
        stream->stream.Close();
    }
}

void hkgl_c_audio_stream_set_loop(HKGL_C_AudioStream * stream, int loop_enabled)
{
    if (stream) {
        stream->stream.SetLoop(loop_enabled != 0);
    }
}

int hkgl_c_audio_stream_get_loop(HKGL_C_AudioStream * stream)
{
    return stream && stream->stream.GetLoop() ? 1 : 0;
}

int hkgl_c_audio_stream_seek_seconds(HKGL_C_AudioStream * stream, double seconds)
{
    return stream ? stream->stream.SeekSeconds(seconds) : -1;
}

double hkgl_c_audio_stream_tell_seconds(HKGL_C_AudioStream * stream)
{
    return stream ? stream->stream.TellSeconds() : 0.0;
}

double hkgl_c_audio_stream_length_seconds(HKGL_C_AudioStream * stream)
{
    return stream ? stream->stream.GetLengthSeconds() : 0.0;
}

int hkgl_c_audio_stream_play(HKGL_C_AudioStream * stream, void * mixer, void * track)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (!stream || !m || !t) {
        return -1;
    }
    return stream->stream.Play(*m, *t);
}

void hkgl_c_audio_stream_stop(HKGL_C_AudioStream * stream)
{
    if (stream) {
        stream->stream.Stop();
    }
}

int hkgl_c_audio_stream_is_playing(HKGL_C_AudioStream * stream)
{
    return stream && stream->stream.IsPlaying() ? 1 : 0;
}

HKGL_C_AudioSource * hkgl_c_audio_source_create(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    return m ? new (std::nothrow) HKGL_C_AudioSource(*m) : nullptr;
}

void hkgl_c_audio_source_destroy(HKGL_C_AudioSource * source)
{
    delete source;
}

void * hkgl_c_audio_source_mixer(HKGL_C_AudioSource * source)
{
    return source ? source->source.GetMixer() : nullptr;
}

void * hkgl_c_audio_source_add_track(HKGL_C_AudioSource * source)
{
    return source ? source->source.AddTrack() : nullptr;
}

int hkgl_c_audio_source_track_count(HKGL_C_AudioSource * source)
{
    return source ? source->source.GetTrackCount() : 0;
}

void * hkgl_c_audio_source_track_at(HKGL_C_AudioSource * source, int index)
{
    return source ? source->source.GetTrack(index) : nullptr;
}

HKGL_C_AudioTrackGroup * hkgl_c_audio_track_group_create(void * mixer)
{
    auto * m = reinterpret_cast<HonkordGL::Audio::AudioMixer *>(mixer);
    return m ? new (std::nothrow) HKGL_C_AudioTrackGroup(*m) : nullptr;
}

void hkgl_c_audio_track_group_destroy(HKGL_C_AudioTrackGroup * group)
{
    delete group;
}

void hkgl_c_audio_track_group_add(HKGL_C_AudioTrackGroup * group, void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (group) {
        group->group.AddTrack(t);
    }
}

void hkgl_c_audio_track_group_remove(HKGL_C_AudioTrackGroup * group, void * track)
{
    auto * t = reinterpret_cast<HonkordGL::Audio::AudioTrack *>(track);
    if (group) {
        group->group.RemoveTrack(t);
    }
}

void hkgl_c_audio_track_group_set_volume(HKGL_C_AudioTrackGroup * group, float linear_0_to_1)
{
    if (group) {
        group->group.SetGroupVolume(linear_0_to_1);
    }
}

float hkgl_c_audio_track_group_get_volume(HKGL_C_AudioTrackGroup * group)
{
    return group ? group->group.GetGroupVolume() : 0.0f;
}

void hkgl_c_audio_track_group_set_muted(HKGL_C_AudioTrackGroup * group, int muted)
{
    if (group) {
        group->group.SetMuted(muted != 0);
    }
}

int hkgl_c_audio_track_group_is_muted(HKGL_C_AudioTrackGroup * group)
{
    return group && group->group.IsMuted() ? 1 : 0;
}

void * hkgl_c_audio_logical_device_create(const char * logical_name, const HKGL_C_AudioLogicalDeviceSettings * settings_or_null)
{
    HonkordGL::Audio::AudioLogicalDeviceSettings s{};
    if (settings_or_null) {
        s.preferredFormat = ToCppAudioFormat(settings_or_null->preferred_format);
        s.deviceId = settings_or_null->device_id;
        s.autoMigrate = settings_or_null->auto_migrate != 0;
    }
    return HonkordGL::Audio::CreateAudioLogicalDevice(logical_name, settings_or_null ? &s : nullptr);
}

void hkgl_c_audio_logical_device_destroy(void * device)
{
    HonkordGL::Audio::DestroyAudioLogicalDevice(reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device));
}

int hkgl_c_audio_logical_device_is_open(void * device)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    return d && d->IsOpen() ? 1 : 0;
}

int hkgl_c_audio_logical_device_format(void * device, HKGL_C_AudioFormat * out_format)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    if (!d || !out_format) {
        return 0;
    }
    *out_format = ToCAudioFormat(d->GetFormat());
    return 1;
}

const char * hkgl_c_audio_logical_device_name(void * device)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    return d ? d->GetName() : nullptr;
}

const char * hkgl_c_audio_logical_device_bound_id(void * device)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    return d ? d->GetBoundDeviceId() : nullptr;
}

int hkgl_c_audio_logical_device_auto_migrate_enabled(void * device)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    return d && d->IsAutoMigrateEnabled() ? 1 : 0;
}

void hkgl_c_audio_logical_device_set_auto_migrate(void * device, int enabled)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(device);
    if (d) {
        d->SetAutoMigrateEnabled(enabled != 0);
    }
}

void * hkgl_c_audio_unified_stream_create(void * logical_device, const HKGL_C_AudioStreamOpenSettings * settings_or_null)
{
    auto * d = reinterpret_cast<HonkordGL::Audio::AudioLogicalDevice *>(logical_device);
    if (!d) {
        return nullptr;
    }
    HonkordGL::Audio::AudioStreamOpenSettings s{};
    if (settings_or_null) {
        s.direction = static_cast<HonkordGL::Audio::AudioStreamDirection>(settings_or_null->direction);
        s.format = ToCppAudioFormat(settings_or_null->format);
        s.targetFramesPerBlock = settings_or_null->target_frames_per_block;
    }
    return HonkordGL::Audio::CreateAudioUnifiedStream(d, settings_or_null ? &s : nullptr);
}

void hkgl_c_audio_unified_stream_destroy(void * stream)
{
    HonkordGL::Audio::DestroyAudioUnifiedStream(reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream));
}

int hkgl_c_audio_unified_stream_is_open(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s && s->IsOpen() ? 1 : 0;
}

unsigned int hkgl_c_audio_unified_stream_direction(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s ? static_cast<unsigned int>(s->Direction()) : 0;
}

int hkgl_c_audio_unified_stream_format(void * stream, HKGL_C_AudioFormat * out_format)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (!s || !out_format) {
        return 0;
    }
    *out_format = ToCAudioFormat(s->GetFormat());
    return 1;
}

int hkgl_c_audio_unified_stream_queue_file(void * stream, const char * path)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s ? s->QueueFile(path) : -1;
}

int hkgl_c_audio_unified_stream_queue_pcm(void * stream, const void * data, size_t bytes, const HKGL_C_AudioFormat * format)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (!s || !format) {
        return -1;
    }
    return s->QueuePcm(data, bytes, ToCppAudioFormat(*format));
}

int hkgl_c_audio_unified_stream_play(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s ? s->Play() : -1;
}

void hkgl_c_audio_unified_stream_pause(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (s) {
        s->Pause();
    }
}

void hkgl_c_audio_unified_stream_stop(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (s) {
        s->Stop();
    }
}

int hkgl_c_audio_unified_stream_is_playing(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s && s->IsPlaying() ? 1 : 0;
}

void hkgl_c_audio_unified_stream_set_loop(void * stream, int loop_enabled)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (s) {
        s->SetLoop(loop_enabled != 0);
    }
}

int hkgl_c_audio_unified_stream_get_loop(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s && s->GetLoop() ? 1 : 0;
}

void hkgl_c_audio_unified_stream_set_volume(void * stream, float linear_0_to_1)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    if (s) {
        s->SetVolume(linear_0_to_1);
    }
}

float hkgl_c_audio_unified_stream_get_volume(void * stream)
{
    auto * s = reinterpret_cast<HonkordGL::Audio::AudioUnifiedStream *>(stream);
    return s ? s->GetVolume() : 0.0f;
}

int hkgl_c_joystick_count(void)
{
    return HonkordGL::Joystick::GetJoystickCount();
}

int hkgl_c_joystick_connected(int index)
{
    return HonkordGL::Joystick::IsJoystickConnected(index) ? 1 : 0;
}

HKGL_C_JoystickId hkgl_c_joystick_id(int index)
{
    HKGL_C_JoystickId out{};
    out.value = HonkordGL::Joystick::GetJoystickId(index).value;
    return out;
}

int hkgl_c_joystick_path(int index, char * buffer, size_t buffer_size)
{
    return HonkordGL::Joystick::GetJoystickPath(index, buffer, buffer_size) ? 1 : 0;
}

int hkgl_c_joystick_name(int index, char * buffer, size_t buffer_size)
{
    return HonkordGL::Joystick::GetJoystickName(index, buffer, buffer_size) ? 1 : 0;
}

int hkgl_c_joystick_poll_snapshot(int index, HKGL_C_JoystickInputSnapshot * out_snapshot)
{
    if (!out_snapshot) {
        return 0;
    }
    HonkordGL::Joystick::JoystickInputSnapshot snap{};
    if (!HonkordGL::Joystick::PollJoystickInputSnapshot(index, &snap)) {
        return 0;
    }
    out_snapshot->axis_count = snap.axis_count;
    for (int i = 0; i < 16; ++i) {
        out_snapshot->axes[i] = snap.axes[i];
    }
    out_snapshot->hat_x = snap.hat_x;
    out_snapshot->hat_y = snap.hat_y;
    out_snapshot->buttons_pressed_mask = snap.buttons_pressed_mask;
    out_snapshot->button_count = snap.button_count;
    return 1;
}

int hkgl_c_joystick_battery(int index, HKGL_C_JoystickBatteryInfo * out_info)
{
    if (!out_info) {
        return 0;
    }
    HonkordGL::Joystick::JoystickBatteryInfo info{};
    if (!HonkordGL::Joystick::GetJoystickBattery(index, &info)) {
        return 0;
    }
    out_info->level_percent = info.level_percent;
    out_info->power = static_cast<unsigned int>(info.power);
    return 1;
}

int hkgl_c_joystick_descriptor(int index, HKGL_C_JoystickDescriptor * out_descriptor)
{
    if (!out_descriptor) {
        return 0;
    }
    HonkordGL::Joystick::JoystickDescriptor d{};
    if (!HonkordGL::Joystick::GetJoystickDescriptor(index, &d)) {
        return 0;
    }
    *out_descriptor = ToCJoystickDescriptor(d);
    return 1;
}

int hkgl_c_joystick_player_index(int index)
{
    return HonkordGL::Joystick::GetJoystickPlayerIndex(index);
}

int hkgl_c_joystick_open(int index)
{
    return HonkordGL::Joystick::OpenJoystick(index) ? 1 : 0;
}

void hkgl_c_joystick_close(int index)
{
    HonkordGL::Joystick::CloseJoystick(index);
}

void hkgl_c_joystick_close_by_id(HKGL_C_JoystickId id)
{
    HonkordGL::Joystick::CloseJoystickById(ToCppJoystickId(id));
}

void hkgl_c_joystick_close_all(void)
{
    HonkordGL::Joystick::CloseAllJoysticks();
}

int hkgl_c_joystick_send_output_packet(int index, const HKGL_C_JoystickOutputPacket * packet)
{
    if (!packet) {
        return 0;
    }
    HonkordGL::Joystick::JoystickOutputPacket p{};
    p.flags = packet->flags;
    p.led_r = packet->led_r;
    p.led_g = packet->led_g;
    p.led_b = packet->led_b;
    p.trigger_rumble_left = packet->trigger_rumble_left;
    p.trigger_rumble_right = packet->trigger_rumble_right;
    return HonkordGL::Joystick::SendJoystickOutputPacket(index, &p) ? 1 : 0;
}

int hkgl_c_joystick_set_led_color(int index, uint8_t r, uint8_t g, uint8_t b)
{
    return HonkordGL::Joystick::SetJoystickLedColor(index, r, g, b) ? 1 : 0;
}

int hkgl_c_joystick_start_trigger_rumble(int index, float left, float right)
{
    return HonkordGL::Joystick::StartJoystickTriggerRumble(index, left, right) ? 1 : 0;
}

void hkgl_c_joystick_stop_trigger_rumble(int index)
{
    HonkordGL::Joystick::StopJoystickTriggerRumble(index);
}

int hkgl_c_joystick_attach_virtual(const HKGL_C_JoystickDescriptor * descriptor, const char * label)
{
    HonkordGL::Joystick::JoystickDescriptor d{};
    return HonkordGL::Joystick::AttachVirtualJoystick(descriptor ? &(d = ToCppJoystickDescriptor(*descriptor)) : nullptr, label);
}

void hkgl_c_joystick_detach_virtual(int index)
{
    HonkordGL::Joystick::DetachVirtualJoystick(index);
}

int hkgl_c_joystick_is_virtual(int index)
{
    return HonkordGL::Joystick::IsVirtualJoystick(index) ? 1 : 0;
}

int hkgl_c_joystick_is_virtual_id(HKGL_C_JoystickId id)
{
    return HonkordGL::Joystick::IsVirtualJoystickId(ToCppJoystickId(id)) ? 1 : 0;
}

int hkgl_c_joystick_detach_virtual_by_id(HKGL_C_JoystickId id)
{
    return HonkordGL::Joystick::DetachVirtualJoystickById(ToCppJoystickId(id)) ? 1 : 0;
}

int hkgl_c_joystick_set_virtual_axis(HKGL_C_JoystickId id, int axis_index, float value)
{
    return HonkordGL::Joystick::SetVirtualJoystickAxis(ToCppJoystickId(id), axis_index, value) ? 1 : 0;
}

int hkgl_c_joystick_set_virtual_ball_motion(HKGL_C_JoystickId id, int ball_index, float delta_x, float delta_y)
{
    return HonkordGL::Joystick::SetVirtualJoystickBallMotion(ToCppJoystickId(id), ball_index, delta_x, delta_y) ? 1 : 0;
}

int hkgl_c_joystick_set_virtual_touchpad_finger(HKGL_C_JoystickId id, int touchpad_index, int finger_index, float x, float y,
    float pressure, int pressed)
{
    return HonkordGL::Joystick::SetVirtualJoystickTouchpadFinger(ToCppJoystickId(id), touchpad_index, finger_index, x, y, pressure, pressed != 0)
        ? 1
        : 0;
}

int hkgl_c_joystick_set_virtual_sensor(HKGL_C_JoystickId id, unsigned int sensor_kind, float x, float y, float z)
{
    return HonkordGL::Joystick::SetVirtualJoystickSensor(ToCppJoystickId(id), static_cast<HonkordGL::Joystick::JoystickSensorKind>(sensor_kind), x, y, z)
        ? 1
        : 0;
}

void hkgl_c_joystick_set_hotplug_callback(HKGL_C_JoystickHotplugCallback fn, void * user)
{
    g_joystick_hotplug_cb = fn;
    g_joystick_hotplug_user = user;
    if (fn) {
        HonkordGL::Joystick::SetJoystickHotplugCallback(&JoystickHotplugThunk, nullptr);
    } else {
        HonkordGL::Joystick::ClearJoystickHotplugCallback();
    }
}

void hkgl_c_joystick_clear_hotplug_callback(void)
{
    g_joystick_hotplug_cb = nullptr;
    g_joystick_hotplug_user = nullptr;
    HonkordGL::Joystick::ClearJoystickHotplugCallback();
}

int hkgl_c_joystick_poll_hotplug(void)
{
    return HonkordGL::Joystick::PollJoystickHotplug();
}

int hkgl_c_joystick_capability_depth(int index, HKGL_C_JoystickCapabilityDepth * out_depth)
{
    if (!out_depth) {
        return 0;
    }
    HonkordGL::Joystick::JoystickCapabilityDepth d{};
    if (!HonkordGL::Joystick::GetJoystickCapabilityDepth(index, &d)) {
        return 0;
    }
    out_depth->is_virtual = d.is_virtual ? 1 : 0;
    out_depth->supports_led = d.supports_led ? 1 : 0;
    out_depth->supports_trigger_rumble = d.supports_trigger_rumble ? 1 : 0;
    out_depth->supports_battery_query = d.supports_battery_query ? 1 : 0;
    out_depth->supports_mapping_db = d.supports_mapping_db ? 1 : 0;
    out_depth->supports_hotplug_callback = d.supports_hotplug_callback ? 1 : 0;
    out_depth->supports_input_snapshot = d.supports_input_snapshot ? 1 : 0;
    out_depth->lists_evdev_only_gamepads = d.lists_evdev_only_gamepads ? 1 : 0;
    out_depth->axis_buffer_depth = d.axis_buffer_depth;
    out_depth->button_buffer_depth = d.button_buffer_depth;
    out_depth->hat_buffer_depth = d.hat_buffer_depth;
    out_depth->touchpad_buffer_depth = d.touchpad_buffer_depth;
    out_depth->sensor_buffer_depth = d.sensor_buffer_depth;
    return 1;
}

int hkgl_c_joystick_set_mapping(HKGL_C_JoystickId id, const HKGL_C_JoystickMapping * mapping)
{
    if (!mapping) {
        return 0;
    }
    HonkordGL::Joystick::JoystickMapping m = ToCppJoystickMapping(*mapping);
    return HonkordGL::Joystick::SetJoystickMapping(ToCppJoystickId(id), &m) ? 1 : 0;
}

int hkgl_c_joystick_get_mapping(HKGL_C_JoystickId id, HKGL_C_JoystickMapping * out_mapping)
{
    if (!out_mapping) {
        return 0;
    }
    HonkordGL::Joystick::JoystickMapping m{};
    if (!HonkordGL::Joystick::GetJoystickMapping(ToCppJoystickId(id), &m)) {
        return 0;
    }
    *out_mapping = ToCJoystickMapping(m);
    return 1;
}

int hkgl_c_joystick_clear_mapping(HKGL_C_JoystickId id)
{
    return HonkordGL::Joystick::ClearJoystickMapping(ToCppJoystickId(id)) ? 1 : 0;
}

int hkgl_c_joystick_mapping_count(void)
{
    return HonkordGL::Joystick::GetJoystickMappingCount();
}

int hkgl_c_joystick_save_mappings(const char * path_utf8)
{
    return HonkordGL::Joystick::SaveJoystickMappingsToFile(path_utf8) ? 1 : 0;
}

int hkgl_c_joystick_load_mappings(const char * path_utf8, int merge)
{
    return HonkordGL::Joystick::LoadJoystickMappingsFromFile(path_utf8, merge != 0) ? 1 : 0;
}

int hkgl_c_joystick_parse_gamecontrollerdb_mapping_line(const char * line_utf8, HKGL_C_JoystickMapping * out_mapping)
{
    if (!out_mapping) {
        return 0;
    }
    HonkordGL::Joystick::JoystickMapping m{};
    if (!HonkordGL::Joystick::ParseGameControllerDbMappingLine(line_utf8, &m)) {
        return 0;
    }
    *out_mapping = ToCJoystickMapping(m);
    return 1;
}

int hkgl_c_joystick_load_gamecontrollerdb_mappings(const char * path_utf8, int merge, unsigned int platform_filter)
{
    return HonkordGL::Joystick::LoadGameControllerDbMappingsFromFile(
               path_utf8, merge != 0, static_cast<HonkordGL::Joystick::GameControllerDbPlatformFilter>(platform_filter))
        ? 1
        : 0;
}

unsigned int hkgl_c_joystick_default_gamecontrollerdb_platform_filter(void)
{
    return static_cast<unsigned int>(HonkordGL::Joystick::DefaultGameControllerDbPlatformFilter());
}

int hkgl_c_joystick_gamecontrollerdb_mapping_count(void)
{
    return HonkordGL::Joystick::GetGameControllerDbMappingCount();
}

int hkgl_c_joystick_gamecontrollerdb_guid(int index, char * buffer_utf8, size_t buffer_size)
{
    return HonkordGL::Joystick::GetJoystickGameControllerDbGuid(index, buffer_utf8, buffer_size) ? 1 : 0;
}

int hkgl_c_joystick_apply_loaded_gamecontrollerdb_mappings(void)
{
    return HonkordGL::Joystick::ApplyLoadedGameControllerDbMappingsToConnectedJoysticks();
}

int hkgl_c_joystick_convert_gamecontrollerdb_file(const char * src_utf8, const char * dest_utf8, unsigned int platform_filter)
{
    return HonkordGL::Joystick::ConvertGameControllerDbFileToHonkordMappingsFile(
               src_utf8, dest_utf8, static_cast<HonkordGL::Joystick::GameControllerDbPlatformFilter>(platform_filter))
        ? 1
        : 0;
}

int hkgl_c_joystick_apply_gamecontrollerdb_mapping_for_index(int index, const char * sdl_guid_utf8)
{
    return HonkordGL::Joystick::ApplyGameControllerDbMappingForJoystickIndex(index, sdl_guid_utf8) ? 1 : 0;
}

int hkgl_c_joystick_import_gamecontrollerdb_mapping_for_index(int index, const char * line_utf8)
{
    return HonkordGL::Joystick::ImportGameControllerDbMappingForJoystickIndex(index, line_utf8) ? 1 : 0;
}

int hkgl_c_joystick_logical_scalar(const HKGL_C_JoystickInputSnapshot * snapshot, const HKGL_C_JoystickMapping * mapping,
    unsigned int logical, float * out_value)
{
    if (!snapshot || !mapping) {
        return 0;
    }
    HonkordGL::Joystick::JoystickInputSnapshot s{};
    s.axis_count = snapshot->axis_count;
    for (int i = 0; i < 16; ++i) {
        s.axes[i] = snapshot->axes[i];
    }
    s.hat_x = snapshot->hat_x;
    s.hat_y = snapshot->hat_y;
    s.buttons_pressed_mask = snapshot->buttons_pressed_mask;
    s.button_count = snapshot->button_count;
    HonkordGL::Joystick::JoystickMapping m = ToCppJoystickMapping(*mapping);
    return HonkordGL::Joystick::GetJoystickLogicalScalar(&s, &m, static_cast<HonkordGL::Joystick::JoystickLogicalInput>(logical), out_value) ? 1 : 0;
}

int hkgl_c_joystick_logical_chord_pressed(const HKGL_C_JoystickInputSnapshot * snapshot, const HKGL_C_JoystickMapping * mapping,
    const unsigned int * logicals, int logical_count, float threshold)
{
    if (!snapshot || !mapping || !logicals || logical_count < 0) {
        return 0;
    }
    HonkordGL::Joystick::JoystickInputSnapshot s{};
    s.axis_count = snapshot->axis_count;
    for (int i = 0; i < 16; ++i) {
        s.axes[i] = snapshot->axes[i];
    }
    s.hat_x = snapshot->hat_x;
    s.hat_y = snapshot->hat_y;
    s.buttons_pressed_mask = snapshot->buttons_pressed_mask;
    s.button_count = snapshot->button_count;
    HonkordGL::Joystick::JoystickMapping m = ToCppJoystickMapping(*mapping);
    HonkordGL::Joystick::JoystickLogicalInput local[32]{};
    int n = logical_count > 32 ? 32 : logical_count;
    for (int i = 0; i < n; ++i) {
        local[i] = static_cast<HonkordGL::Joystick::JoystickLogicalInput>(logicals[i]);
    }
    return HonkordGL::Joystick::AreJoystickLogicalChordPressed(&s, &m, local, n, threshold) ? 1 : 0;
}

int hkgl_c_joystick_set_action_binding(HKGL_C_JoystickId id, const HKGL_C_JoystickActionBinding * binding)
{
    if (!binding) {
        return 0;
    }
    HonkordGL::Joystick::JoystickActionBinding b{};
    std::memcpy(b.action_name, binding->action_name, sizeof(b.action_name));
    b.logical_count = binding->logical_count;
    b.threshold = binding->threshold;
    for (int i = 0; i < 8; ++i) {
        b.logicals[i] = static_cast<HonkordGL::Joystick::JoystickLogicalInput>(binding->logicals[i]);
    }
    return HonkordGL::Joystick::SetJoystickActionBinding(ToCppJoystickId(id), &b) ? 1 : 0;
}

void hkgl_c_joystick_clear_action_bindings(HKGL_C_JoystickId id)
{
    HonkordGL::Joystick::ClearJoystickActionBindings(ToCppJoystickId(id));
}

int hkgl_c_joystick_is_action_pressed(HKGL_C_JoystickId id, const HKGL_C_JoystickInputSnapshot * snapshot, const char * action_name_utf8)
{
    if (!snapshot) {
        return 0;
    }
    HonkordGL::Joystick::JoystickInputSnapshot s{};
    s.axis_count = snapshot->axis_count;
    for (int i = 0; i < 16; ++i) {
        s.axes[i] = snapshot->axes[i];
    }
    s.hat_x = snapshot->hat_x;
    s.hat_y = snapshot->hat_y;
    s.buttons_pressed_mask = snapshot->buttons_pressed_mask;
    s.button_count = snapshot->button_count;
    return HonkordGL::Joystick::IsJoystickActionPressed(ToCppJoystickId(id), &s, action_name_utf8) ? 1 : 0;
}

int hkgl_c_joystick_ensure_canonical_mapping(int index)
{
    return HonkordGL::Joystick::EnsureCanonicalJoystickMapping(index) ? 1 : 0;
}

int hkgl_c_joystick_canonical_profile_count(void)
{
    return HonkordGL::Joystick::GetCanonicalJoystickProfileCount();
}

const char * hkgl_c_get_internal_api_error(void)
{
    return HonkordGL::Graphics::GetInternalApiError();
}

} // extern "C"
