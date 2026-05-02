/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — audio mixing, buffers, tracks, and device/buffer output (API surface)
 * Copyright (C) 2026 Honkord
 */
 
#ifndef HONKORDGL_AUDIO_H
#define HONKORDGL_AUDIO_H

#include <HonkordGL/Config.h>

#include <cstddef>
#include <cstdint>

namespace HonkordGL::Audio
{
/** Bit flags for `InitializeAudioSubsystem` (combine with `|` and pass as `unsigned int`). */
enum class AudioSubsystemInit : unsigned int {
    None = 0,
    /** Prefer low-latency device streams when available. */
    LowLatency = 1u << 0,
};

/** PCM layout for buffers and mixer output. */
enum class AudioSampleFormat : unsigned char {
    Unknown = 0,
    Float32Interleaved,
    Int16Interleaved,
};

/** Where in the mixing pipeline a callback runs (inspect or modify `samples`). */
enum class AudioMixCallbackStage : unsigned char {
    /** Before any track is summed (same buffer may be unused or zero). */
    PreMix = 0,
    /** After each track is accumulated (see `AudioMixCallbackContext::trackIndex`). */
    PostTrack,
    /** Final interleaved mix before device output or memory buffer write. */
    PostMix,
};

/** Opaque user data passed to mix callbacks. */
struct AudioMixCallbackContext {
    void * user{};
    /** For `PostTrack`: index of the track just mixed; otherwise undefined. */
    int trackIndex{-1};
};

/**
 * Called at `stage` with `frameCount` frames × `channelCount` interleaved floats (writable).
 * Return non-zero if the engine should treat samples as modified (implementation-defined).
 */
using AudioMixPipelineCallback = int (*)(AudioMixCallbackStage stage,
    float * interleavedSamples,
    std::uint32_t frameCount,
    int channelCount,
    AudioMixCallbackContext * ctx);

struct AudioFormat {
    int sampleRateHz{48000};
    int channelCount{2};
    AudioSampleFormat sampleFormat{AudioSampleFormat::Float32Interleaved};
};

class AudioMixer;
class AudioBuffer;
class AudioTrack;
class AudioSource;
class AudioTrackGroup;
class AudioStream;

enum class AudioBackendKind : unsigned char {
    Stub = 0,
    Alsa,
    PipeWire,
    CoreAudio,
    AAudio,
    WASAPI,
};

struct AudioCapabilities {
    AudioBackendKind backend{AudioBackendKind::Stub};
    bool supportsStreaming{false};
    bool supportsDeviceChangeEvents{false};
    bool supportsMixPipelineCallback{true};
    bool supportsTrackPitch{true};
    bool supportsTrackLoop{true};
    int decoderCount{0};
};

struct AudioDeviceInfo {
    char id[64]{};
    char name[128]{};
    int sampleRateHz{0};
    int channelCount{0};
    int isDefaultOutput{0};
};

enum class AudioDeviceChangeEvent : unsigned char {
    DefaultOutputChanged = 0,
    DeviceLost,
    DeviceRecovered,
};

using AudioDeviceChangeCallback = void (*)(AudioDeviceChangeEvent event, char const * deviceId, void * user);

namespace detail {
struct MixerImpl;
struct TrackImpl;
MixerImpl * mixerImpl(AudioMixer const * mixer) noexcept;
TrackImpl * trackImpl(AudioTrack const * track) noexcept;
}

// ---------------------------------------------------------------------------
// Subsystem lifecycle (namespace)
// ---------------------------------------------------------------------------

/** Initializes decoder registry, device enumeration, and global state. @return 0 on success. */
HONKORDGL_API HONKORDGL_ND int InitializeAudioSubsystem(unsigned int flags = 0) noexcept;

/** Shuts down the active audio subsystem (devices stop; buffers may be invalidated). */
HONKORDGL_API void ShutdownAudioSubsystem() noexcept;

/** Full teardown: subsystem shutdown and release of all library-held audio resources. */
HONKORDGL_API void QuitAudioLibrary() noexcept;

/** @return Number of registered decoders (file formats), or 0 if uninitialized. */
HONKORDGL_API HONKORDGL_ND int GetAudioDecoderCount() noexcept;

/**
 * Copies a human-readable decoder name (UTF-8). @return 0 on success; non-zero if `index` out of range.
 */
HONKORDGL_API HONKORDGL_ND int GetAudioDecoderName(int index, char * buf, std::size_t bufBytes) noexcept;
HONKORDGL_API HONKORDGL_ND AudioBackendKind GetAudioBackendKind() noexcept;
HONKORDGL_API HONKORDGL_ND AudioCapabilities GetAudioCapabilities() noexcept;
HONKORDGL_API HONKORDGL_ND int GetAudioDeviceCount() noexcept;
HONKORDGL_API HONKORDGL_ND int GetAudioDeviceInfoByIndex(int index, AudioDeviceInfo * out) noexcept;
HONKORDGL_API void SetAudioDeviceChangeCallback(AudioDeviceChangeCallback fn, void * user) noexcept;
HONKORDGL_API void ClearAudioDeviceChangeCallback() noexcept;
/** Internal backend hook to emit device events into the public callback path. */
HONKORDGL_API void AudioEmitDeviceChange(AudioDeviceChangeEvent event, char const * deviceId) noexcept;

// ---------------------------------------------------------------------------
// Mixer factory (namespace) — single mixed output stream (device or memory)
// ---------------------------------------------------------------------------

/**
 * Creates a mixer that plays the final mix to the default (or selected) physical output device.
 * `nameTag` is a non-empty label used with `PlayAudioMixerByName`. `request` may be null for defaults.
 */
HONKORDGL_API HONKORDGL_ND AudioMixer * CreateAudioMixerDevice(char const * nameTag, AudioFormat const * request) noexcept;

/**
 * Creates a mixer that writes the mixed stream into `buffer` (interleaved samples, layout from `format`).
 * `bufferBytes` is the writable size; underrun/overflow behavior is implementation-defined.
 */
HONKORDGL_API HONKORDGL_ND AudioMixer * CreateAudioMixerBuffer(
    char const * nameTag,
    AudioFormat const * format,
    void * buffer,
    std::size_t bufferBytes) noexcept;

/** Stops output, disconnects tracks, and frees the mixer and its resources. */
HONKORDGL_API void ObliterateAudioMixer(AudioMixer * mixer) noexcept;

/** Fills `out` with the format the mixer is generating (device or buffer). @return 0 on success. */
HONKORDGL_API HONKORDGL_ND int GetAudioMixerFormat(AudioMixer const * mixer, AudioFormat * out) noexcept;

/**
 * Locks the mixer's internal mutex for thread-safe inspection or mutation of tracks/buffers.
 * Pair every call with `UnlockAudioMixer`. Returns an opaque handle (may be null if unneeded).
 */
HONKORDGL_API HONKORDGL_ND void * LockAudioMixer(AudioMixer * mixer) noexcept;

HONKORDGL_API void UnlockAudioMixer(AudioMixer * mixer) noexcept;

/** Starts or resumes mixed output on the mixer identified by `nameTag`. @return 0 on success. */
HONKORDGL_API HONKORDGL_ND int PlayAudioMixerByName(char const * nameTag) noexcept;

// ---------------------------------------------------------------------------
// AudioMixer — first class: one mixed output + pipeline callbacks
// ---------------------------------------------------------------------------

class HONKORDGL_API AudioMixer {
public:
    ~AudioMixer();

    AudioMixer(AudioMixer const &) = delete;
    AudioMixer & operator=(AudioMixer const &) = delete;
    AudioMixer(AudioMixer &&) = delete;
    AudioMixer & operator=(AudioMixer &&) = delete;

    /** Non-empty tag used for `PlayAudioMixerByName`. */
    HONKORDGL_ND char const * GetNameTag() const noexcept;
    void SetNameTag(char const * nameTag) noexcept;

    /** Replaces the pipeline callback (null disables). `user` is passed through `ctx->user`. */
    void SetMixPipelineCallback(AudioMixPipelineCallback fn, void * user) noexcept;
    void ClearMixPipelineCallback() noexcept;

    /** Output format produced by this mixer. */
    HONKORDGL_ND AudioFormat GetFormat() const noexcept;

    /** Locks the mixer's internal mutex; pair with `Unlock()`. Returns opaque handle if applicable. */
    HONKORDGL_ND void * Lock() noexcept;
    void Unlock() noexcept;

    /** Adds a playable track; the mixer owns the track until `RemoveTrack`. */
    HONKORDGL_ND AudioTrack * AddTrack() noexcept;
    void RemoveTrack(AudioTrack * track) noexcept;

    HONKORDGL_ND int GetTrackCount() const noexcept;
    HONKORDGL_ND AudioTrack * GetTrack(int index) noexcept;
    HONKORDGL_ND AudioTrack const * GetTrack(int index) const noexcept;

    /** Starts this mixer's output (device or buffer writer). @return 0 on success. */
    HONKORDGL_ND int Play() noexcept;
    void Stop() noexcept;

private:
    friend HONKORDGL_API AudioMixer * CreateAudioMixerDevice(char const *, AudioFormat const *) noexcept;
    friend HONKORDGL_API AudioMixer * CreateAudioMixerBuffer(char const *, AudioFormat const *, void *, std::size_t) noexcept;
    friend HONKORDGL_API void ObliterateAudioMixer(AudioMixer *) noexcept;
    friend struct detail::MixerImpl;
    friend detail::MixerImpl * detail::mixerImpl(AudioMixer const *) noexcept;

    AudioMixer() noexcept = default;
    struct Impl;
    Impl * impl_{nullptr};
};

// ---------------------------------------------------------------------------
// AudioTrack — one line into the mix (owned by `AudioMixer`)
// ---------------------------------------------------------------------------

class HONKORDGL_API AudioTrack {
public:
    /** Bind audio data; replaces previous buffer reference. */
    HONKORDGL_ND int SetAudioBuffer(AudioBuffer * buffer) noexcept;

    HONKORDGL_ND int Play() noexcept;
    void Pause() noexcept;
    void Stop() noexcept;
    void Resume() noexcept;

    void SetVolume(float linear0To1) noexcept;
    HONKORDGL_ND float GetVolume() const noexcept;

    /** Pitch shift factor (1.0 = original). */
    void SetPitch(float ratio) noexcept;
    HONKORDGL_ND float GetPitch() const noexcept;

    void SetLoop(bool loop) noexcept;
    HONKORDGL_ND bool GetLoop() const noexcept;

    HONKORDGL_ND bool IsPlaying() const noexcept;
    HONKORDGL_ND bool IsPaused() const noexcept;

private:
    friend class AudioMixer;
    friend class AudioSource;
    friend HONKORDGL_API void ObliterateAudioMixer(AudioMixer *) noexcept;
    friend struct detail::MixerImpl;
    friend detail::TrackImpl * detail::trackImpl(AudioTrack const *) noexcept;

    AudioTrack() noexcept = default;
    ~AudioTrack();

    struct Impl;
    Impl * impl_{nullptr};
};

HONKORDGL_API void AudioTrackPause(AudioTrack & track) noexcept;
HONKORDGL_API void AudioTrackStop(AudioTrack & track) noexcept;
HONKORDGL_API void AudioTrackResume(AudioTrack & track) noexcept;
HONKORDGL_API void AudioTrackSetVolume(AudioTrack & track, float linear0To1) noexcept;
HONKORDGL_API HONKORDGL_ND float AudioTrackGetVolume(AudioTrack const & track) noexcept;
HONKORDGL_API void AudioTrackSetPitch(AudioTrack & track, float ratio) noexcept;
HONKORDGL_API HONKORDGL_ND float AudioTrackGetPitch(AudioTrack const & track) noexcept;
HONKORDGL_API void AudioTrackSetLoop(AudioTrack & track, bool loop) noexcept;
HONKORDGL_API HONKORDGL_ND bool AudioTrackGetLoop(AudioTrack const & track) noexcept;

// ---------------------------------------------------------------------------
// AudioBuffer — second class: RAM-backed audio / procedural sine
// ---------------------------------------------------------------------------

class HONKORDGL_API AudioBuffer {
public:
    AudioBuffer() noexcept = default;
    ~AudioBuffer();

    AudioBuffer(AudioBuffer const &) = delete;
    AudioBuffer & operator=(AudioBuffer const &) = delete;
    AudioBuffer(AudioBuffer && other) noexcept;
    AudioBuffer & operator=(AudioBuffer && other) noexcept;

    /**
     * Loads a full copy of `data` into RAM in the original / specified layout (no streaming I/O).
     * @return 0 on success.
     */
    HONKORDGL_ND int LoadCacheRaw(void const * data, std::size_t dataBytes, AudioFormat const & formatHint) noexcept;

    /** Decodes `path` into a full in-memory buffer. @return 0 on success. */
    HONKORDGL_ND int LoadFromFile(char const * path) noexcept;

    /** Fills the buffer with a sine wave at `frequencyHz`, `amplitude` (0..1), `durationSeconds`. */
    HONKORDGL_ND int GenerateSineWave(
        double frequencyHz,
        double amplitude,
        double durationSeconds,
        AudioFormat const & format) noexcept;

    /** Length in sample frames (one frame = one index step across all channels). */
    HONKORDGL_ND std::uint64_t GetLengthFrames() const noexcept;

    /** Length in seconds (derived from frames and sample rate). */
    HONKORDGL_ND double GetLengthSeconds() const noexcept;

    /** Releases storage immediately; object remains usable for new loads. */
    void Obliterate() noexcept;

    /**
     * Arms playback of this buffer on `track` (assigns source and starts if the mixer is running).
     * @return 0 on success.
     */
    HONKORDGL_ND int Play(AudioMixer & mixer, AudioTrack & track) noexcept;

private:
    friend class AudioTrack;

    struct Impl;
    Impl * impl_{nullptr};
};

class HONKORDGL_API AudioStream {
public:
    AudioStream() noexcept;
    ~AudioStream();

    AudioStream(AudioStream const &) = delete;
    AudioStream & operator=(AudioStream const &) = delete;
    AudioStream(AudioStream && other) noexcept;
    AudioStream & operator=(AudioStream && other) noexcept;

    /** Opens (or reopens) a stream source path. */
    HONKORDGL_ND int OpenFromFile(char const * path) noexcept;
    void Close() noexcept;

    void SetLoop(bool loop) noexcept;
    HONKORDGL_ND bool GetLoop() const noexcept;

    /** Seek target in seconds (best-effort; backends may clamp). */
    HONKORDGL_ND int SeekSeconds(double seconds) noexcept;
    HONKORDGL_ND double TellSeconds() const noexcept;
    HONKORDGL_ND double GetLengthSeconds() const noexcept;

    /** Play this stream on a track through the target mixer. */
    HONKORDGL_ND int Play(AudioMixer & mixer, AudioTrack & track) noexcept;
    void Stop() noexcept;
    HONKORDGL_ND bool IsPlaying() const noexcept;

private:
    struct Impl;
    Impl * impl_{nullptr};
};

// ---------------------------------------------------------------------------
// AudioSource — many tracks associated with one mixer (logical bus)
// ---------------------------------------------------------------------------

class HONKORDGL_API AudioSource {
public:
    explicit AudioSource(AudioMixer & mixer) noexcept;
    ~AudioSource();

    AudioSource(AudioSource const &) = delete;
    AudioSource & operator=(AudioSource const &) = delete;

    HONKORDGL_ND AudioMixer * GetMixer() noexcept;
    HONKORDGL_ND AudioMixer const * GetMixer() const noexcept;

    /** Creates a new track on the underlying mixer and associates it with this source. */
    HONKORDGL_ND AudioTrack * AddTrack() noexcept;
    HONKORDGL_ND int GetTrackCount() const noexcept;
    HONKORDGL_ND AudioTrack * GetTrack(int index) noexcept;

private:
    struct Impl;
    Impl * impl_{nullptr};
};

// ---------------------------------------------------------------------------
// AudioTrackGroup — grouping of tracks (shared gain / mute)
// ---------------------------------------------------------------------------

class HONKORDGL_API AudioTrackGroup {
public:
    explicit AudioTrackGroup(AudioMixer & mixer) noexcept;
    ~AudioTrackGroup();

    AudioTrackGroup(AudioTrackGroup const &) = delete;
    AudioTrackGroup & operator=(AudioTrackGroup const &) = delete;

    void AddTrack(AudioTrack * track) noexcept;
    void RemoveTrack(AudioTrack * track) noexcept;

    void SetGroupVolume(float linear0To1) noexcept;
    HONKORDGL_ND float GetGroupVolume() const noexcept;

    void SetMuted(bool muted) noexcept;
    HONKORDGL_ND bool IsMuted() const noexcept;

private:
    struct Impl;
    Impl * impl_{nullptr};
};

// ---------------------------------------------------------------------------
// Unified stream architecture (logical devices + universal streams)
// ---------------------------------------------------------------------------

enum class AudioStreamDirection : unsigned char {
    Playback = 0,
    Capture = 1,
};

struct AudioLogicalDeviceSettings {
    AudioFormat preferredFormat{};
    /** Device id hint (currently "default" in this build). */
    const char * deviceId{nullptr};
    /** Keep logical endpoint alive across output device changes. */
    bool autoMigrate{true};
};

struct AudioStreamOpenSettings {
    AudioStreamDirection direction{AudioStreamDirection::Playback};
    AudioFormat format{};
    /** Hint: lower value requests lower latency. */
    unsigned int targetFramesPerBlock{512};
};

struct AudioStreamIoContext {
    void * user{};
    int channelCount{0};
    int sampleRateHz{0};
};

using AudioStreamCallback = int (*)(float * interleavedSamples, std::uint32_t frameCount, AudioStreamIoContext * ctx);

class AudioLogicalDevice;
class AudioUnifiedStream;

/** Creates a logical device abstraction (playback today; capture reserved). */
HONKORDGL_API HONKORDGL_ND AudioLogicalDevice * CreateAudioLogicalDevice(
    char const * logicalName, AudioLogicalDeviceSettings const * settings = nullptr) noexcept;
HONKORDGL_API void DestroyAudioLogicalDevice(AudioLogicalDevice * device) noexcept;

/** Creates a unified stream bound to a logical device. */
HONKORDGL_API HONKORDGL_ND AudioUnifiedStream * CreateAudioUnifiedStream(
    AudioLogicalDevice * device, AudioStreamOpenSettings const * settings = nullptr) noexcept;
HONKORDGL_API void DestroyAudioUnifiedStream(AudioUnifiedStream * stream) noexcept;

class HONKORDGL_API AudioLogicalDevice {
public:
    struct Impl;

    AudioLogicalDevice() = default;
    ~AudioLogicalDevice();

    AudioLogicalDevice(AudioLogicalDevice const &) = delete;
    AudioLogicalDevice & operator=(AudioLogicalDevice const &) = delete;
    AudioLogicalDevice(AudioLogicalDevice &&) = delete;
    AudioLogicalDevice & operator=(AudioLogicalDevice &&) = delete;

    HONKORDGL_ND bool IsOpen() const noexcept;
    HONKORDGL_ND AudioFormat GetFormat() const noexcept;
    HONKORDGL_ND char const * GetName() const noexcept;
    HONKORDGL_ND char const * GetBoundDeviceId() const noexcept;
    HONKORDGL_ND bool IsAutoMigrateEnabled() const noexcept;
    void SetAutoMigrateEnabled(bool enabled) noexcept;

private:
    friend HONKORDGL_API AudioLogicalDevice * CreateAudioLogicalDevice(char const *, AudioLogicalDeviceSettings const *) noexcept;
    friend HONKORDGL_API void DestroyAudioLogicalDevice(AudioLogicalDevice *) noexcept;
    friend HONKORDGL_API AudioUnifiedStream * CreateAudioUnifiedStream(AudioLogicalDevice *, AudioStreamOpenSettings const *) noexcept;
    friend class AudioUnifiedStream;
    Impl * impl_{nullptr};
};

class HONKORDGL_API AudioUnifiedStream {
public:
    struct Impl;

    AudioUnifiedStream() = default;
    ~AudioUnifiedStream();

    AudioUnifiedStream(AudioUnifiedStream const &) = delete;
    AudioUnifiedStream & operator=(AudioUnifiedStream const &) = delete;
    AudioUnifiedStream(AudioUnifiedStream &&) = delete;
    AudioUnifiedStream & operator=(AudioUnifiedStream &&) = delete;

    HONKORDGL_ND bool IsOpen() const noexcept;
    HONKORDGL_ND AudioStreamDirection Direction() const noexcept;
    HONKORDGL_ND AudioFormat GetFormat() const noexcept;

    /** Easy-path enqueue from file (decoded into stream source). */
    HONKORDGL_ND int QueueFile(char const * path) noexcept;
    /** Easy-path enqueue from in-memory PCM. */
    HONKORDGL_ND int QueuePcm(void const * data, std::size_t bytes, AudioFormat const & format) noexcept;

    HONKORDGL_ND int Play() noexcept;
    void Pause() noexcept;
    void Stop() noexcept;
    HONKORDGL_ND bool IsPlaying() const noexcept;

    void SetLoop(bool loop) noexcept;
    HONKORDGL_ND bool GetLoop() const noexcept;
    void SetVolume(float linear0To1) noexcept;
    HONKORDGL_ND float GetVolume() const noexcept;

    /** Optional callback (invoked through mix pipeline on playback streams). */
    void SetCallback(AudioStreamCallback callback, void * user) noexcept;
    void ClearCallback() noexcept;

private:
    friend HONKORDGL_API AudioUnifiedStream * CreateAudioUnifiedStream(AudioLogicalDevice *, AudioStreamOpenSettings const *) noexcept;
    friend HONKORDGL_API void DestroyAudioUnifiedStream(AudioUnifiedStream *) noexcept;
    Impl * impl_{nullptr};
};

} // namespace HonkordGL::Audio

#endif