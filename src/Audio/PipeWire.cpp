/**
 * HonkordGL — Audio API implementation (PipeWire on Linux when HONKORDGL_AUDIO_USE_PIPEWIRE; stubs otherwise)
 * Copyright (C) 2026 Honkord
 *
 * Build: -DHONKORDGL_AUDIO_USE_PIPEWIRE=1, link libpipewire-0.3 (and omit ALSA Audio API TU or rely on AudioAlsa.cpp stub).
 */

#include <HonkordGL/Audio.h>
#include <HonkordGL/Config.h>
#include "AudioDecodeCommon.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if !HONKORDGL_AUDIO_DISABLED && HONKORDGL_PLATFORM_LINUX && HONKORDGL_AUDIO_USE_PIPEWIRE

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

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

    pw_thread_loop * threadLoop{};
    pw_context * pwContext{};
    pw_core * pwCore{};
    pw_stream * pwStream{};
    struct spa_hook streamHook{};
    std::atomic<bool> deviceOutputActive{false};

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
        if (!deviceOutputActive.load(std::memory_order_acquire)) {
            int const ch = format.channelCount > 0 ? format.channelCount : 2;
            std::size_t const n = static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(ch);
            std::memset(out, 0, n * sizeof(float));
            return;
        }
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

static void onPwStreamProcess(void * userdata) noexcept
{
    auto * m = static_cast<MixerImpl *>(userdata);
    if (!m || m->isBufferMode || !m->pwStream)
        return;

    struct pw_buffer * b = pw_stream_dequeue_buffer(m->pwStream);
    if (!b || !b->buffer)
        return;

    struct spa_buffer * buf = b->buffer;
    if (buf->n_datas < 1 || !buf->datas[0].data)
        return;

    struct spa_data * d = &buf->datas[0];
    float * dst = static_cast<float *>(d->data);
    if (!dst || !d->chunk)
        return;

    uint32_t const stride = static_cast<uint32_t>(sizeof(float))
        * static_cast<uint32_t>(m->format.channelCount > 0 ? m->format.channelCount : 2);
    uint32_t byteCount = d->chunk->size;
    if (byteCount == 0)
        byteCount = d->maxsize;
    if (stride == 0 || byteCount < stride)
        return;

    uint32_t const nFrames = byteCount / stride;
    if (nFrames == 0)
        return;

    m->renderToOutputFloats(dst, nFrames);

    d->chunk->offset = 0;
    d->chunk->stride = stride;
    d->chunk->size = nFrames * stride;

    pw_stream_queue_buffer(m->pwStream, b);
}

static const struct pw_stream_events s_pwStreamEvents = {
    PW_VERSION_STREAM_EVENTS,
    .process = onPwStreamProcess,
};

static void destroyPipeWireGraph(MixerImpl & m) noexcept
{
    if (m.threadLoop)
        pw_thread_loop_lock(m.threadLoop);

    if (m.pwStream) {
        spa_hook_remove(&m.streamHook);
        pw_stream_destroy(m.pwStream);
        m.pwStream = nullptr;
    }
    if (m.pwCore) {
        pw_core_disconnect(m.pwCore);
        m.pwCore = nullptr;
    }
    if (m.pwContext) {
        pw_context_destroy(m.pwContext);
        m.pwContext = nullptr;
    }

    if (m.threadLoop) {
        pw_thread_loop_unlock(m.threadLoop);
        pw_thread_loop_stop(m.threadLoop);
        pw_thread_loop_destroy(m.threadLoop);
        m.threadLoop = nullptr;
    }
}

bool openPipeWire(MixerImpl & out, AudioFormat const & req, unsigned subsystemFlags) noexcept
{
    int const rate = req.sampleRateHz > 0 ? req.sampleRateHz : 48000;
    int const ch = req.channelCount > 0 ? req.channelCount : 2;

    out.threadLoop = pw_thread_loop_new("honkordgl-audio", nullptr);
    if (!out.threadLoop)
        return false;

    if (pw_thread_loop_start(out.threadLoop) < 0) {
        pw_thread_loop_destroy(out.threadLoop);
        out.threadLoop = nullptr;
        return false;
    }

    pw_thread_loop_lock(out.threadLoop);

    struct pw_loop * loop = pw_thread_loop_get_loop(out.threadLoop);
    out.pwContext = pw_context_new(loop, nullptr, 0);
    if (!out.pwContext) {
        pw_thread_loop_unlock(out.threadLoop);
        destroyPipeWireGraph(out);
        return false;
    }

    out.pwCore = pw_context_connect(out.pwContext, nullptr, 0);
    if (!out.pwCore) {
        pw_thread_loop_unlock(out.threadLoop);
        destroyPipeWireGraph(out);
        return false;
    }

    struct pw_properties * props = pw_properties_new(
        PW_KEY_MEDIA_TYPE,
        "Audio",
        PW_KEY_MEDIA_CATEGORY,
        "Playback",
        PW_KEY_MEDIA_ROLE,
        "Game",
        PW_KEY_APP_NAME,
        "HonkordGL",
        nullptr);

    out.pwStream = pw_stream_new(out.pwCore, "HonkordGL playback", props);
    if (!out.pwStream) {
        pw_thread_loop_unlock(out.threadLoop);
        destroyPipeWireGraph(out);
        return false;
    }

    pw_stream_add_listener(out.pwStream, &out.streamHook, &s_pwStreamEvents, &out);

    uint8_t podBuf[4096]{};
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));

    struct spa_audio_info_raw raw{};
    raw.format = SPA_AUDIO_FORMAT_F32;
    raw.flags = 0;
    raw.rate = static_cast<uint32_t>(rate);
    raw.channels = static_cast<uint32_t>(ch);

    const struct spa_pod * params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw);

    uint32_t streamFlags = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS;
    if ((subsystemFlags & static_cast<unsigned>(AudioSubsystemInit::LowLatency)) != 0u)
        streamFlags |= PW_STREAM_FLAG_RT_PROCESS;

    int const conn = pw_stream_connect(
        out.pwStream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(streamFlags),
        params,
        1u);

    if (conn < 0) {
        spa_hook_remove(&out.streamHook);
        pw_stream_destroy(out.pwStream);
        out.pwStream = nullptr;
        pw_thread_loop_unlock(out.threadLoop);
        destroyPipeWireGraph(out);
        return false;
    }

    pw_thread_loop_unlock(out.threadLoop);

    out.format.sampleRateHz = rate;
    out.format.channelCount = ch;
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
    if (m->pwStream) {
        m->deviceOutputActive.store(true, std::memory_order_release);
    }
    return 0;
}

void AudioMixer::Stop() noexcept
{
    detail::MixerImpl * m = detail::mixerImpl(this);
    if (!m)
        return;
    m->stopWorker();
    if (m->pwStream)
        m->deviceOutputActive.store(false, std::memory_order_release);
}

int InitializeAudioSubsystem(unsigned int flags) noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    detail::g_subsysInitFlags |= flags;
    ++detail::g_initCount;
    if (detail::g_initCount == 1)
        pw_init(nullptr, nullptr);
    AudioEmitDeviceChange(AudioDeviceChangeEvent::DeviceRecovered, "default");
    return 0;
}

void ShutdownAudioSubsystem() noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    if (detail::g_initCount > 0) {
        --detail::g_initCount;
        if (detail::g_initCount == 0)
            pw_deinit();
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
    detail::g_subsysInitFlags = 0;
    if (detail::g_initCount > 0) {
        detail::g_initCount = 0;
        pw_deinit();
    }
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
    if (!detail::openPipeWire(*mimpl, req, detail::g_subsysInitFlags)) {
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
    detail::destroyPipeWireGraph(*mimpl);

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
namespace detail
{
constexpr int kPeriodFrames = 512;
constexpr unsigned kDecoderCount = 2;
static char const kDecoderWav[] = "WAV (PCM)";
static char const kDecoderOgg[] = "OGG (Vorbis)";
static std::mutex g_subsysMutex;
static int g_initCount = 0;
static std::mutex g_mixerMapMutex;
static std::unordered_map<std::string, AudioMixer *> g_mixersByName;

struct BufferData {
    std::vector<float> interleaved;
    int sampleRateHz{48000};
    int channelCount{2};
};

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
    mutable std::mutex mutex;
    std::vector<AudioTrack *> trackOwners;
    std::vector<AudioTrack *> trackList;
    AudioMixPipelineCallback mixCb{};
    void * mixCbUser{};
    std::atomic<bool> workerStop{true};
    std::thread worker;
};

static bool ParseWavFile(char const * path, BufferData & out) noexcept
{
    FILE * f = std::fopen(path, "rb");
    if (!f) return false;
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
        if (std::fread(ch, 1, 8, f) != 8u) break;
        std::uint32_t chunkSize = static_cast<std::uint32_t>(ch[4]) | (static_cast<std::uint32_t>(ch[5]) << 8u)
            | (static_cast<std::uint32_t>(ch[6]) << 16u) | (static_cast<std::uint32_t>(ch[7]) << 24u);
        if (std::memcmp(ch, "fmt ", 4) == 0) {
            std::vector<unsigned char> fmt(chunkSize);
            if (std::fread(fmt.data(), 1, chunkSize, f) != chunkSize || chunkSize < 16u) {
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
    if (!gotFmt || !gotData || numChannels == 0 || sampleRate == 0) return false;
    if (audioFormat != 1 && audioFormat != 3) return false;
    out.sampleRateHz = static_cast<int>(sampleRate);
    out.channelCount = static_cast<int>(numChannels);
    if (audioFormat == 1 && bitsPerSample == 16) {
        std::size_t const nSamp = rawPcm.size() / 2u;
        out.interleaved.resize(nSamp);
        auto const * p = reinterpret_cast<std::int16_t const *>(rawPcm.data());
        for (std::size_t i = 0; i < nSamp; ++i) out.interleaved[i] = static_cast<float>(p[i]) / 32768.f;
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

static void MixFrames(MixerImpl & m, std::uint32_t frameCount, std::vector<float> & mix, std::vector<float> & trackScratch) noexcept
{
    int const ch = m.format.channelCount > 0 ? m.format.channelCount : 2;
    std::size_t const sampleCount = frameCount * static_cast<std::size_t>(ch);
    mix.assign(sampleCount, 0.f);
    trackScratch.assign(sampleCount, 0.f);
    AudioMixCallbackContext ctx{};
    std::lock_guard<std::mutex> lock(m.mutex);
    ctx.user = m.mixCbUser;
    if (m.mixCb) {
        ctx.trackIndex = -1;
        (void)m.mixCb(AudioMixCallbackStage::PreMix, mix.data(), frameCount, ch, &ctx);
    }
    for (std::size_t ti = 0; ti < m.trackList.size(); ++ti) {
        AudioTrack * const tr = m.trackList[ti];
        TrackImpl * tp = trackImpl(tr);
        if (!tp) continue;
        auto & t = *tp;
        if (!t.playing.load(std::memory_order_acquire) || t.paused.load(std::memory_order_acquire)) continue;
        BufferData * bd = t.bufferData;
        if (!bd || bd->interleaved.empty() || bd->channelCount != ch) continue;
        std::fill(trackScratch.begin(), trackScratch.end(), 0.f);
        std::uint64_t const totalFrames = bd->interleaved.size() / static_cast<std::size_t>(bd->channelCount);
        float const vol = t.volume * (t.group && !t.group->muted ? t.group->groupVolume : 1.f);
        for (std::size_t f = 0; f < frameCount; ++f) {
            std::uint64_t i0 = static_cast<std::uint64_t>(std::floor(t.readPos));
            if (i0 >= totalFrames) {
                if (t.loop && totalFrames > 0) {
                    t.readPos = 0.0;
                    i0 = 0;
                } else {
                    t.playing.store(false, std::memory_order_release);
                    break;
                }
            }
            for (int c = 0; c < ch; ++c) {
                std::size_t const dst = f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c);
                std::size_t const src = static_cast<std::size_t>(i0) * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c);
                trackScratch[dst] = bd->interleaved[src] * vol;
            }
            t.readPos += std::max(0.01, static_cast<double>(t.pitch));
        }
        for (std::size_t i = 0; i < sampleCount; ++i) mix[i] += trackScratch[i];
        if (m.mixCb) {
            ctx.trackIndex = static_cast<int>(ti);
            (void)m.mixCb(AudioMixCallbackStage::PostTrack, mix.data(), frameCount, ch, &ctx);
        }
    }
    for (float & s : mix) s = std::clamp(s, -1.f, 1.f);
    if (m.mixCb) {
        ctx.trackIndex = -1;
        (void)m.mixCb(AudioMixCallbackStage::PostMix, mix.data(), frameCount, ch, &ctx);
    }
}

static void WorkerLoop(MixerImpl * m) noexcept
{
    std::vector<float> mix;
    std::vector<float> scratch;
    int const sr = (m->format.sampleRateHz > 0 ? m->format.sampleRateHz : 48000);
    while (!m->workerStop.load(std::memory_order_acquire)) {
        std::uint32_t const frames = static_cast<std::uint32_t>(kPeriodFrames);
        MixFrames(*m, frames, mix, scratch);
        if (m->isBufferMode && m->userBuffer && m->userBufferBytes >= sizeof(float)) {
            auto * dst = reinterpret_cast<float *>(m->userBuffer);
            std::size_t const cap = m->userBufferBytes / sizeof(float);
            std::size_t w = m->bufferWriteByte.load(std::memory_order_acquire) / sizeof(float);
            for (float v : mix) {
                if (cap == 0) break;
                dst[w] = v;
                w = (w + 1u) % cap;
            }
            m->bufferWriteByte.store(w * sizeof(float), std::memory_order_release);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds((1000 * static_cast<int>(frames)) / std::max(1, sr)));
    }
}

MixerImpl * mixerImpl(AudioMixer const * mixer) noexcept
{
    return mixer ? reinterpret_cast<MixerImpl *>(mixer->impl_) : nullptr;
}
TrackImpl * trackImpl(AudioTrack const * track) noexcept
{
    return track ? reinterpret_cast<TrackImpl *>(track->impl_) : nullptr;
}
} // namespace detail

struct AudioBuffer::Impl {
    detail::BufferData data{};
};

AudioBuffer::~AudioBuffer()
{
    delete impl_;
    impl_ = nullptr;
}
AudioBuffer::AudioBuffer(AudioBuffer && other) noexcept : impl_(other.impl_)
{
    other.impl_ = nullptr;
}
AudioBuffer & AudioBuffer::operator=(AudioBuffer && other) noexcept
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

int AudioBuffer::LoadCacheRaw(void const * data, std::size_t dataBytes, AudioFormat const & hint) noexcept
{
    if (!data || dataBytes < sizeof(float) || hint.channelCount <= 0 || hint.sampleRateHz <= 0) return -1;
    if (hint.sampleFormat != AudioSampleFormat::Float32Interleaved) return -1;
    if (!impl_) impl_ = new Impl{};
    std::size_t count = dataBytes / sizeof(float);
    impl_->data.interleaved.resize(count);
    std::memcpy(impl_->data.interleaved.data(), data, count * sizeof(float));
    impl_->data.sampleRateHz = hint.sampleRateHz;
    impl_->data.channelCount = hint.channelCount;
    return 0;
}
int AudioBuffer::LoadFromFile(char const * path) noexcept
{
    if (!path || !path[0]) return -1;
    if (!impl_) impl_ = new Impl{};
    return detail::DecodeAudioFileCommon(path, impl_->data) ? 0 : -1;
}
int AudioBuffer::GenerateSineWave(double frequencyHz, double amplitude, double durationSeconds, AudioFormat const & format) noexcept
{
    if (frequencyHz <= 0.0 || durationSeconds <= 0.0 || format.sampleRateHz <= 0 || format.channelCount <= 0) return -1;
    if (!impl_) impl_ = new Impl{};
    std::size_t frames = static_cast<std::size_t>(durationSeconds * static_cast<double>(format.sampleRateHz));
    impl_->data.interleaved.assign(frames * static_cast<std::size_t>(format.channelCount), 0.f);
    double const step = 2.0 * 3.14159265358979323846 * frequencyHz / static_cast<double>(format.sampleRateHz);
    for (std::size_t f = 0; f < frames; ++f) {
        float v = static_cast<float>(std::sin(step * static_cast<double>(f)) * std::clamp(amplitude, 0.0, 1.0));
        for (int c = 0; c < format.channelCount; ++c) impl_->data.interleaved[f * static_cast<std::size_t>(format.channelCount) + static_cast<std::size_t>(c)] = v;
    }
    impl_->data.sampleRateHz = format.sampleRateHz;
    impl_->data.channelCount = format.channelCount;
    return 0;
}
std::uint64_t AudioBuffer::GetLengthFrames() const noexcept
{
    if (!impl_ || impl_->data.channelCount <= 0) return 0;
    return static_cast<std::uint64_t>(impl_->data.interleaved.size() / static_cast<std::size_t>(impl_->data.channelCount));
}
double AudioBuffer::GetLengthSeconds() const noexcept
{
    if (!impl_ || impl_->data.sampleRateHz <= 0) return 0.0;
    return static_cast<double>(GetLengthFrames()) / static_cast<double>(impl_->data.sampleRateHz);
}
void AudioBuffer::Obliterate() noexcept
{
    if (impl_) impl_->data.interleaved.clear();
}
int AudioBuffer::Play(AudioMixer &, AudioTrack &) noexcept
{
    return 0;
}

struct AudioTrack::Impl {
    detail::TrackImpl state{};
};

AudioTrack::~AudioTrack()
{
    delete impl_;
    impl_ = nullptr;
}
int AudioTrack::SetAudioBuffer(AudioBuffer * buffer) noexcept
{
    if (!impl_) return -1;
    auto & st = impl_->state;
    st.buffer = buffer;
    if (!buffer || !buffer->impl_) {
        st.bufferData = nullptr;
        return -1;
    }
    st.bufferData = &buffer->impl_->data;
    st.readPos = 0.0;
    return 0;
}
int AudioTrack::Play() noexcept
{
    if (!impl_ || !impl_->state.bufferData) return -1;
    impl_->state.playing.store(true, std::memory_order_release);
    impl_->state.paused.store(false, std::memory_order_release);
    return 0;
}
void AudioTrack::Pause() noexcept { if (impl_) impl_->state.paused.store(true, std::memory_order_release); }
void AudioTrack::Stop() noexcept
{
    if (!impl_) return;
    impl_->state.playing.store(false, std::memory_order_release);
    impl_->state.paused.store(false, std::memory_order_release);
    impl_->state.readPos = 0.0;
}
void AudioTrack::Resume() noexcept { if (impl_) impl_->state.paused.store(false, std::memory_order_release); }
void AudioTrack::SetVolume(float v) noexcept { if (impl_) impl_->state.volume = std::clamp(v, 0.f, 1.f); }
float AudioTrack::GetVolume() const noexcept
{
    return impl_ ? impl_->state.volume : 0.f;
}
void AudioTrack::SetPitch(float r) noexcept { if (impl_) impl_->state.pitch = std::max(0.01f, r); }
float AudioTrack::GetPitch() const noexcept
{
    return impl_ ? impl_->state.pitch : 1.f;
}
void AudioTrack::SetLoop(bool loop) noexcept { if (impl_) impl_->state.loop = loop; }
bool AudioTrack::GetLoop() const noexcept
{
    return impl_ && impl_->state.loop;
}
bool AudioTrack::IsPlaying() const noexcept
{
    return impl_ && impl_->state.playing.load(std::memory_order_acquire);
}
bool AudioTrack::IsPaused() const noexcept
{
    return impl_ && impl_->state.paused.load(std::memory_order_acquire);
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

struct AudioMixer::Impl {
    detail::MixerImpl state{};
};

AudioMixer::~AudioMixer()
{
    if (!impl_) return;
    Stop();
    auto * m = &impl_->state;
    {
        std::lock_guard<std::mutex> lock(m->mutex);
        for (AudioTrack * t : m->trackOwners) delete t;
        m->trackOwners.clear();
        m->trackList.clear();
    }
    delete impl_;
    impl_ = nullptr;
}
char const * AudioMixer::GetNameTag() const noexcept
{
    return impl_ ? impl_->state.nameTag.c_str() : "";
}
void AudioMixer::SetNameTag(char const * nameTag) noexcept
{
    if (!impl_) return;
    std::string const tag = (nameTag ? nameTag : "");
    std::lock_guard<std::mutex> mapLock(detail::g_mixerMapMutex);
    if (!impl_->state.nameTag.empty()) detail::g_mixersByName.erase(impl_->state.nameTag);
    impl_->state.nameTag = tag;
    if (!tag.empty()) detail::g_mixersByName[tag] = this;
}
void AudioMixer::SetMixPipelineCallback(AudioMixPipelineCallback fn, void * user) noexcept
{
    if (!impl_) return;
    std::lock_guard<std::mutex> lock(impl_->state.mutex);
    impl_->state.mixCb = fn;
    impl_->state.mixCbUser = user;
}
void AudioMixer::ClearMixPipelineCallback() noexcept { SetMixPipelineCallback(nullptr, nullptr); }
AudioFormat AudioMixer::GetFormat() const noexcept
{
    return impl_ ? impl_->state.format : AudioFormat{};
}
void * AudioMixer::Lock() noexcept
{
    if (!impl_) return nullptr;
    impl_->state.mutex.lock();
    return &impl_->state.mutex;
}
void AudioMixer::Unlock() noexcept { if (impl_) impl_->state.mutex.unlock(); }
AudioTrack * AudioMixer::AddTrack() noexcept
{
    if (!impl_) return nullptr;
    auto * t = new AudioTrack{};
    t->impl_ = new AudioTrack::Impl{};
    t->impl_->state.mixer = &impl_->state;
    std::lock_guard<std::mutex> lock(impl_->state.mutex);
    impl_->state.trackOwners.push_back(t);
    impl_->state.trackList.push_back(t);
    return t;
}
void AudioMixer::RemoveTrack(AudioTrack * track) noexcept
{
    if (!impl_ || !track) return;
    std::lock_guard<std::mutex> lock(impl_->state.mutex);
    auto & list = impl_->state.trackList;
    list.erase(std::remove(list.begin(), list.end(), track), list.end());
    auto & owners = impl_->state.trackOwners;
    auto it = std::find(owners.begin(), owners.end(), track);
    if (it != owners.end()) {
        delete *it;
        owners.erase(it);
    }
}
int AudioMixer::GetTrackCount() const noexcept
{
    if (!impl_) return 0;
    std::lock_guard<std::mutex> lock(impl_->state.mutex);
    return static_cast<int>(impl_->state.trackList.size());
}
AudioTrack * AudioMixer::GetTrack(int index) noexcept
{
    if (!impl_ || index < 0) return nullptr;
    std::lock_guard<std::mutex> lock(impl_->state.mutex);
    if (static_cast<std::size_t>(index) >= impl_->state.trackList.size()) return nullptr;
    return impl_->state.trackList[static_cast<std::size_t>(index)];
}
AudioTrack const * AudioMixer::GetTrack(int index) const noexcept
{
    return const_cast<AudioMixer *>(this)->GetTrack(index);
}
int AudioMixer::Play() noexcept
{
    if (!impl_) return -1;
    if (!impl_->state.workerStop.load(std::memory_order_acquire)) return 0;
    impl_->state.workerStop.store(false, std::memory_order_release);
    impl_->state.worker = std::thread(detail::WorkerLoop, &impl_->state);
    return 0;
}
void AudioMixer::Stop() noexcept
{
    if (!impl_) return;
    impl_->state.workerStop.store(true, std::memory_order_release);
    if (impl_->state.worker.joinable()) impl_->state.worker.join();
}

int InitializeAudioSubsystem(unsigned int) noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    ++detail::g_initCount;
    return 0;
}
void ShutdownAudioSubsystem() noexcept
{
    std::lock_guard<std::mutex> lock(detail::g_subsysMutex);
    if (detail::g_initCount > 0) --detail::g_initCount;
}
void QuitAudioLibrary() noexcept { ShutdownAudioSubsystem(); }
int GetAudioDecoderCount() noexcept
{
    return detail::kDecoderCount;
}
int GetAudioDecoderName(int index, char * buf, std::size_t bufBytes) noexcept
{
    if (!buf || bufBytes == 0 || index < 0 || index >= static_cast<int>(detail::kDecoderCount))
        return -1;
    char const * const name = (index == 0) ? detail::kDecoderWav : detail::kDecoderOgg;
    std::strncpy(buf, name, bufBytes - 1u);
    buf[bufBytes - 1u] = '\0';
    return 0;
}
AudioMixer * CreateAudioMixerDevice(char const * nameTag, AudioFormat const * request) noexcept
{
    auto * m = new AudioMixer{};
    m->impl_ = new AudioMixer::Impl{};
    auto & st = m->impl_->state;
    st.self = m;
    st.nameTag = nameTag ? nameTag : "";
    st.format = request ? *request : AudioFormat{};
    st.isBufferMode = false;
    if (!st.nameTag.empty()) {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        detail::g_mixersByName[st.nameTag] = m;
    }
    return m;
}
AudioMixer * CreateAudioMixerBuffer(char const * nameTag, AudioFormat const * format, void * buffer, std::size_t bufferBytes) noexcept
{
    if (!buffer || bufferBytes == 0) return nullptr;
    auto * m = CreateAudioMixerDevice(nameTag, format);
    if (!m || !m->impl_) return m;
    m->impl_->state.isBufferMode = true;
    m->impl_->state.userBuffer = buffer;
    m->impl_->state.userBufferBytes = bufferBytes;
    m->impl_->state.bufferWriteByte.store(0, std::memory_order_release);
    return m;
}
void ObliterateAudioMixer(AudioMixer * mixer) noexcept
{
    if (!mixer) return;
    {
        std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
        if (mixer->impl_ && !mixer->impl_->state.nameTag.empty()) detail::g_mixersByName.erase(mixer->impl_->state.nameTag);
    }
    delete mixer;
}
int GetAudioMixerFormat(AudioMixer const * mixer, AudioFormat * out) noexcept
{
    auto * m = detail::mixerImpl(mixer);
    if (!m || !out) return -1;
    *out = m->format;
    return 0;
}
void * LockAudioMixer(AudioMixer * mixer) noexcept
{
    return mixer ? mixer->Lock() : nullptr;
}
void UnlockAudioMixer(AudioMixer * mixer) noexcept { if (mixer) mixer->Unlock(); }
int PlayAudioMixerByName(char const * nameTag) noexcept
{
    if (!nameTag || !nameTag[0]) return -1;
    std::lock_guard<std::mutex> lock(detail::g_mixerMapMutex);
    auto it = detail::g_mixersByName.find(nameTag);
    return (it != detail::g_mixersByName.end() && it->second) ? it->second->Play() : -1;
}

struct AudioSource::Impl {
    AudioMixer * mixer{};
    std::vector<AudioTrack *> tracks;
};
AudioSource::AudioSource(AudioMixer & mixer) noexcept : impl_(new Impl{})
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
    if (!impl_ || !impl_->mixer) return nullptr;
    AudioTrack * t = impl_->mixer->AddTrack();
    if (t) impl_->tracks.push_back(t);
    return t;
}
int AudioSource::GetTrackCount() const noexcept
{
    return impl_ ? static_cast<int>(impl_->tracks.size()) : 0;
}
AudioTrack * AudioSource::GetTrack(int index) noexcept
{
    if (!impl_ || index < 0 || static_cast<std::size_t>(index) >= impl_->tracks.size()) return nullptr;
    return impl_->tracks[static_cast<std::size_t>(index)];
}

struct AudioTrackGroup::Impl {
    detail::TrackGroupState state{};
    std::vector<AudioTrack *> tracks;
};
AudioTrackGroup::AudioTrackGroup(AudioMixer &) noexcept : impl_(new Impl{}) {}
AudioTrackGroup::~AudioTrackGroup()
{
    if (impl_) {
        for (AudioTrack * t : impl_->tracks) {
            if (auto * tp = detail::trackImpl(t)) tp->group = nullptr;
        }
        delete impl_;
        impl_ = nullptr;
    }
}
void AudioTrackGroup::AddTrack(AudioTrack * track) noexcept
{
    auto * tp = detail::trackImpl(track);
    if (!impl_ || !tp) return;
    tp->group = &impl_->state;
    impl_->tracks.push_back(track);
}
void AudioTrackGroup::RemoveTrack(AudioTrack * track) noexcept
{
    auto * tp = detail::trackImpl(track);
    if (!impl_ || !tp) return;
    tp->group = nullptr;
    auto & v = impl_->tracks;
    v.erase(std::remove(v.begin(), v.end(), track), v.end());
}
void AudioTrackGroup::SetGroupVolume(float v) noexcept { if (impl_) impl_->state.groupVolume = std::clamp(v, 0.f, 1.f); }
float AudioTrackGroup::GetGroupVolume() const noexcept
{
    return impl_ ? impl_->state.groupVolume : 0.f;
}
void AudioTrackGroup::SetMuted(bool muted) noexcept { if (impl_) impl_->state.muted = muted; }
bool AudioTrackGroup::IsMuted() const noexcept
{
    return impl_ && impl_->state.muted;
}

} // namespace HonkordGL::Audio

#endif