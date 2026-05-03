/**
 * HonkordGL — cross-backend audio capabilities/stream helpers
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Audio.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace HonkordGL::Audio
{

namespace
{
std::mutex g_device_cb_mu;
AudioDeviceChangeCallback g_device_cb = nullptr;
void * g_device_cb_user = nullptr;

AudioBackendKind DetectBackendKind() noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    return AudioBackendKind::WASAPI;
#elif HONKORDGL_PLATFORM_APPLE
    return AudioBackendKind::CoreAudio;
#elif HONKORDGL_PLATFORM_ANDROID
    return AudioBackendKind::AAudio;
#elif HONKORDGL_PLATFORM_LINUX
# if HONKORDGL_AUDIO_USE_PIPEWIRE
    return AudioBackendKind::PipeWire;
# else
    return AudioBackendKind::Alsa;
# endif
#else
    return AudioBackendKind::Stub;
#endif
}

AudioCapabilities BuildCapabilities() noexcept
{
    AudioCapabilities c{};
    c.backend = DetectBackendKind();
    c.supportsStreaming = true;
    c.supportsDeviceChangeEvents = true;
    c.supportsMixPipelineCallback = true;
    c.supportsTrackPitch = true;
    c.supportsTrackLoop = true;
    c.decoderCount = GetAudioDecoderCount();
    return c;
}

} // namespace

AudioBackendKind GetAudioBackendKind() noexcept
{
    return DetectBackendKind();
}

AudioCapabilities GetAudioCapabilities() noexcept
{
    return BuildCapabilities();
}

int GetAudioDeviceCount() noexcept
{
    return 1;
}

int GetAudioDeviceInfoByIndex(int index, AudioDeviceInfo * out) noexcept
{
    if (!out || index != 0)
        return -1;
    std::memset(out, 0, sizeof(*out));
    std::snprintf(out->id, sizeof(out->id), "%s", "default");
    std::snprintf(out->name, sizeof(out->name), "%s", "Default Output Device");
    out->sampleRateHz = 48000;
    out->channelCount = 2;
    out->isDefaultOutput = 1;
    return 0;
}

void SetAudioDeviceChangeCallback(AudioDeviceChangeCallback fn, void * user) noexcept
{
    std::lock_guard<std::mutex> lk(g_device_cb_mu);
    g_device_cb = fn;
    g_device_cb_user = user;
}

void ClearAudioDeviceChangeCallback() noexcept
{
    std::lock_guard<std::mutex> lk(g_device_cb_mu);
    g_device_cb = nullptr;
    g_device_cb_user = nullptr;
}

void AudioEmitDeviceChange(AudioDeviceChangeEvent event, char const * deviceId) noexcept
{
    AudioDeviceChangeCallback fn = nullptr;
    void * user = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_device_cb_mu);
        fn = g_device_cb;
        user = g_device_cb_user;
    }
    if (fn)
        fn(event, deviceId ? deviceId : "default", user);
}

struct AudioStream::Impl {
    std::string path;
    AudioBuffer buffer{};
    bool loaded{false};
    bool playing{false};
    bool loop{false};
    double positionSec{0.0};
};

AudioStream::AudioStream() noexcept
    : impl_(new Impl())
{
}

AudioStream::~AudioStream()
{
    delete impl_;
    impl_ = nullptr;
}

AudioStream::AudioStream(AudioStream && other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

AudioStream & AudioStream::operator=(AudioStream && other) noexcept
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

int AudioStream::OpenFromFile(char const * path) noexcept
{
    if (!impl_ || !path || !path[0])
        return -1;
    impl_->path = path;
    impl_->loaded = false;
    impl_->playing = false;
    impl_->positionSec = 0.0;
    return 0;
}

void AudioStream::Close() noexcept
{
    if (!impl_)
        return;
    impl_->buffer.Obliterate();
    impl_->path.clear();
    impl_->loaded = false;
    impl_->playing = false;
    impl_->positionSec = 0.0;
}

void AudioStream::SetLoop(bool loop) noexcept
{
    if (impl_)
        impl_->loop = loop;
}

bool AudioStream::GetLoop() const noexcept
{
    return impl_ && impl_->loop;
}

int AudioStream::SeekSeconds(double seconds) noexcept
{
    if (!impl_)
        return -1;
    impl_->positionSec = (std::max)(0.0, seconds);
    return 0;
}

double AudioStream::TellSeconds() const noexcept
{
    return impl_ ? impl_->positionSec : 0.0;
}

double AudioStream::GetLengthSeconds() const noexcept
{
    if (!impl_ || !impl_->loaded)
        return 0.0;
    return impl_->buffer.GetLengthSeconds();
}

int AudioStream::Play(AudioMixer & mixer, AudioTrack & track) noexcept
{
    if (!impl_)
        return -1;
    if (!impl_->loaded) {
        if (impl_->path.empty())
            return -1;
        if (impl_->buffer.LoadFromFile(impl_->path.c_str()) != 0)
            return -1;
        impl_->loaded = true;
    }
    track.SetLoop(impl_->loop);
    const int rc = impl_->buffer.Play(mixer, track);
    impl_->playing = rc == 0;
    return rc;
}

void AudioStream::Stop() noexcept
{
    if (!impl_)
        return;
    impl_->playing = false;
}

bool AudioStream::IsPlaying() const noexcept
{
    return impl_ && impl_->playing;
}

struct AudioLogicalDevice::Impl {
    std::string logicalName{};
    std::string deviceId{"default"};
    bool autoMigrate{true};
    AudioFormat format{};
    AudioMixer * mixer{nullptr};
    std::vector<struct AudioUnifiedStream::Impl *> streams{};
    std::mutex streamMu{};
};

struct AudioUnifiedStream::Impl {
    AudioLogicalDevice::Impl * device{};
    AudioStreamDirection direction{AudioStreamDirection::Playback};
    AudioFormat format{};
    AudioTrack * track{};
    AudioBuffer queuedBuffer{};
    AudioStream queuedStream{};
    bool loop{false};
    float volume{1.f};
    bool playing{false};
    AudioStreamCallback callback{nullptr};
    void * callbackUser{nullptr};
};

static int LogicalDeviceMixCallback(
    AudioMixCallbackStage stage, float * interleavedSamples, std::uint32_t frameCount, int channelCount, AudioMixCallbackContext * ctx)
{
    if (stage != AudioMixCallbackStage::PostMix || !ctx || !ctx->user || !interleavedSamples)
        return 0;
    auto * const device = static_cast<AudioLogicalDevice::Impl *>(ctx->user);
    std::lock_guard<std::mutex> lk(device->streamMu);
    AudioStreamIoContext io{};
    io.channelCount = channelCount;
    io.sampleRateHz = device->format.sampleRateHz;
    int modified = 0;
    for (auto * s : device->streams) {
        if (!s || !s->callback)
            continue;
        io.user = s->callbackUser;
        modified |= s->callback(interleavedSamples, frameCount, &io);
    }
    return modified;
}

AudioLogicalDevice * CreateAudioLogicalDevice(char const * logicalName, AudioLogicalDeviceSettings const * settings) noexcept
{
    AudioFormat req = settings ? settings->preferredFormat : AudioFormat{};
    AudioMixer * m = CreateAudioMixerDevice(logicalName ? logicalName : "logical_device", &req);
    if (!m)
        return nullptr;
    auto * out = new AudioLogicalDevice();
    out->impl_ = new AudioLogicalDevice::Impl();
    out->impl_->logicalName = logicalName ? logicalName : "logical_device";
    out->impl_->deviceId = settings && settings->deviceId ? settings->deviceId : "default";
    out->impl_->autoMigrate = settings ? settings->autoMigrate : true;
    out->impl_->mixer = m;
    out->impl_->format = m->GetFormat();
    out->impl_->mixer->SetMixPipelineCallback(LogicalDeviceMixCallback, out->impl_);
    return out;
}

void DestroyAudioLogicalDevice(AudioLogicalDevice * device) noexcept
{
    if (!device)
        return;
    delete device;
}

AudioUnifiedStream * CreateAudioUnifiedStream(AudioLogicalDevice * device, AudioStreamOpenSettings const * settings) noexcept
{
    if (!device || !device->impl_ || !device->impl_->mixer)
        return nullptr;
    auto * s = new AudioUnifiedStream();
    s->impl_ = new AudioUnifiedStream::Impl();
    s->impl_->device = device->impl_;
    s->impl_->direction = settings ? settings->direction : AudioStreamDirection::Playback;
    s->impl_->format = settings ? settings->format : device->impl_->format;
    if (s->impl_->direction == AudioStreamDirection::Capture) {
        // Capture stream object exists for architecture symmetry; capture backend can be wired later.
        s->impl_->track = nullptr;
    } else {
        s->impl_->track = device->impl_->mixer->AddTrack();
        if (!s->impl_->track) {
            delete s->impl_;
            delete s;
            return nullptr;
        }
    }
    {
        std::lock_guard<std::mutex> lk(device->impl_->streamMu);
        device->impl_->streams.push_back(s->impl_);
    }
    return s;
}

void DestroyAudioUnifiedStream(AudioUnifiedStream * stream) noexcept
{
    if (!stream)
        return;
    delete stream;
}

AudioLogicalDevice::~AudioLogicalDevice()
{
    if (!impl_)
        return;
    if (impl_->mixer) {
        impl_->mixer->ClearMixPipelineCallback();
        ObliterateAudioMixer(impl_->mixer);
        impl_->mixer = nullptr;
    }
    delete impl_;
    impl_ = nullptr;
}

bool AudioLogicalDevice::IsOpen() const noexcept
{
    return impl_ && impl_->mixer;
}

AudioFormat AudioLogicalDevice::GetFormat() const noexcept
{
    return impl_ ? impl_->format : AudioFormat{};
}

char const * AudioLogicalDevice::GetName() const noexcept
{
    return impl_ ? impl_->logicalName.c_str() : "";
}

char const * AudioLogicalDevice::GetBoundDeviceId() const noexcept
{
    return impl_ ? impl_->deviceId.c_str() : "";
}

bool AudioLogicalDevice::IsAutoMigrateEnabled() const noexcept
{
    return impl_ && impl_->autoMigrate;
}

void AudioLogicalDevice::SetAutoMigrateEnabled(bool enabled) noexcept
{
    if (impl_)
        impl_->autoMigrate = enabled;
}

AudioUnifiedStream::~AudioUnifiedStream()
{
    if (!impl_)
        return;
    if (impl_->device) {
        std::lock_guard<std::mutex> lk(impl_->device->streamMu);
        auto & vec = impl_->device->streams;
        vec.erase(std::remove(vec.begin(), vec.end(), impl_), vec.end());
        if (impl_->track && impl_->device->mixer)
            impl_->device->mixer->RemoveTrack(impl_->track);
    }
    delete impl_;
    impl_ = nullptr;
}

bool AudioUnifiedStream::IsOpen() const noexcept
{
    return impl_ && impl_->device && impl_->device->mixer;
}

AudioStreamDirection AudioUnifiedStream::Direction() const noexcept
{
    return impl_ ? impl_->direction : AudioStreamDirection::Playback;
}

AudioFormat AudioUnifiedStream::GetFormat() const noexcept
{
    return impl_ ? impl_->format : AudioFormat{};
}

int AudioUnifiedStream::QueueFile(char const * path) noexcept
{
    if (!impl_ || impl_->direction != AudioStreamDirection::Playback)
        return -1;
    int const rc = impl_->queuedStream.OpenFromFile(path);
    return rc;
}

int AudioUnifiedStream::QueuePcm(void const * data, std::size_t bytes, AudioFormat const & format) noexcept
{
    if (!impl_ || impl_->direction != AudioStreamDirection::Playback)
        return -1;
    return impl_->queuedBuffer.LoadCacheRaw(data, bytes, format);
}

int AudioUnifiedStream::Play() noexcept
{
    if (!impl_ || !impl_->device || !impl_->device->mixer || impl_->direction != AudioStreamDirection::Playback || !impl_->track)
        return -1;
    impl_->track->SetLoop(impl_->loop);
    impl_->track->SetVolume(impl_->volume);
    int rc = impl_->queuedStream.Play(*impl_->device->mixer, *impl_->track);
    if (rc != 0)
        rc = impl_->queuedBuffer.Play(*impl_->device->mixer, *impl_->track);
    if (rc == 0)
        rc = impl_->device->mixer->Play();
    impl_->playing = (rc == 0);
    return rc;
}

void AudioUnifiedStream::Pause() noexcept
{
    if (impl_ && impl_->track)
        impl_->track->Pause();
}

void AudioUnifiedStream::Stop() noexcept
{
    if (impl_ && impl_->track)
        impl_->track->Stop();
    if (impl_)
        impl_->playing = false;
}

bool AudioUnifiedStream::IsPlaying() const noexcept
{
    if (!impl_)
        return false;
    return impl_->track ? impl_->track->IsPlaying() : impl_->playing;
}

void AudioUnifiedStream::SetLoop(bool loop) noexcept
{
    if (!impl_)
        return;
    impl_->loop = loop;
    if (impl_->track)
        impl_->track->SetLoop(loop);
}

bool AudioUnifiedStream::GetLoop() const noexcept
{
    return impl_ && impl_->loop;
}

void AudioUnifiedStream::SetVolume(float linear0To1) noexcept
{
    if (!impl_)
        return;
    impl_->volume = std::clamp(linear0To1, 0.f, 1.f);
    if (impl_->track)
        impl_->track->SetVolume(impl_->volume);
}

float AudioUnifiedStream::GetVolume() const noexcept
{
    return impl_ ? impl_->volume : 0.f;
}

void AudioUnifiedStream::SetCallback(AudioStreamCallback callback, void * user) noexcept
{
    if (!impl_)
        return;
    impl_->callback = callback;
    impl_->callbackUser = user;
}

void AudioUnifiedStream::ClearCallback() noexcept
{
    SetCallback(nullptr, nullptr);
}

} // namespace HonkordGL::Audio