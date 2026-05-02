/**
 * HonkordGL — Audio API implementation (AAudio on Android; stubs elsewhere)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Audio.h>
#include <HonkordGL/PlatformAdapter.h>
#include "AudioDecodeCommon.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if HONKORDGL_PLATFORM_ANDROID

#include <aaudio/AAudio.h>

namespace HonkordGL::Audio
{
namespace detail
{

constexpr int kPeriodFrames = 512;
constexpr unsigned kDecoderCount = 2;
static char const kDecoderWav[] = "WAV (PCM)";
static char const kDecoderOgg[] = "OGG (Vorbis)";

static std::mutex g_subsysMutex;
static int g_initCount = 0;
static unsigned g_subsysInitFlags = 0;

static std::mutex g_mixerMapMutex;
static std::unordered_map<std::string, AudioMixer *> g_mixersByName;

struct BufferData {
    std::vector<float> interleaved;
    int sampleRateHz{48000};
    int channelCount{2};
};

static void FloatToS16(float const * src, std::int16_t * dst, std::size_t sampleCount) noexcept
{
    for (std::size_t i = 0; i < sampleCount; ++i) {
        float x = std::clamp(src[i], -1.f, 1.f);
        dst[i] = static_cast<std::int16_t>(x * 32767.f);
    }
}
bool ParseWavFile(char const * path, BufferData & out) noexcept
{
    FILE * f = std::fopen(path, "rb");
    if (!f)
        return false;
    unsigned char hdr[12]{};
    if (std::fread(hdr, 1, 12, f) != 12u || std::memcmp(hdr, "RIFF", 4) != 0 || std::memcmp(hdr + 8, "WAVE", 4) != 0) {
        std::fclose(f);
        return false;
    }
    std::uint16_t audioFormat = 0;
    std::uint16_t numChannels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    bool gotFmt = false;
    bool gotData = false;
    std::vector<unsigned char> rawPcm;

    while (!feof(f)) {
        unsigned char ch[8]{};
        if (std::fread(ch, 1, 8, f) != 8u)
            break;
        std::uint32_t chunkSize = static_cast<std::uint32_t>(ch[4]) | (static_cast<std::uint32_t>(ch[5]) << 8u)
            | (static_cast<std::uint32_t>(ch[6]) << 16u) | (static_cast<std::uint32_t>(ch[7]) << 24u);
        if (std::memcmp(ch, "fmt ", 4) == 0) {
            std::vector<unsigned char> fmt(chunkSize);
            if (std::fread(fmt.data(), 1, chunkSize, f) != chunkSize) {
                std::fclose(f);
                return false;
            }
            if (chunkSize < 16u) {
                std::fclose(f);
                return false;
            }
            audioFormat = static_cast<std::uint16_t>(fmt[0]) | (static_cast<std::uint16_t>(fmt[1]) << 8);
            numChannels = static_cast<std::uint16_t>(fmt[2]) | (static_cast<std::uint16_t>(fmt[3]) << 8);
            sampleRate = static_cast<std::uint32_t>(fmt[4]) | (static_cast<std::uint32_t>(fmt[5]) << 8u)
                | (static_cast<std::uint32_t>(fmt[6]) << 16u) | (static_cast<std::uint32_t>(fmt[7]) << 24u);
            bitsPerSample = static_cast<std::uint16_t>(fmt[14]) | (static_cast<std::uint16_t>(fmt[15]) << 8);
            gotFmt = true;
            continue;
        }
        if (std::memcmp(ch, "data", 4) == 0) {
            rawPcm.resize(chunkSize);
            if (std::fread(rawPcm.data(), 1, chunkSize, f) != chunkSize) {
                std::fclose(f);
                return false;
            }
            gotData = true;
            break;
        }
        if (chunkSize > 0 && std::fseek(f, static_cast<long>(chunkSize), SEEK_CUR) != 0) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    if (!gotFmt || !gotData || numChannels == 0 || sampleRate == 0)
        return false;
    if (audioFormat != 1 && audioFormat != 3) {
        return false;
    }

    out.sampleRateHz = static_cast<int>(sampleRate);
    out.channelCount = static_cast<int>(numChannels);

    if (audioFormat == 1 && bitsPerSample == 16) {
        std::size_t const nSamp = rawPcm.size() / 2u;
        out.interleaved.resize(nSamp);
        auto const * p = reinterpret_cast<std::int16_t const *>(rawPcm.data());
        for (std::size_t i = 0; i < nSamp; ++i)
            out.interleaved[i] = static_cast<float>(p[i]) / 32768.f;
        return true;
    }
    if (audioFormat == 3 && bitsPerSample == 32) {
        std::size_t const nSamp = rawPcm.size() / 4u;
        out.interleaved.resize(nSamp);
        std::memcpy(out.interleaved.data(), rawPcm.data(), rawPcm.size());
        return true;
    }
    return false;
}

struct TrackGroupState {
    float groupVolume{1.f};
    bool muted{false};
};

struct MixerImpl;

struct TrackImpl {
    MixerImpl * mixer{};
    AudioBuffer * buffer{};
    BufferData * bufferData{};
    double readPos{0.0};
    float volume{1.f};
    float pitch{1.f};
    bool loop{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    TrackGroupState * group{};
};

struct MixerImpl {
    AudioMixer * self{};
    std::string nameTag;
    AudioFormat format{};
    bool isBufferMode{false};
    void * userBuffer{};
    std::size_t userBufferBytes{};
    std::atomic<std::size_t> bufferWriteByte{0};

    AAudioStream * stream{nullptr};

    mutable std::mutex mutex;
    std::vector<float> mixBuf;
    std::vector<float> trackScratchBuf;
    std::vector<std::int16_t> s16ScratchBuf;

    std::vector<AudioTrack *> trackOwners;
    std::vector<AudioTrack *> trackList;

    AudioMixPipelineCallback mixCb{};
    void * mixCbUser{};

    std::atomic<bool> workerStop{true};
    std::thread worker;

    void runMixForFrames(std::uint32_t frameCount) noexcept
    {
        int const ch = format.channelCount > 0 ? format.channelCount : 2;
        std::size_t const sampleCount = frameCount * static_cast<std::size_t>(ch);
        mixBuf.assign(sampleCount, 0.f);
        trackScratchBuf.resize(sampleCount);
        AudioMixCallbackContext ctx{};

        std::lock_guard<std::mutex> lock(mutex);
        ctx.user = mixCbUser;

        if (mixCb) {
            ctx.trackIndex = -1;
            (void)mixCb(AudioMixCallbackStage::PreMix, mixBuf.data(), frameCount, ch, &ctx);
        }

        for (std::size_t ti = 0; ti < trackList.size(); ++ti) {
            AudioTrack * const tr = trackList[ti];
            if (!tr || !tr->impl_)
                continue;
            TrackImpl & t = *reinterpret_cast<TrackImpl *>(tr->impl_);
            if (!t.playing.load(std::memory_order_acquire) || t.paused.load(std::memory_order_acquire))
                continue;
            BufferData * bd = t.bufferData;
            if (!bd || bd->interleaved.empty())
                continue;
            int const bc = bd->channelCount;
            if (bc != ch)
                continue;

            std::fill(trackScratchBuf.begin(), trackScratchBuf.end(), 0.f);
            std::uint64_t const totalFrames = bd->interleaved.size() / static_cast<std::size_t>(bd->channelCount);
            float const vol = t.volume * (t.group && !t.group->muted ? t.group->groupVolume : 1.f);

            for (std::size_t f = 0; f < frameCount; ++f) {
                double const pos = t.readPos;
                std::uint64_t const i0 = static_cast<std::uint64_t>(std::floor(pos));
                double const frac = static_cast<float>(pos - static_cast<double>(i0));

                if (i0 >= totalFrames) {
                    if (t.loop && totalFrames > 0) {
                        t.readPos = 0.0;
                    } else {
                        t.playing.store(false, std::memory_order_release);
                        break;
                    }
                }
                if (!t.playing.load(std::memory_order_acquire))
                    break;

                std::uint64_t i1 = i0 + 1;
                if (t.loop && i1 >= totalFrames)
                    i1 = 0;

                for (int c = 0; c < bc; ++c) {
                    std::size_t const idx0 = i0 * static_cast<std::uint64_t>(bc) + static_cast<std::size_t>(c);
                    std::size_t const idx1 = i1 * static_cast<std::uint64_t>(bc) + static_cast<std::size_t>(c);
                    float s0 = (i0 < totalFrames) ? bd->interleaved[idx0] : 0.f;
                    float s1 = (i1 < totalFrames) ? bd->interleaved[idx1] : s0;
                    float s = s0 + static_cast<float>(frac) * (s1 - s0);
                    trackScratchBuf[f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c)] = s * vol;
                }
                t.readPos += static_cast<double>(t.pitch);
            }

            for (std::size_t s = 0; s < sampleCount; ++s)
                mixBuf[s] += trackScratchBuf[s];

            if (mixCb) {
                ctx.trackIndex = static_cast<int>(ti);
                (void)mixCb(AudioMixCallbackStage::PostTrack, mixBuf.data(), frameCount, ch, &ctx);
            }
        }

        if (mixCb) {
            ctx.trackIndex = -1;
            (void)mixCb(AudioMixCallbackStage::PostMix, mixBuf.data(), frameCount, ch, &ctx);
        }
    }

    void renderToOutputFloats(float * out, std::uint32_t frameCount) noexcept
    {
        runMixForFrames(frameCount);
        int const ch = format.channelCount > 0 ? format.channelCount : 2;
        std::size_t const n = static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(ch);
        std::memcpy(out, mixBuf.data(), n * sizeof(float));
    }

    void workerLoop() noexcept
    {
        unsigned const period = static_cast<unsigned>(kPeriodFrames);
        int const ch = format.channelCount > 0 ? format.channelCount : 2;

        while (!workerStop.load(std::memory_order_acquire)) {
            std::size_t const frameCount = period;
            std::size_t const sampleCount = frameCount * static_cast<std::size_t>(ch);

            runMixForFrames(static_cast<std::uint32_t>(frameCount));

            if (isBufferMode && userBuffer && userBufferBytes > 0) {
                std::size_t const bytesPerFrame =
                    format.sampleFormat == AudioSampleFormat::Int16Interleaved
                        ? static_cast<std::size_t>(ch) * 2u
                        : static_cast<std::size_t>(ch) * 4u;
                std::size_t const outChunkBytes = frameCount * bytesPerFrame;
                std::size_t pos = bufferWriteByte.load(std::memory_order_relaxed) % userBufferBytes;
                std::size_t canWrite = outChunkBytes;
                if (canWrite > userBufferBytes)
                    canWrite = userBufferBytes;

                if (format.sampleFormat == AudioSampleFormat::Float32Interleaved) {
                    float * dst = static_cast<float *>(userBuffer);
                    std::size_t floatCount = outChunkBytes / sizeof(float);
                    for (std::size_t i = 0; i < floatCount && i < sampleCount; ++i) {
                        std::size_t o = (pos / sizeof(float) + i) % (userBufferBytes / sizeof(float));
                        dst[o] = mixBuf[i];
                    }
                } else {
                    s16ScratchBuf.resize(sampleCount);
                    FloatToS16(mixBuf.data(), s16ScratchBuf.data(), sampleCount);
                    std::memcpy(
                        static_cast<unsigned char *>(userBuffer) + pos,
                        s16ScratchBuf.data(),
                        std::min(outChunkBytes, userBufferBytes - pos));
                }
                bufferWriteByte.fetch_add(outChunkBytes, std::memory_order_relaxed);
            }

            if (workerStop.load(std::memory_order_acquire))
                break;
        }
    }
    void startWorker() noexcept
    {
        if (isBufferMode) {
            if (!workerStop.exchange(false, std::memory_order_acq_rel))
                return;
            worker = std::thread([this] { workerLoop(); });
        }
    }
    void stopWorker() noexcept
    {
        if (isBufferMode) {
            workerStop.store(true, std::memory_order_release);
            if (worker.joinable())
                worker.join();
        }
    }
};

static aaudio_data_callback_result_t aaudioDataCallback(
    AAudioStream * stream,
    void * userData,
    void * audioData,
    int32_t numFrames)
{
    (void)stream;
    auto * m = static_cast<MixerImpl *>(userData);
    if (!m || m->isBufferMode || numFrames <= 0 || !audioData)
        return AAUDIO_CALLBACK_RESULT_STOP;
    m->renderToOutputFloats(static_cast<float *>(audioData), static_cast<std::uint32_t>(numFrames));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
bool openAAudio(MixerImpl & out, AudioFormat const & req, unsigned subsystemFlags) noexcept
{
    AAudioStreamBuilder * builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder)
        return false;

    int const rate = req.sampleRateHz > 0 ? req.sampleRateHz : 48000;
    int const ch = req.channelCount > 0 ? req.channelCount : 2;

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, rate);
    AAudioStreamBuilder_setChannelCount(builder, ch);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    if ((subsystemFlags & static_cast<unsigned>(AudioSubsystemInit::LowLatency)) != 0u)
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    else
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setDataCallback(builder, aaudioDataCallback, &out);

    AAudioStream * stream = nullptr;
    aaudio_result_t const result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream)
        return false;

    out.stream = stream;
    out.format.sampleRateHz = AAudioStream_getSampleRate(stream);
    out.format.channelCount = AAudioStream_getChannelCount(stream);
    out.format.sampleFormat = AudioSampleFormat::Float32Interleaved;
    return true;
}

} // namespace detail

struct AudioMixer::Impl : detail::MixerImpl {};
struct AudioTrack::Impl : detail::TrackImpl {};

namespace detail
{

MixerImpl * mixerImpl(AudioMixer const * m) noexcept
{
    return m && m->impl_ ? static_cast<MixerImpl *>(reinterpret_cast<AudioMixer::Impl *>(m->impl_)) : nullptr;
}
TrackImpl * trackImpl(AudioTrack const * t) noexcept
{
    return t && t->impl_ ? static_cast<TrackImpl *>(reinterpret_cast<AudioTrack::Impl *>(t->impl_)) : nullptr;
}

} // namespace detail

struct AudioBuffer::Impl {
    detail::BufferData data;
};

AudioBuffer::~AudioBuffer()
{
    Obliterate();
}
AudioBuffer::AudioBuffer(AudioBuffer && other) noexcept
    : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

AudioBuffer & AudioBuffer::operator=(AudioBuffer && other) noexcept
{
    if (this != &other) {
        Obliterate();
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}
int AudioBuffer::LoadCacheRaw(void const * data, std::size_t dataBytes, AudioFormat const & formatHint) noexcept
{
    if (!data || dataBytes == 0)
        return -1;
    if (!impl_)
        impl_ = new Impl();
    auto & d = impl_->data;
    d.sampleRateHz = formatHint.sampleRateHz > 0 ? formatHint.sampleRateHz : 48000;
    d.channelCount = formatHint.channelCount > 0 ? formatHint.channelCount : 2;
    if (formatHint.sampleFormat == AudioSampleFormat::Float32Interleaved) {
        std::size_t const nFloat = dataBytes / sizeof(float);
        d.interleaved.resize(nFloat);
        std::memcpy(d.interleaved.data(), data, nFloat * sizeof(float));
        return 0;
    }
    if (formatHint.sampleFormat == AudioSampleFormat::Int16Interleaved) {
        std::size_t const nSamp = dataBytes / 2u;
        d.interleaved.resize(nSamp);
        auto const * p = static_cast<std::int16_t const *>(data);
        for (std::size_t i = 0; i < nSamp; ++i)
            d.interleaved[i] = static_cast<float>(p[i]) / 32768.f;
        return 0;
    }
    return -1;
}
int AudioBuffer::LoadFromFile(char const * path) noexcept
{
    if (!path)
        return -1;
    if (!impl_)
        impl_ = new Impl();
    return detail::DecodeAudioFileCommon(path, impl_->data) ? 0 : -1;
}

int AudioBuffer::GenerateSineWave(
    double frequencyHz,
    double amplitude,
    double durationSeconds,
    AudioFormat const & format) noexcept
{
    if (frequencyHz <= 0.0 || durationSeconds <= 0.0)
        return -1;
    if (!impl_)
        impl_ = new Impl();
    int const rate = format.sampleRateHz > 0 ? format.sampleRateHz : 48000;
    int const ch = format.channelCount > 0 ? format.channelCount : 2;
    impl_->data.sampleRateHz = rate;
    impl_->data.channelCount = ch;
    std::uint64_t const frames = static_cast<std::uint64_t>(std::llround(durationSeconds * static_cast<double>(rate)));
    impl_->data.interleaved.assign(static_cast<std::size_t>(frames) * static_cast<std::size_t>(ch), 0.f);
    double const twopi = 6.28318530717958647692;
    float const amp = static_cast<float>(std::clamp(amplitude, 0.0, 1.0));
    for (std::uint64_t f = 0; f < frames; ++f) {
        float const s =
            amp * std::sin(static_cast<float>(twopi * frequencyHz * static_cast<double>(f) / static_cast<double>(rate)));
        for (int c = 0; c < ch; ++c)
            impl_->data.interleaved[f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c)] = s;
    }
    return 0;
}
std::uint64_t AudioBuffer::GetLengthFrames() const noexcept
{
    if (!impl_ || impl_->data.channelCount <= 0)
        return 0;
    return impl_->data.interleaved.size() / static_cast<std::size_t>(impl_->data.channelCount);
}
double AudioBuffer::GetLengthSeconds() const noexcept
{
    if (!impl_ || impl_->data.sampleRateHz <= 0)
        return 0.0;
    return static_cast<double>(GetLengthFrames()) / static_cast<double>(impl_->data.sampleRateHz);
}
void AudioBuffer::Obliterate() noexcept
{
    delete impl_;
    impl_ = nullptr;
}
int AudioBuffer::Play(AudioMixer & mixer, AudioTrack & track) noexcept
{
    if (track.SetAudioBuffer(this) != 0)
        return -1;
    (void)mixer;
    return track.Play();
}

AudioTrack::~AudioTrack()
{
    delete reinterpret_cast<AudioTrack::Impl *>(impl_);
    impl_ = nullptr;
}

int AudioTrack::SetAudioBuffer(AudioBuffer * buffer) noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (!t)
        return -1;
    t->buffer = buffer;
    t->bufferData =
        buffer && buffer->impl_ ? &reinterpret_cast<AudioBuffer::Impl *>(buffer->impl_)->data : nullptr;
    return 0;
}
int AudioTrack::Play() noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (!t)
        return -1;
    t->readPos = 0.0;
    t->paused.store(false, std::memory_order_release);
    t->playing.store(true, std::memory_order_release);
    return 0;
}
void AudioTrack::Pause() noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t)
        t->paused.store(true, std::memory_order_release);
}
void AudioTrack::Stop() noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t) {
        t->playing.store(false, std::memory_order_release);
        t->paused.store(false, std::memory_order_release);
        t->readPos = 0.0;
    }
}
void AudioTrack::Resume() noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t) {
        t->paused.store(false, std::memory_order_release);
        t->playing.store(true, std::memory_order_release);
    }
}
void AudioTrack::SetVolume(float linear0To1) noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t)
        t->volume = std::clamp(linear0To1, 0.f, 1.f);
}
float AudioTrack::GetVolume() const noexcept
{
    detail::TrackImpl const * t = detail::trackImpl(this);
    return t ? t->volume : 0.f;
}

void AudioTrack::SetPitch(float ratio) noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t)
        t->pitch = std::max(0.01f, ratio);
}
float AudioTrack::GetPitch() const noexcept
{
    detail::TrackImpl const * t = detail::trackImpl(this);
    return t ? t->pitch : 1.f;
}
void AudioTrack::SetLoop(bool loop) noexcept
{
    detail::TrackImpl * t = detail::trackImpl(this);
    if (t)
        t->loop = loop;
}
bool AudioTrack::GetLoop() const noexcept
{
    detail::TrackImpl const * t = detail::trackImpl(this);
    return t && t->loop;
}
bool AudioTrack::IsPlaying() const noexcept
{
    detail::TrackImpl const * t = detail::trackImpl(this);
    return t && t->playing.load(std::memory_order_acquire);
}
bool AudioTrack::IsPaused() const noexcept
{
    detail::TrackImpl const * t = detail::trackImpl(this);
    return t && t->paused.load(std::memory_order_acquire);
}
void AudioTrackPause(AudioTrack & track) noexcept
{
    track.Pause();
}
void AudioTrackStop(AudioTrack & track) noexcept
{
    track.Stop();
}
void AudioTrackResume(AudioTrack & track) noexcept
{
    track.Resume();
}
void AudioTrackSetVolume(AudioTrack & track, float linear0To1) noexcept
{
    track.SetVolume(linear0To1);
}
float AudioTrackGetVolume(AudioTrack const & track) noexcept
{
    return track.GetVolume();
}
void AudioTrackSetPitch(AudioTrack & track, float ratio) noexcept
{
    track.SetPitch(ratio);
}
float AudioTrackGetPitch(AudioTrack const & track) noexcept
{
    return track.GetPitch();
}
void AudioTrackSetLoop(AudioTrack & track, bool loop) noexcept
{
    track.SetLoop(loop);
}
bool AudioTrackGetLoop(AudioTrack const & track) noexcept
{
    return track.GetLoop();
}

AudioMixer::~AudioMixer()
{
    ObliterateAudioMixer(this);
}

char const * AudioMixer::GetNameTag() const noexcept
{
    detail::MixerImpl const * m = detail::mixerImpl(this);
    return m ? m->nameTag.c_str() : "";
}
void AudioMixer::SetNameTag(char const * nameTag) noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m || !nameTag)
        return;
    std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
    detail::g_mixersByName.erase(m->nameTag);
    m->nameTag = nameTag;
    detail::g_mixersByName[m->nameTag] = this;
}
void AudioMixer::SetMixPipelineCallback(AudioMixPipelineCallback fn, void * user) noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return;
    std::lock_guard<std::mutex> lock(m->mutex);
    m->mixCb = fn;
    m->mixCbUser = user;
}
void AudioMixer::ClearMixPipelineCallback() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return;
    std::lock_guard<std::mutex> lock(m->mutex);
    m->mixCb = nullptr;
    m->mixCbUser = nullptr;
}
AudioFormat AudioMixer::GetFormat() const noexcept
{
    detail::MixerImpl const * m = detail::mixerImpl(this);
    return m ? m->format : AudioFormat{};
}
void * AudioMixer::Lock() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return nullptr;
    m->mutex.lock();
    return &m->mutex;
}
void AudioMixer::Unlock() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (m)
        m->mutex.unlock();
}
AudioTrack * AudioMixer::AddTrack() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return nullptr;
    auto * raw = new AudioTrack();
    raw->impl_ = new AudioTrack::Impl();
    detail::TrackImpl * ti = static_cast<detail::TrackImpl *>(reinterpret_cast<AudioTrack::Impl *>(raw->impl_));
    ti->mixer = m;
    std::lock_guard<std::mutex> lock(m->mutex);
    m->trackOwners.push_back(raw);
    m->trackList.push_back(raw);
    return raw;
}
void AudioMixer::RemoveTrack(AudioTrack * track) noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m || !track)
        return;
    std::lock_guard<std::mutex> lock(m->mutex);
    for (auto it = m->trackList.begin(); it != m->trackList.end(); ++it) {
        if (*it == track) {
            m->trackList.erase(it);
            break;
        }
    }
    for (auto it = m->trackOwners.begin(); it != m->trackOwners.end(); ++it) {
        if (*it == track) {
            m->trackOwners.erase(it);
            delete track;
            return;
        }
    }
}
int AudioMixer::GetTrackCount() const noexcept
{
    detail::MixerImpl const * m = detail::mixerImpl(this);
    if (!m)
        return 0;
    std::lock_guard<std::mutex> lock(m->mutex);
    return static_cast<int>(m->trackList.size());
}
AudioTrack * AudioMixer::GetTrack(int index) noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m || index < 0)
        return nullptr;
    std::lock_guard<std::mutex> lock(m->mutex);
    return static_cast<std::size_t>(index) < m->trackList.size() ? m->trackList[static_cast<std::size_t>(index)]
                                                                   : nullptr;
}
AudioTrack const * AudioMixer::GetTrack(int index) const noexcept
{
    return const_cast<AudioMixer *>(this)->GetTrack(index);
}
int AudioMixer::Play() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return -1;
    m->startWorker();
    if (m->stream) {
        aaudio_result_t const r = AAudioStream_requestStart(m->stream);
        if (r != AAUDIO_OK)
            return static_cast<int>(r);
    }
    return 0;
}
void AudioMixer::Stop() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return;
    m->stopWorker();
    if (m->stream)
        (void)AAudioStream_requestPause(m->stream);
}
int InitializeAudioSubsystem(unsigned int flags) noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    detail::g_subsysInitFlags |= flags;
    ++detail::g_initCount;
    AudioEmitDeviceChange(AudioDeviceChangeEvent::DeviceRecovered, "default");
    return 0;
}
void ShutdownAudioSubsystem() noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    if (detail::g_initCount > 0) {
        --detail::g_initCount;
        AudioEmitDeviceChange(AudioDeviceChangeEvent::DeviceLost, "default");
    }
}
void QuitAudioLibrary() noexcept
{
    std::vector<AudioMixer *> toKill;
    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        for (auto & kv : detail::g_mixersByName)
            toKill.push_back(kv.second);
        detail::g_mixersByName.clear();
    }
    for (AudioMixer * mx : toKill)
        ObliterateAudioMixer(mx);
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    detail::g_initCount = 0;
    detail::g_subsysInitFlags = 0;
}
int GetAudioDecoderCount() noexcept
{
    return detail::g_initCount > 0 ? static_cast<int>(detail::kDecoderCount) : 0;
}
int GetAudioDecoderName(int index, char * buf, std::size_t bufBytes) noexcept
{
    if (!buf || bufBytes == 0)
        return -1;
    if (index < 0 || index >= static_cast<int>(detail::kDecoderCount))
        return -1;
    if (index == 0)
        std::snprintf(buf, bufBytes, "%s", detail::kDecoderWav);
    else
        std::snprintf(buf, bufBytes, "%s", detail::kDecoderOgg);
    return 0;
}
AudioMixer * CreateAudioMixerDevice(char const * nameTag, AudioFormat const * request) noexcept
{
    if (!nameTag || nameTag[0] == '\0')
        return nullptr;
    if (detail::g_initCount <= 0)
        return nullptr;

    auto * mimpl = new AudioMixer::Impl();
    mimpl->nameTag = nameTag;
    mimpl->isBufferMode = false;
    AudioFormat req = request ? *request : AudioFormat{};
    if (!detail::openAAudio(*mimpl, req, detail::g_subsysInitFlags)) {
        delete mimpl;
        return nullptr;
    }

    auto * mixer = new AudioMixer();
    mixer->impl_ = mimpl;
    mimpl->self = mixer;

    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        detail::g_mixersByName[mimpl->nameTag] = mixer;
    }
    AudioEmitDeviceChange(AudioDeviceChangeEvent::DefaultOutputChanged, "default");
    return mixer;
}
AudioMixer * CreateAudioMixerBuffer(char const * nameTag, AudioFormat const * format, void * buffer, std::size_t bufferBytes) noexcept
{
    if (!nameTag || nameTag[0] == '\0' || !buffer || bufferBytes == 0 || !format)
        return nullptr;
    if (detail::g_initCount <= 0)
        return nullptr;

    auto * mimpl = new AudioMixer::Impl();
    mimpl->nameTag = nameTag;
    mimpl->isBufferMode = true;
    mimpl->userBuffer = buffer;
    mimpl->userBufferBytes = bufferBytes;
    mimpl->format = *format;
    if (mimpl->format.sampleRateHz <= 0)
        mimpl->format.sampleRateHz = 48000;
    if (mimpl->format.channelCount <= 0)
        mimpl->format.channelCount = 2;

    auto * mixer = new AudioMixer();
    mixer->impl_ = mimpl;
    mimpl->self = mixer;

    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        detail::g_mixersByName[mimpl->nameTag] = mixer;
    }
    return mixer;
}
void ObliterateAudioMixer(AudioMixer * mixer) noexcept
{
    if (!mixer || !mixer->impl_)
        return;
    auto * mimpl = static_cast<detail::MixerImpl *>(reinterpret_cast<AudioMixer::Impl *>(mixer->impl_));

    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        detail::g_mixersByName.erase(mimpl->nameTag);
    }

    mimpl->stopWorker();
    if (mimpl->stream) {
        (void)AAudioStream_requestStop(mimpl->stream);
        (void)AAudioStream_close(mimpl->stream);
        mimpl->stream = nullptr;
    }

    std::lock_guard<std::mutex> lock(mimpl->mutex);
    for (AudioTrack * t : mimpl->trackOwners)
        delete t;
    mimpl->trackOwners.clear();
    mimpl->trackList.clear();

    delete reinterpret_cast<AudioMixer::Impl *>(mixer->impl_);
    mixer->impl_ = nullptr;
}
int GetAudioMixerFormat(AudioMixer const * mixer, AudioFormat * out) noexcept
{
    if (!mixer || !out)
        return -1;
    detail::MixerImpl const * m = detail::mixerImpl(mixer);
    if (!m)
        return -1;
    *out = m->format;
    return 0;
}
void * LockAudioMixer(AudioMixer * mixer) noexcept
{
    return mixer ? mixer->Lock() : nullptr;
}
void UnlockAudioMixer(AudioMixer * mixer) noexcept
{
    if (mixer)
        mixer->Unlock();
}
int PlayAudioMixerByName(char const * nameTag) noexcept
{
    if (!nameTag)
        return -1;
    AudioMixer * m = nullptr;
    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        auto it = detail::g_mixersByName.find(nameTag);
        if (it != detail::g_mixersByName.end())
            m = it->second;
    }
    return m ? m->Play() : -1;
}

struct AudioSource::Impl {
    AudioMixer * mixer{};
    std::vector<AudioTrack *> tracks;
};

AudioSource::AudioSource(AudioMixer & mixer) noexcept
    : impl_(new Impl)
{
    impl_->mixer = &mixer;
}
AudioSource::~AudioSource()
{
    delete impl_;
    impl_ = nullptr;
}

AudioMixer * AudioSource::GetMixer() noexcept
{
    return impl_ ? impl_->mixer : nullptr;
}
AudioMixer const * AudioSource::GetMixer() const noexcept
{
    return impl_ ? impl_->mixer : nullptr;
}
AudioTrack * AudioSource::AddTrack() noexcept
{
    if (!impl_ || !impl_->mixer)
        return nullptr;
    AudioTrack * t = impl_->mixer->AddTrack();
    if (t)
        impl_->tracks.push_back(t);
    return t;
}
int AudioSource::GetTrackCount() const noexcept
{
    return impl_ ? static_cast<int>(impl_->tracks.size()) : 0;
}
AudioTrack * AudioSource::GetTrack(int index) noexcept
{
    if (!impl_ || index < 0 || static_cast<std::size_t>(index) >= impl_->tracks.size())
        return nullptr;
    return impl_->tracks[static_cast<std::size_t>(index)];
}

struct AudioTrackGroup::Impl {
    detail::MixerImpl * mixer{};
    std::vector<AudioTrack *> members;
    detail::TrackGroupState state{};
};

AudioTrackGroup::AudioTrackGroup(AudioMixer & mixer) noexcept
    : impl_(new Impl)
{
    impl_->mixer = detail::mixerImpl(&mixer);
}
AudioTrackGroup::~AudioTrackGroup()
{
    if (impl_) {
        for (AudioTrack * t : impl_->members) {
            detail::TrackImpl * ti = detail::trackImpl(t);
            if (ti)
                ti->group = nullptr;
        }
    }
    delete impl_;
    impl_ = nullptr;
}

void AudioTrackGroup::AddTrack(AudioTrack * track) noexcept
{
    if (!impl_ || !track)
        return;
    impl_->members.push_back(track);
    detail::TrackImpl * ti = detail::trackImpl(track);
    if (ti)
        ti->group = &impl_->state;
}
void AudioTrackGroup::RemoveTrack(AudioTrack * track) noexcept
{
    if (!impl_ || !track)
        return;
    auto & v = impl_->members;
    v.erase(std::remove(v.begin(), v.end(), track), v.end());
    detail::TrackImpl * ti = detail::trackImpl(track);
    if (ti)
        ti->group = nullptr;
}
void AudioTrackGroup::SetGroupVolume(float linear0To1) noexcept
{
    if (impl_)
        impl_->state.groupVolume = std::clamp(linear0To1, 0.f, 1.f);
}
float AudioTrackGroup::GetGroupVolume() const noexcept
{
    return impl_ ? impl_->state.groupVolume : 0.f;
}
void AudioTrackGroup::SetMuted(bool muted) noexcept
{
    if (impl_)
        impl_->state.muted = muted;
}
bool AudioTrackGroup::IsMuted() const noexcept
{
    return impl_ && impl_->state.muted;
}

} // namespace HonkordGL::Audio

#else

namespace HonkordGL::Audio
{

struct AudioBuffer::Impl {};

AudioBuffer::~AudioBuffer() = default;
AudioBuffer::AudioBuffer(AudioBuffer &&) noexcept = default;
AudioBuffer & AudioBuffer::operator=(AudioBuffer &&) noexcept = default;

int AudioBuffer::LoadCacheRaw(void const *, std::size_t, AudioFormat const &) noexcept
{
    return -1;
}
int AudioBuffer::LoadFromFile(char const *) noexcept
{
    return -1;
}
int AudioBuffer::GenerateSineWave(double, double, double, AudioFormat const &) noexcept
{
    return -1;
}
std::uint64_t AudioBuffer::GetLengthFrames() const noexcept
{
    return 0;
}
double AudioBuffer::GetLengthSeconds() const noexcept
{
    return 0.0;
}
void AudioBuffer::Obliterate() noexcept {}
int AudioBuffer::Play(AudioMixer &, AudioTrack &) noexcept
{
    return -1;
}

AudioTrack::~AudioTrack() = default;
int AudioTrack::SetAudioBuffer(AudioBuffer *) noexcept
{
    return -1;
}
int AudioTrack::Play() noexcept
{
    return -1;
}
void AudioTrack::Pause() noexcept {}
void AudioTrack::Stop() noexcept {}
void AudioTrack::Resume() noexcept {}
void AudioTrack::SetVolume(float) noexcept {}
float AudioTrack::GetVolume() const noexcept
{
    return 0.f;
}
void AudioTrack::SetPitch(float) noexcept {}
float AudioTrack::GetPitch() const noexcept
{
    return 1.f;
}
void AudioTrack::SetLoop(bool) noexcept {}
bool AudioTrack::GetLoop() const noexcept
{
    return false;
}
bool AudioTrack::IsPlaying() const noexcept
{
    return false;
}
bool AudioTrack::IsPaused() const noexcept
{
    return false;
}

void AudioTrackPause(AudioTrack &) noexcept {}
void AudioTrackStop(AudioTrack &) noexcept {}
void AudioTrackResume(AudioTrack &) noexcept {}
void AudioTrackSetVolume(AudioTrack &, float) noexcept {}
float AudioTrackGetVolume(AudioTrack const &) noexcept
{
    return 0.f;
}
void AudioTrackSetPitch(AudioTrack &, float) noexcept {}
float AudioTrackGetPitch(AudioTrack const &) noexcept
{
    return 1.f;
}
void AudioTrackSetLoop(AudioTrack &, bool) noexcept {}
bool AudioTrackGetLoop(AudioTrack const &) noexcept
{
    return false;
}

AudioMixer::~AudioMixer() = default;
char const * AudioMixer::GetNameTag() const noexcept
{
    return "";
}
void AudioMixer::SetNameTag(char const *) noexcept {}
void AudioMixer::SetMixPipelineCallback(AudioMixPipelineCallback, void *) noexcept {}
void AudioMixer::ClearMixPipelineCallback() noexcept {}
AudioFormat AudioMixer::GetFormat() const noexcept
{
    return {};
}
void * AudioMixer::Lock() noexcept
{
    return nullptr;
}
void AudioMixer::Unlock() noexcept {}
AudioTrack * AudioMixer::AddTrack() noexcept
{
    return nullptr;
}
void AudioMixer::RemoveTrack(AudioTrack *) noexcept {}
int AudioMixer::GetTrackCount() const noexcept
{
    return 0;
}
AudioTrack * AudioMixer::GetTrack(int) noexcept
{
    return nullptr;
}
AudioTrack const * AudioMixer::GetTrack(int) const noexcept
{
    return nullptr;
}
int AudioMixer::Play() noexcept
{
    return -1;
}
void AudioMixer::Stop() noexcept {}

int InitializeAudioSubsystem(unsigned int) noexcept
{
    return -1;
}
void ShutdownAudioSubsystem() noexcept {}
void QuitAudioLibrary() noexcept {}
int GetAudioDecoderCount() noexcept
{
    return 0;
}
int GetAudioDecoderName(int, char *, std::size_t) noexcept
{
    return -1;
}
AudioMixer * CreateAudioMixerDevice(char const *, AudioFormat const *) noexcept
{
    return nullptr;
}
AudioMixer * CreateAudioMixerBuffer(char const *, AudioFormat const *, void *, std::size_t) noexcept
{
    return nullptr;
}
void ObliterateAudioMixer(AudioMixer *) noexcept {}
int GetAudioMixerFormat(AudioMixer const *, AudioFormat *) noexcept
{
    return -1;
}
void * LockAudioMixer(AudioMixer *) noexcept
{
    return nullptr;
}
void UnlockAudioMixer(AudioMixer *) noexcept {}
int PlayAudioMixerByName(char const *) noexcept
{
    return -1;
}

struct AudioSource::Impl {};
AudioSource::AudioSource(AudioMixer &) noexcept : impl_(nullptr) {}
AudioSource::~AudioSource() = default;
AudioMixer * AudioSource::GetMixer() noexcept
{
    return nullptr;
}
AudioMixer const * AudioSource::GetMixer() const noexcept
{
    return nullptr;
}
AudioTrack * AudioSource::AddTrack() noexcept
{
    return nullptr;
}
int AudioSource::GetTrackCount() const noexcept
{
    return 0;
}
AudioTrack * AudioSource::GetTrack(int) noexcept
{
    return nullptr;
}

struct AudioTrackGroup::Impl {};
AudioTrackGroup::AudioTrackGroup(AudioMixer &) noexcept : impl_(nullptr) {}
AudioTrackGroup::~AudioTrackGroup() = default;
void AudioTrackGroup::AddTrack(AudioTrack *) noexcept {}
void AudioTrackGroup::RemoveTrack(AudioTrack *) noexcept {}
void AudioTrackGroup::SetGroupVolume(float) noexcept {}
float AudioTrackGroup::GetGroupVolume() const noexcept
{
    return 0.f;
}
void AudioTrackGroup::SetMuted(bool) noexcept {}
bool AudioTrackGroup::IsMuted() const noexcept
{
    return false;
}

} // namespace HonkordGL::Audio

#endif