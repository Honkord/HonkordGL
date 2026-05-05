# Audio backends

## Implementations

| Platform | TU | Notes |
|----------|-----|------|
| Linux | `AudioAlsa.cpp`, optionally `PipeWire.cpp` | `HONKORDGL_AUDIO_USE_PIPEWIRE` selects PipeWire path |
| Windows | `PipeWire.cpp` | Stub/no-op style paths when Linux PipeWire block inactive |
| Apple | `CoreAudio.cpp` | |
| Disabled | All above compile stub branches when `HONKORDGL_AUDIO_DISABLED` | From `HONKORDGL_ENABLE_AUDIO=OFF` |

## CMake options

`HONKORDGL_ENABLE_AUDIO`, `HONKORDGL_ENABLE_ALSA`, `HONKORDGL_ENABLE_PIPEWIRE` (Linux).

## Capability flag

`HONKORDGL_HAVE_AUDIO` / `IsAudioBackendAvailable()`.
