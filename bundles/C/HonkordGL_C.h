/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — C ABI wrapper backed by the C++ core
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_C_API_H
#define HONKORDGL_C_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HKGL_C_App HKGL_C_App;
typedef struct HKGL_C_WindowBackend HKGL_C_WindowBackend;
typedef struct HKGL_C_GpuRenderer HKGL_C_GpuRenderer;
typedef struct HKGL_C_GpuRenderTarget HKGL_C_GpuRenderTarget;
typedef struct HKGL_C_GpuRenderPassEncoder HKGL_C_GpuRenderPassEncoder;
typedef struct HKGL_C_GpuGlStateTracker HKGL_C_GpuGlStateTracker;
typedef struct HKGL_C_GpuRenderGraph HKGL_C_GpuRenderGraph;
typedef struct HKGL_C_AudioBuffer HKGL_C_AudioBuffer;
typedef struct HKGL_C_AudioStream HKGL_C_AudioStream;
typedef struct HKGL_C_AudioSource HKGL_C_AudioSource;
typedef struct HKGL_C_AudioTrackGroup HKGL_C_AudioTrackGroup;

typedef struct HKGL_C_Event {
    unsigned int type;
    unsigned int key;
    unsigned int mouse_button;
    int x;
    int y;
    int width;
    int height;
    int focused;
    unsigned int modifiers;
    unsigned int character;
} HKGL_C_Event;

typedef struct HKGL_C_AudioFormat {
    int sample_rate_hz;
    int channel_count;
    unsigned int sample_format;
} HKGL_C_AudioFormat;

typedef struct HKGL_C_AudioCapabilities {
    unsigned int backend;
    int supports_streaming;
    int supports_device_change_events;
    int supports_mix_pipeline_callback;
    int supports_track_pitch;
    int supports_track_loop;
    int decoder_count;
} HKGL_C_AudioCapabilities;

typedef struct HKGL_C_AudioDeviceInfo {
    char id[64];
    char name[128];
    int sample_rate_hz;
    int channel_count;
    int is_default_output;
} HKGL_C_AudioDeviceInfo;

typedef struct HKGL_C_AudioLogicalDeviceSettings {
    HKGL_C_AudioFormat preferred_format;
    const char * device_id;
    int auto_migrate;
} HKGL_C_AudioLogicalDeviceSettings;

typedef struct HKGL_C_AudioStreamOpenSettings {
    unsigned int direction;
    HKGL_C_AudioFormat format;
    unsigned int target_frames_per_block;
} HKGL_C_AudioStreamOpenSettings;

typedef struct HKGL_C_JoystickId {
    uint64_t value;
} HKGL_C_JoystickId;

typedef struct HKGL_C_JoystickInputSnapshot {
    int axis_count;
    float axes[16];
    int hat_x;
    int hat_y;
    uint64_t buttons_pressed_mask;
    int button_count;
} HKGL_C_JoystickInputSnapshot;

typedef struct HKGL_C_JoystickDescriptor {
    unsigned int type;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t version_bcd;
    int button_count;
    int axis_count;
    int hat_count;
    int touchpad_count;
    int sensor_count;
} HKGL_C_JoystickDescriptor;

typedef struct HKGL_C_JoystickOutputPacket {
    uint32_t flags;
    uint8_t led_r;
    uint8_t led_g;
    uint8_t led_b;
    float trigger_rumble_left;
    float trigger_rumble_right;
} HKGL_C_JoystickOutputPacket;

typedef struct HKGL_C_JoystickCapabilityDepth {
    int is_virtual;
    int supports_led;
    int supports_trigger_rumble;
    int supports_battery_query;
    int supports_mapping_db;
    int supports_hotplug_callback;
    int supports_input_snapshot;
    int lists_evdev_only_gamepads;
    int axis_buffer_depth;
    int button_buffer_depth;
    int hat_buffer_depth;
    int touchpad_buffer_depth;
    int sensor_buffer_depth;
} HKGL_C_JoystickCapabilityDepth;

typedef struct HKGL_C_JoystickMappingEntry {
    unsigned int logical;
    unsigned int source_kind;
    int source_index;
    float scale;
    float deadzone;
} HKGL_C_JoystickMappingEntry;

typedef struct HKGL_C_JoystickMapping {
    HKGL_C_JoystickMappingEntry entries[64];
    int entry_count;
} HKGL_C_JoystickMapping;

typedef struct HKGL_C_JoystickActionBinding {
    char action_name[48];
    unsigned int logicals[8];
    int logical_count;
    float threshold;
} HKGL_C_JoystickActionBinding;

typedef struct HKGL_C_JoystickBatteryInfo {
    int level_percent;
    unsigned int power;
} HKGL_C_JoystickBatteryInfo;

typedef struct HKGL_C_GpuLimits {
    int max_texture_2d_size;
    int max_cube_map_texture_size;
    int max_renderbuffer_size;
    int max_viewport_width;
    int max_viewport_height;
    int max_vertex_attribs;
    int max_vertex_uniform_vectors;
    int max_fragment_uniform_vectors;
    int max_varying_vectors;
    int uniform_buffer_offset_alignment;
    int max_framebuffer_color_attachments;
    int max_msaa_samples;
    uint64_t gpu_timestamp_ns_per_tick;
} HKGL_C_GpuLimits;

typedef struct HKGL_C_GpuRenderTargetCreateInfo {
    int width;
    int height;
    int color_count;
    unsigned int color_formats[8];
    unsigned int depth_stencil;
} HKGL_C_GpuRenderTargetCreateInfo;

typedef struct HKGL_C_GpuRenderPassClearColor {
    float r;
    float g;
    float b;
    float a;
} HKGL_C_GpuRenderPassClearColor;

typedef struct HKGL_C_GpuRenderPassBeginInfo {
    uint32_t clear_mask;
    HKGL_C_GpuRenderPassClearColor color_clears[8];
    float depth_clear;
    int stencil_clear;
    int enable_depth_test;
    int depth_write;
    unsigned int depth_func;
} HKGL_C_GpuRenderPassBeginInfo;

typedef struct HKGL_C_GpuRenderGraphPass {
    HKGL_C_GpuRenderTarget * target;
    int surface_width;
    int surface_height;
    HKGL_C_GpuRenderPassBeginInfo begin;
    const char * debug_name;
} HKGL_C_GpuRenderGraphPass;

enum {
    HKGL_C_RENDERER_MODE_ASSISTED = 0,
    HKGL_C_RENDERER_MODE_MINIMAL = 1
};

enum {
    HKGL_C_PLATFORM_UNKNOWN = 0,
    HKGL_C_PLATFORM_WINDOWS = 1,
    HKGL_C_PLATFORM_APPLE = 2,
    HKGL_C_PLATFORM_LINUX_DESKTOP = 3,
    HKGL_C_PLATFORM_LINUX_ANDROID = 4,
    HKGL_C_PLATFORM_LINUX_RASPBERRY_PI = 5,
    HKGL_C_PLATFORM_NETBSD = 6,
    HKGL_C_PLATFORM_FREEBSD = 7,
    HKGL_C_PLATFORM_EMSCRIPTEN = 8
};

typedef void (*HKGL_C_AudioDeviceChangeCallback)(unsigned int event, const char * device_id, void * user);
typedef void (*HKGL_C_JoystickHotplugCallback)(unsigned int event, HKGL_C_JoystickId id, int index_hint, void * user);

HKGL_C_App * hkgl_c_app_create(void);
void hkgl_c_app_destroy(HKGL_C_App * app);
void hkgl_c_app_set_title(HKGL_C_App * app, const char * title_utf8);
void hkgl_c_app_set_client_rect(HKGL_C_App * app, int x, int y, int width, int height);
void hkgl_c_app_set_visible(HKGL_C_App * app, int visible);

HKGL_C_WindowBackend * hkgl_c_backend_create(void);
void hkgl_c_backend_destroy(HKGL_C_WindowBackend * backend);
bool hkgl_c_backend_initialize(HKGL_C_WindowBackend * backend, const char * display_or_app_name);
void hkgl_c_backend_shutdown(HKGL_C_WindowBackend * backend);
bool hkgl_c_backend_create_window(HKGL_C_WindowBackend * backend, HKGL_C_App * app);
void hkgl_c_backend_destroy_window(HKGL_C_WindowBackend * backend, HKGL_C_App * app);
bool hkgl_c_backend_process_events(HKGL_C_WindowBackend * backend, HKGL_C_App * app);
int hkgl_c_poll_event(HKGL_C_App * app, HKGL_C_Event * out_event);
int hkgl_c_backend_kind(HKGL_C_WindowBackend * backend);
int hkgl_c_video_driver_count(void);
int hkgl_c_build_platform(void);

HKGL_C_GpuRenderer * hkgl_c_renderer_create(void);
void hkgl_c_renderer_destroy(HKGL_C_GpuRenderer * renderer);
void hkgl_c_renderer_bind(HKGL_C_GpuRenderer * renderer, HKGL_C_App * app);
void hkgl_c_renderer_set_mode(HKGL_C_GpuRenderer * renderer, int mode);
int hkgl_c_renderer_create_best_available(HKGL_C_GpuRenderer * renderer);
int hkgl_c_renderer_create_vulkan(HKGL_C_GpuRenderer * renderer);
int hkgl_c_renderer_make_current(HKGL_C_GpuRenderer * renderer);
void hkgl_c_renderer_clear_color(HKGL_C_GpuRenderer * renderer, float r, float g, float b, float a);
void hkgl_c_renderer_swap_buffers(HKGL_C_GpuRenderer * renderer);
void hkgl_c_renderer_detach_context(HKGL_C_GpuRenderer * renderer);
int hkgl_c_renderer_query_hardware_limits(HKGL_C_GpuRenderer * renderer, HKGL_C_GpuLimits * out_limits);
int hkgl_c_renderer_try_enable_optional_feature(HKGL_C_GpuRenderer * renderer, unsigned int feature);
int hkgl_c_renderer_is_optional_feature_enabled(HKGL_C_GpuRenderer * renderer, unsigned int feature);
void hkgl_c_renderer_set_default_viewport(HKGL_C_GpuRenderer * renderer);
void hkgl_c_renderer_set_viewport(HKGL_C_GpuRenderer * renderer, int x, int y, int width, int height);
int hkgl_c_renderer_compile_shader_program(HKGL_C_GpuRenderer * renderer, const char * vertex_source, const char * fragment_source,
    unsigned int * out_program, char * info_log, size_t info_log_bytes);
void hkgl_c_renderer_delete_shader_program(HKGL_C_GpuRenderer * renderer, unsigned int program);
int hkgl_c_renderer_create_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int stage, unsigned int * out_shader);
int hkgl_c_renderer_set_shader_source(HKGL_C_GpuRenderer * renderer, unsigned int shader, const char * source);
int hkgl_c_renderer_compile_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int shader, char * info_log, size_t info_log_bytes);
int hkgl_c_renderer_create_program_object(HKGL_C_GpuRenderer * renderer, unsigned int * out_program);
void hkgl_c_renderer_attach_shader(HKGL_C_GpuRenderer * renderer, unsigned int program, unsigned int shader);
int hkgl_c_renderer_bind_attrib_location(HKGL_C_GpuRenderer * renderer, unsigned int program, unsigned int location, const char * name);
int hkgl_c_renderer_link_shader_objects(HKGL_C_GpuRenderer * renderer, unsigned int program, char * info_log, size_t info_log_bytes);
void hkgl_c_renderer_delete_shader_object(HKGL_C_GpuRenderer * renderer, unsigned int shader);
int hkgl_c_renderer_create_buffer(HKGL_C_GpuRenderer * renderer, unsigned int * out_buffer);
void hkgl_c_renderer_destroy_buffer(HKGL_C_GpuRenderer * renderer, unsigned int buffer);
int hkgl_c_renderer_bind_buffer(HKGL_C_GpuRenderer * renderer, unsigned int target, unsigned int buffer);
int hkgl_c_renderer_upload_buffer_data(HKGL_C_GpuRenderer * renderer, unsigned int target, const void * data, size_t byte_size, unsigned int usage);
int hkgl_c_renderer_create_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int * out_vao);
void hkgl_c_renderer_destroy_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int vao);
int hkgl_c_renderer_bind_vertex_array(HKGL_C_GpuRenderer * renderer, unsigned int vao);
int hkgl_c_renderer_enable_vertex_attribute(HKGL_C_GpuRenderer * renderer, unsigned int location);
int hkgl_c_renderer_set_vertex_attribute_float(
    HKGL_C_GpuRenderer * renderer, unsigned int location, int components, int stride_bytes, size_t offset_bytes, int normalized);
void hkgl_c_renderer_set_depth_test_enabled(HKGL_C_GpuRenderer * renderer, int enabled);
int hkgl_c_renderer_draw_arrays(HKGL_C_GpuRenderer * renderer, unsigned int primitive, int first_vertex, int vertex_count);
int hkgl_c_renderer_draw_indexed(
    HKGL_C_GpuRenderer * renderer, unsigned int primitive, int index_count, unsigned int index_type, size_t index_offset_bytes);

HKGL_C_GpuRenderTarget * hkgl_c_gpu_render_target_create(HKGL_C_GpuRenderer * renderer, const HKGL_C_GpuRenderTargetCreateInfo * info);
void hkgl_c_gpu_render_target_destroy(HKGL_C_GpuRenderTarget * target, HKGL_C_GpuRenderer * renderer);
int hkgl_c_gpu_render_target_resize(HKGL_C_GpuRenderTarget * target, HKGL_C_GpuRenderer * renderer, int width, int height);
int hkgl_c_gpu_render_target_width(HKGL_C_GpuRenderTarget * target);
int hkgl_c_gpu_render_target_height(HKGL_C_GpuRenderTarget * target);
int hkgl_c_gpu_render_target_color_count(HKGL_C_GpuRenderTarget * target);
unsigned int hkgl_c_gpu_render_target_color_texture(HKGL_C_GpuRenderTarget * target, int index);
unsigned int hkgl_c_gpu_render_target_depth_stencil_texture(HKGL_C_GpuRenderTarget * target);
unsigned int hkgl_c_gpu_render_target_fbo(HKGL_C_GpuRenderTarget * target);

HKGL_C_GpuRenderPassEncoder * hkgl_c_gpu_render_pass_begin(
    HKGL_C_GpuRenderer * renderer, HKGL_C_GpuRenderTarget * target, const HKGL_C_GpuRenderPassBeginInfo * info);
void hkgl_c_gpu_render_pass_end(HKGL_C_GpuRenderPassEncoder * encoder);
void hkgl_c_gpu_render_pass_set_scissor(HKGL_C_GpuRenderPassEncoder * encoder, int x, int y, int width, int height);
int hkgl_c_gpu_render_pass_last_result(HKGL_C_GpuRenderPassEncoder * encoder);

HKGL_C_GpuGlStateTracker * hkgl_c_gpu_state_tracker_create(void);
void hkgl_c_gpu_state_tracker_destroy(HKGL_C_GpuGlStateTracker * tracker);
void hkgl_c_gpu_state_tracker_invalidate(HKGL_C_GpuGlStateTracker * tracker);
void hkgl_c_gpu_state_tracker_bind_framebuffer(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, unsigned int fbo);
void hkgl_c_gpu_state_tracker_set_draw_buffer_count(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, int color_attachment_count);
void hkgl_c_gpu_state_tracker_set_viewport(HKGL_C_GpuGlStateTracker * tracker, int x, int y, int width, int height);
void hkgl_c_gpu_state_tracker_set_depth_state(
    HKGL_C_GpuGlStateTracker * tracker, int test_enabled, int depth_write, unsigned int depth_func);
void hkgl_c_gpu_state_tracker_set_scissor_enabled(
    HKGL_C_GpuGlStateTracker * tracker, int enabled, int x, int y, int width, int height);
void hkgl_c_gpu_state_tracker_set_color_mask(HKGL_C_GpuGlStateTracker * tracker, int r, int g, int b, int a);
int hkgl_c_gpu_state_tracker_begin_pass(HKGL_C_GpuGlStateTracker * tracker, HKGL_C_GpuRenderer * renderer, HKGL_C_GpuRenderTarget * target,
    const HKGL_C_GpuRenderPassBeginInfo * info, int surface_width, int surface_height);
unsigned int hkgl_c_gpu_state_tracker_cached_fbo(HKGL_C_GpuGlStateTracker * tracker);

HKGL_C_GpuRenderGraph * hkgl_c_gpu_render_graph_create(void);
void hkgl_c_gpu_render_graph_destroy(HKGL_C_GpuRenderGraph * graph);
void hkgl_c_gpu_render_graph_clear(HKGL_C_GpuRenderGraph * graph);
void hkgl_c_gpu_render_graph_reserve(HKGL_C_GpuRenderGraph * graph, size_t pass_capacity);
void hkgl_c_gpu_render_graph_add_pass(HKGL_C_GpuRenderGraph * graph, const HKGL_C_GpuRenderGraphPass * pass);
void hkgl_c_gpu_render_graph_add_dependency(HKGL_C_GpuRenderGraph * graph, int prerequisite_pass_index, int dependent_pass_index);
int hkgl_c_gpu_render_graph_compile(HKGL_C_GpuRenderGraph * graph);
int hkgl_c_gpu_render_graph_execute(HKGL_C_GpuRenderGraph * graph, HKGL_C_GpuRenderer * renderer, HKGL_C_GpuGlStateTracker * tracker,
    void (*per_pass)(void * user_data, int pass_index, const HKGL_C_GpuRenderGraphPass * pass), void * user_data);

int hkgl_c_gpu_query_limits(HKGL_C_App * app, HKGL_C_GpuLimits * out_limits);
int hkgl_c_gpu_try_enable_optional_feature(HKGL_C_App * app, unsigned int feature);
int hkgl_c_gpu_optional_feature_is_enabled(HKGL_C_App * app, unsigned int feature);
int hkgl_c_gpu_shader_load_compiler_procs(void);
int hkgl_c_gpu_shader_compile_program(HKGL_C_App * app, const char * vertex_source, const char * fragment_source,
    unsigned int * out_program, char * info_log, size_t info_log_cap, const int * attrib_locations, const char * const * attrib_names,
    int attrib_count);
void hkgl_c_gpu_shader_delete_program(unsigned int program);
int hkgl_c_gpu_shader_create_object(HKGL_C_App * app, unsigned int stage, unsigned int * out_shader);
int hkgl_c_gpu_shader_set_source(HKGL_C_App * app, unsigned int shader, const char * source);
int hkgl_c_gpu_shader_compile_object(HKGL_C_App * app, unsigned int shader, char * info_log, size_t info_log_cap);
int hkgl_c_gpu_shader_create_program_object(HKGL_C_App * app, unsigned int * out_program);
void hkgl_c_gpu_shader_attach(HKGL_C_App * app, unsigned int program, unsigned int shader);
int hkgl_c_gpu_shader_bind_attrib(HKGL_C_App * app, unsigned int program, unsigned int location, const char * name);
int hkgl_c_gpu_shader_link_program(HKGL_C_App * app, unsigned int program, char * info_log, size_t info_log_cap);
void hkgl_c_gpu_shader_delete_object(unsigned int shader);

int hkgl_c_audio_initialize(unsigned int flags);
void hkgl_c_audio_shutdown(void);
int hkgl_c_audio_decoder_count(void);
int hkgl_c_audio_decoder_name(int index, char * buf, size_t buf_bytes);
unsigned int hkgl_c_audio_backend_kind(void);
HKGL_C_AudioCapabilities hkgl_c_audio_capabilities(void);
int hkgl_c_audio_device_count(void);
int hkgl_c_audio_device_info_by_index(int index, HKGL_C_AudioDeviceInfo * out_info);
void hkgl_c_audio_set_device_change_callback(HKGL_C_AudioDeviceChangeCallback fn, void * user);
void hkgl_c_audio_clear_device_change_callback(void);
int hkgl_c_audio_play_mixer_by_name(const char * name_tag);
void * hkgl_c_audio_create_mixer_device(const char * name_tag, const HKGL_C_AudioFormat * request_or_null);
void * hkgl_c_audio_create_mixer_buffer(const char * name_tag, const HKGL_C_AudioFormat * format_or_null, void * buffer, size_t buffer_bytes);
void hkgl_c_audio_destroy_mixer(void * mixer);
int hkgl_c_audio_mixer_format(void * mixer, HKGL_C_AudioFormat * out_format);
void * hkgl_c_audio_mixer_lock(void * mixer);
void hkgl_c_audio_mixer_unlock(void * mixer);
int hkgl_c_audio_mixer_play(void * mixer);
void hkgl_c_audio_mixer_stop(void * mixer);
void * hkgl_c_audio_mixer_add_track(void * mixer);
void hkgl_c_audio_mixer_remove_track(void * mixer, void * track);
int hkgl_c_audio_mixer_track_count(void * mixer);
void * hkgl_c_audio_mixer_track_at(void * mixer, int index);
int hkgl_c_audio_track_play(void * track);
void hkgl_c_audio_track_pause(void * track);
void hkgl_c_audio_track_stop(void * track);
void hkgl_c_audio_track_resume(void * track);
void hkgl_c_audio_track_set_volume(void * track, float linear_0_to_1);
float hkgl_c_audio_track_get_volume(void * track);
void hkgl_c_audio_track_set_pitch(void * track, float ratio);
float hkgl_c_audio_track_get_pitch(void * track);
int hkgl_c_audio_track_set_loop(void * track, int loop_enabled);
int hkgl_c_audio_track_get_loop(void * track);
int hkgl_c_audio_track_is_playing(void * track);
int hkgl_c_audio_track_is_paused(void * track);

HKGL_C_AudioBuffer * hkgl_c_audio_buffer_create(void);
void hkgl_c_audio_buffer_destroy(HKGL_C_AudioBuffer * buffer);
int hkgl_c_audio_buffer_load_cache_raw(HKGL_C_AudioBuffer * buffer, const void * data, size_t data_bytes, const HKGL_C_AudioFormat * hint);
int hkgl_c_audio_buffer_load_file(HKGL_C_AudioBuffer * buffer, const char * path);
int hkgl_c_audio_buffer_generate_sine(HKGL_C_AudioBuffer * buffer, double frequency_hz, double amplitude, double duration_seconds,
    const HKGL_C_AudioFormat * format);
uint64_t hkgl_c_audio_buffer_length_frames(HKGL_C_AudioBuffer * buffer);
double hkgl_c_audio_buffer_length_seconds(HKGL_C_AudioBuffer * buffer);
void hkgl_c_audio_buffer_obliterate(HKGL_C_AudioBuffer * buffer);
int hkgl_c_audio_buffer_play(HKGL_C_AudioBuffer * buffer, void * mixer, void * track);
int hkgl_c_audio_track_set_buffer(void * track, HKGL_C_AudioBuffer * buffer);

HKGL_C_AudioStream * hkgl_c_audio_stream_create(void);
void hkgl_c_audio_stream_destroy(HKGL_C_AudioStream * stream);
int hkgl_c_audio_stream_open_file(HKGL_C_AudioStream * stream, const char * path);
void hkgl_c_audio_stream_close(HKGL_C_AudioStream * stream);
void hkgl_c_audio_stream_set_loop(HKGL_C_AudioStream * stream, int loop_enabled);
int hkgl_c_audio_stream_get_loop(HKGL_C_AudioStream * stream);
int hkgl_c_audio_stream_seek_seconds(HKGL_C_AudioStream * stream, double seconds);
double hkgl_c_audio_stream_tell_seconds(HKGL_C_AudioStream * stream);
double hkgl_c_audio_stream_length_seconds(HKGL_C_AudioStream * stream);
int hkgl_c_audio_stream_play(HKGL_C_AudioStream * stream, void * mixer, void * track);
void hkgl_c_audio_stream_stop(HKGL_C_AudioStream * stream);
int hkgl_c_audio_stream_is_playing(HKGL_C_AudioStream * stream);

HKGL_C_AudioSource * hkgl_c_audio_source_create(void * mixer);
void hkgl_c_audio_source_destroy(HKGL_C_AudioSource * source);
void * hkgl_c_audio_source_mixer(HKGL_C_AudioSource * source);
void * hkgl_c_audio_source_add_track(HKGL_C_AudioSource * source);
int hkgl_c_audio_source_track_count(HKGL_C_AudioSource * source);
void * hkgl_c_audio_source_track_at(HKGL_C_AudioSource * source, int index);

HKGL_C_AudioTrackGroup * hkgl_c_audio_track_group_create(void * mixer);
void hkgl_c_audio_track_group_destroy(HKGL_C_AudioTrackGroup * group);
void hkgl_c_audio_track_group_add(HKGL_C_AudioTrackGroup * group, void * track);
void hkgl_c_audio_track_group_remove(HKGL_C_AudioTrackGroup * group, void * track);
void hkgl_c_audio_track_group_set_volume(HKGL_C_AudioTrackGroup * group, float linear_0_to_1);
float hkgl_c_audio_track_group_get_volume(HKGL_C_AudioTrackGroup * group);
void hkgl_c_audio_track_group_set_muted(HKGL_C_AudioTrackGroup * group, int muted);
int hkgl_c_audio_track_group_is_muted(HKGL_C_AudioTrackGroup * group);

void * hkgl_c_audio_logical_device_create(const char * logical_name, const HKGL_C_AudioLogicalDeviceSettings * settings_or_null);
void hkgl_c_audio_logical_device_destroy(void * device);
int hkgl_c_audio_logical_device_is_open(void * device);
int hkgl_c_audio_logical_device_format(void * device, HKGL_C_AudioFormat * out_format);
const char * hkgl_c_audio_logical_device_name(void * device);
const char * hkgl_c_audio_logical_device_bound_id(void * device);
int hkgl_c_audio_logical_device_auto_migrate_enabled(void * device);
void hkgl_c_audio_logical_device_set_auto_migrate(void * device, int enabled);

void * hkgl_c_audio_unified_stream_create(void * logical_device, const HKGL_C_AudioStreamOpenSettings * settings_or_null);
void hkgl_c_audio_unified_stream_destroy(void * stream);
int hkgl_c_audio_unified_stream_is_open(void * stream);
unsigned int hkgl_c_audio_unified_stream_direction(void * stream);
int hkgl_c_audio_unified_stream_format(void * stream, HKGL_C_AudioFormat * out_format);
int hkgl_c_audio_unified_stream_queue_file(void * stream, const char * path);
int hkgl_c_audio_unified_stream_queue_pcm(void * stream, const void * data, size_t bytes, const HKGL_C_AudioFormat * format);
int hkgl_c_audio_unified_stream_play(void * stream);
void hkgl_c_audio_unified_stream_pause(void * stream);
void hkgl_c_audio_unified_stream_stop(void * stream);
int hkgl_c_audio_unified_stream_is_playing(void * stream);
void hkgl_c_audio_unified_stream_set_loop(void * stream, int loop_enabled);
int hkgl_c_audio_unified_stream_get_loop(void * stream);
void hkgl_c_audio_unified_stream_set_volume(void * stream, float linear_0_to_1);
float hkgl_c_audio_unified_stream_get_volume(void * stream);

int hkgl_c_joystick_count(void);
int hkgl_c_joystick_connected(int index);
HKGL_C_JoystickId hkgl_c_joystick_id(int index);
int hkgl_c_joystick_path(int index, char * buffer, size_t buffer_size);
int hkgl_c_joystick_name(int index, char * buffer, size_t buffer_size);
int hkgl_c_joystick_poll_snapshot(int index, HKGL_C_JoystickInputSnapshot * out_snapshot);
int hkgl_c_joystick_battery(int index, HKGL_C_JoystickBatteryInfo * out_info);
int hkgl_c_joystick_descriptor(int index, HKGL_C_JoystickDescriptor * out_descriptor);
int hkgl_c_joystick_player_index(int index);
int hkgl_c_joystick_open(int index);
void hkgl_c_joystick_close(int index);
void hkgl_c_joystick_close_by_id(HKGL_C_JoystickId id);
void hkgl_c_joystick_close_all(void);
int hkgl_c_joystick_send_output_packet(int index, const HKGL_C_JoystickOutputPacket * packet);
int hkgl_c_joystick_set_led_color(int index, uint8_t r, uint8_t g, uint8_t b);
int hkgl_c_joystick_start_trigger_rumble(int index, float left, float right);
void hkgl_c_joystick_stop_trigger_rumble(int index);
int hkgl_c_joystick_attach_virtual(const HKGL_C_JoystickDescriptor * descriptor, const char * label);
void hkgl_c_joystick_detach_virtual(int index);
int hkgl_c_joystick_is_virtual(int index);
int hkgl_c_joystick_is_virtual_id(HKGL_C_JoystickId id);
int hkgl_c_joystick_detach_virtual_by_id(HKGL_C_JoystickId id);
int hkgl_c_joystick_set_virtual_axis(HKGL_C_JoystickId id, int axis_index, float value);
int hkgl_c_joystick_set_virtual_ball_motion(HKGL_C_JoystickId id, int ball_index, float delta_x, float delta_y);
int hkgl_c_joystick_set_virtual_touchpad_finger(HKGL_C_JoystickId id, int touchpad_index, int finger_index, float x, float y,
    float pressure, int pressed);
int hkgl_c_joystick_set_virtual_sensor(HKGL_C_JoystickId id, unsigned int sensor_kind, float x, float y, float z);
void hkgl_c_joystick_set_hotplug_callback(HKGL_C_JoystickHotplugCallback fn, void * user);
void hkgl_c_joystick_clear_hotplug_callback(void);
int hkgl_c_joystick_poll_hotplug(void);
int hkgl_c_joystick_capability_depth(int index, HKGL_C_JoystickCapabilityDepth * out_depth);
int hkgl_c_joystick_set_mapping(HKGL_C_JoystickId id, const HKGL_C_JoystickMapping * mapping);
int hkgl_c_joystick_get_mapping(HKGL_C_JoystickId id, HKGL_C_JoystickMapping * out_mapping);
int hkgl_c_joystick_clear_mapping(HKGL_C_JoystickId id);
int hkgl_c_joystick_mapping_count(void);
int hkgl_c_joystick_save_mappings(const char * path_utf8);
int hkgl_c_joystick_load_mappings(const char * path_utf8, int merge);
int hkgl_c_joystick_parse_gamecontrollerdb_mapping_line(const char * line_utf8, HKGL_C_JoystickMapping * out_mapping);
int hkgl_c_joystick_load_gamecontrollerdb_mappings(const char * path_utf8, int merge, unsigned int platform_filter);
unsigned int hkgl_c_joystick_default_gamecontrollerdb_platform_filter(void);
int hkgl_c_joystick_gamecontrollerdb_mapping_count(void);
int hkgl_c_joystick_gamecontrollerdb_guid(int index, char * buffer_utf8, size_t buffer_size);
int hkgl_c_joystick_apply_loaded_gamecontrollerdb_mappings(void);
int hkgl_c_joystick_convert_gamecontrollerdb_file(const char * src_utf8, const char * dest_utf8, unsigned int platform_filter);
int hkgl_c_joystick_apply_gamecontrollerdb_mapping_for_index(int index, const char * sdl_guid_utf8);
int hkgl_c_joystick_import_gamecontrollerdb_mapping_for_index(int index, const char * line_utf8);
int hkgl_c_joystick_logical_scalar(const HKGL_C_JoystickInputSnapshot * snapshot, const HKGL_C_JoystickMapping * mapping,
    unsigned int logical, float * out_value);
int hkgl_c_joystick_logical_chord_pressed(const HKGL_C_JoystickInputSnapshot * snapshot, const HKGL_C_JoystickMapping * mapping,
    const unsigned int * logicals, int logical_count, float threshold);
int hkgl_c_joystick_set_action_binding(HKGL_C_JoystickId id, const HKGL_C_JoystickActionBinding * binding);
void hkgl_c_joystick_clear_action_bindings(HKGL_C_JoystickId id);
int hkgl_c_joystick_is_action_pressed(HKGL_C_JoystickId id, const HKGL_C_JoystickInputSnapshot * snapshot, const char * action_name_utf8);
int hkgl_c_joystick_ensure_canonical_mapping(int index);
int hkgl_c_joystick_canonical_profile_count(void);

const char * hkgl_c_get_internal_api_error(void);

#ifdef __cplusplus
}
#endif

#endif
