#include "AudioDecodeCommon.hpp"

#define STB_VORBIS_HEADER_ONLY
#include <HonkordGL/third_party/stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

#define STB_VORBIS_IMPLEMENTATION
#include <HonkordGL/third_party/stb_vorbis.c>

namespace HonkordGL::Audio::detail {

bool DecodeOggVorbisToF32(char const * path, std::vector<float> & samples, int & sampleRateHz, int & channelCount) noexcept
{
    sampleRateHz = 0;
    channelCount = 0;
    if (!path || !path[0])
        return false;

    int channels = 0;
    int sample_rate = 0;
    short * output = nullptr;
    int sample_count = stb_vorbis_decode_filename(path, &channels, &sample_rate, &output);
    if (sample_count <= 0 || channels <= 0 || sample_rate <= 0 || !output)
        return false;

    std::size_t const total = static_cast<std::size_t>(sample_count) * static_cast<std::size_t>(channels);
    samples.resize(total);
    for (std::size_t i = 0; i < total; ++i)
        samples[i] = static_cast<float>(output[i]) / 32768.0f;

    free(output);
    sampleRateHz = sample_rate;
    channelCount = channels;
    return true;
}

} // namespace HonkordGL::Audio::detail