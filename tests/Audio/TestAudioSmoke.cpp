/**
 * HonkordGL Audio smoke test:
 * - backend/capability query
 * - decoder enumeration
 * - device callback registration
 * - stream open/loop controls
 */

#include <HonkordGL/Audio.h>

#include <cstdio>

using namespace HonkordGL::Audio;

namespace {
void OnDeviceEvent(AudioDeviceChangeEvent, char const *, void *) {}
}

int main()
{
    if (InitializeAudioSubsystem(0u) != 0)
        return 1;

    const AudioBackendKind backend = GetAudioBackendKind();
    const AudioCapabilities caps = GetAudioCapabilities();
    const int decCount = GetAudioDecoderCount();

    std::printf("backend=%d streaming=%d deviceEvents=%d decoders=%d\n",
        static_cast<int>(backend),
        caps.supportsStreaming ? 1 : 0,
        caps.supportsDeviceChangeEvents ? 1 : 0,
        decCount);

    for (int i = 0; i < decCount; ++i) {
        char name[96]{};
        if (GetAudioDecoderName(i, name, sizeof(name)) == 0)
            std::printf("decoder[%d]=%s\n", i, name);
    }

    SetAudioDeviceChangeCallback(&OnDeviceEvent, nullptr);
    ClearAudioDeviceChangeCallback();

    AudioStream stream{};
    (void)stream.OpenFromFile("works/Tetris/Party Sector.ogg");
    stream.SetLoop(true);
    (void)stream.SeekSeconds(0.0);
    (void)stream.TellSeconds();

    ShutdownAudioSubsystem();
    return 0;
}