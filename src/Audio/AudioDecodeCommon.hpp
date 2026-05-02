#ifndef HONKORDGL_AUDIO_DECODE_COMMON_HPP
#define HONKORDGL_AUDIO_DECODE_COMMON_HPP

#include <HonkordGL/PlatformAdapter.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace HonkordGL::Audio::detail {

template <class BufferDataT>
    bool ParseWavFileCommon(char const * path, BufferDataT & out) noexcept
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
        if (audioFormat != 1 && audioFormat != 3)
            return false;

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

bool DecodeOggVorbisToF32(char const * path, std::vector<float> & samples, int & sampleRateHz, int & channelCount) noexcept;

template <class BufferDataT>
    bool ParseOggVorbisFileCommon(char const * path, BufferDataT & out) noexcept
    {
        int sr = 0;
        int ch = 0;
        std::vector<float> pcm{};
        if (!DecodeOggVorbisToF32(path, pcm, sr, ch))
            return false;
        out.sampleRateHz = sr;
        out.channelCount = ch;
        out.interleaved = std::move(pcm);
        return true;
    }

template <class BufferDataT>
    bool DecodeAudioFileCommon(char const * path, BufferDataT & out) noexcept
    {
        if (ParseWavFileCommon(path, out))
            return true;
        return ParseOggVorbisFileCommon(path, out);
    }

} // namespace HonkordGL::Audio::detail

#endif
