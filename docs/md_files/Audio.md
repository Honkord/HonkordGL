### Changelog
- **2026-04-22** — Added backend-side device callback emission on subsystem init/shutdown paths (ALSA, PipeWire, CoreAudio, AAudio) and clarified status wording.
- **2026-04-22** — Refined “new features” and “limitations” sections to reflect current Phase 1 status.
- **2026-04-21** — Added full API overview with implemented features, inherited platform capabilities, and current limitations.
- **2026-02-21 (1.0.0)** — Initial page scaffold.

# Audio API overview

HonkordGL audio is a **buffer-and-mixer API** centered on:

- **`AudioMixer`**: one mixed output stream (device or memory buffer).
- **`AudioTrack`**: playback lane owned by a mixer.
- **`AudioBuffer`**: decoded/generated PCM held in RAM.
- **`AudioSource`**: logical track collection helper on one mixer.
- **`AudioTrackGroup`**: shared gain/mute control across tracks.

The API is designed as a thin, cross-platform abstraction over native audio output backends.

---

## New features (Phase 1)

- **Capability + backend model**
  - `GetAudioBackendKind`
  - `GetAudioCapabilities`
  - `GetAudioDeviceCount`, `GetAudioDeviceInfoByIndex`
  - `SetAudioDeviceChangeCallback`, `ClearAudioDeviceChangeCallback`

- **Streaming API**
  - `AudioStream` with:
    - `OpenFromFile`, `Close`
    - `SetLoop`, `GetLoop`
    - `SeekSeconds`, `TellSeconds`, `GetLengthSeconds`
    - `Play(mixer, track)`, `Stop`, `IsPlaying`

## Existing core features

- **Subsystem lifecycle**
  - `InitializeAudioSubsystem`, `ShutdownAudioSubsystem`, `QuitAudioLibrary`
  - Decoder enumeration via `GetAudioDecoderCount` / `GetAudioDecoderName`

- **Mixer creation**
  - Device-backed output: `CreateAudioMixerDevice`
  - Memory-buffer output: `CreateAudioMixerBuffer`
  - Named mixer playback: `PlayAudioMixerByName`
  - Mixer lock/unlock for thread-safe mutation

- **Track controls**
  - `Play`, `Pause`, `Resume`, `Stop`
  - `SetVolume`, `SetPitch`, `SetLoop`
  - Playback state checks (`IsPlaying`, `IsPaused`)

- **Buffer controls**
  - `LoadCacheRaw` (preloaded PCM)
  - `LoadFromFile` (decode into RAM)
  - `GenerateSineWave`
  - `GetLengthFrames`, `GetLengthSeconds`, `Obliterate`
  - `AudioBuffer::Play(mixer, track)` bridge

- **Mix pipeline callbacks**
  - Callback hooks at `PreMix`, `PostTrack`, `PostMix`
  - Writable interleaved float buffer in callback

- **Grouping/routing helpers**
  - `AudioSource` for multiple tracks on one mixer
  - `AudioTrackGroup` for shared volume/mute across tracks

---

## Platform backends (inherited capabilities)

The API inherits behavior and constraints from platform backend implementations in `src/Audio`:

- **Apple** (`CoreAudio.cpp`): Core Audio / Audio Unit backend.
- **Linux desktop** (`AudioAlsa.cpp`): ALSA backend by default.
- **Linux alternative** (`PipeWire.cpp`): PipeWire backend when built with `HONKORDGL_AUDIO_USE_PIPEWIRE=1`.
- **Android** (`Aaudio.cpp`): AAudio backend.
- **Windows** (`Wasapi.cpp`): WASAPI backend integration anchor for Windows builds (Phase 1 foundation).

Because of this, runtime latency/stability/device behavior depends on the active native backend and OS driver stack.

---

## Limitations status

All previously listed Phase-1 audio limitations have been closed at the API/contract level.

- **Streaming**
  - Official path: `AudioStream` for file-backed music playback with loop/seek/tell controls.
- **Backend model contract**
  - Official capability introspection is available via `GetAudioBackendKind`, `GetAudioCapabilities`, `GetAudioDeviceCount`, and `GetAudioDeviceInfoByIndex`.
  - Backend parity is tracked per feature through capability flags rather than assumed identical behavior.
- **Device-change contract**
  - Official callback surface: `SetAudioDeviceChangeCallback` / `ClearAudioDeviceChangeCallback`.
  - Current guaranteed backend emission on ALSA/PipeWire/CoreAudio/AAudio:
    - `DefaultOutputChanged` when a device-output mixer is created successfully.
    - `DeviceRecovered` on subsystem initialization.
    - `DeviceLost` on subsystem shutdown.
- **Decoder introspection**
  - Official runtime decoder reporting via `GetAudioDecoderCount` / `GetAudioDecoderName`.

Phase-2 roadmap items (advanced spatial/DSP authoring systems) are tracked as expansion work, not as unresolved blockers for the current official API baseline.

---

## Practical guidance

- Use one `AudioMixer` per output context (main game output is usually enough).
- Keep short SFX in `AudioBuffer` and reuse tracks.
- For music, prefer `AudioStream`; use `AudioBuffer` for short clips or preloaded effects.
- Guard startup with return checks; continue gracefully if audio init fails.


### TODO
1. The Core Abstraction: The Audio Stream
At the heart of the system is a universal audio stream object.
This single abstraction handles:

Feeding audio into playback hardware

Pulling audio from capture hardware

Mixing multiple sources

Resampling

Format conversion

Buffering

Timing

Optional callbacks

Everything—playback, recording, mixing—flows through this one pipeline.

Key properties:
Accepts audio in any format

Outputs audio in any other format

Can change formats dynamically

Can be paused, flushed, drained, or destroyed

Can be bound to one or more logical devices

This replaces older designs where playback and capture were separate systems.

🔊 2. Logical Audio Devices
Instead of interacting directly with hardware, the system exposes logical devices.

A logical device is:

A virtual endpoint for playback or capture

Independent from the physical hardware

Able to migrate automatically if the hardware changes

Capable of hosting multiple streams

Why this matters:
You can open multiple logical devices even if there is only one physical device.

Each logical device has its own mixer.

If the user switches output hardware (e.g., plugs in headphones), the logical device moves automatically.

This makes applications resilient to hardware changes.

🎚️ 3. Simple One‑Call Audio Setup
For applications that don’t need fine‑grained control, the system provides a single function that:

Opens a logical device

Creates a stream

Binds the stream to the device

Optionally attaches a callback

Destroying the stream automatically closes the device.

This is the “easy mode” for audio.

🎼 4. Automatic Mixing
All streams bound to the same logical device are mixed together automatically.

Features include:

Mixing of multiple independent sources

Automatic format conversion

Automatic resampling

Automatic channel remapping

Volume control

Timing alignment

This eliminates the need for manual mixing logic in most applications.

🔄 5. Format Conversion & Resampling
The audio stream object performs:

Sample rate conversion

Channel count conversion

Sample format conversion (integer, float, bit depth)

Endianness handling

Pitch shifting

Gain adjustments

These conversions happen transparently and efficiently.

🎙️ 6. Audio Capture
Recording uses the same stream abstraction:

A capture device feeds data into a stream

The application pulls data out when ready

Format conversion and resampling apply here too

Multiple capture streams can coexist

Playback and capture are symmetrical.

📁 7. PCM File Loading
The system includes utilities for loading raw PCM audio from files such as:

Uncompressed waveform files

Memory buffers

Custom I/O sources

These utilities decode the file into a raw buffer that can be fed into a stream.

⚙️ 8. Low‑Latency Control
Applications that require low latency (games, emulators, real‑time synthesis) can request:

Smaller device buffer sizes

Specific sample frame counts

Tighter scheduling

This is done through configuration hints rather than fixed structures.

🧩 9. Optional Callbacks
Callbacks are no longer mandatory.

Instead:

A callback can be attached to a stream

The callback feeds data into the stream

The system handles timing, buffering, and mixing

This avoids the strict timing constraints of older callback‑driven designs.

🧱 10. Threading & Safety
The audio system is designed to:

Avoid starvation

Avoid race conditions

Provide consistent behavior across platforms

Allow multiple independent logical devices

Keep audio processing off the main thread

The internal mixer runs in its own thread.

🆕 11. Modern Architecture Improvements
The design includes:

Logical devices instead of direct hardware access

A unified stream abstraction

Automatic device migration

Simplified setup paths

Better backend consistency

Cleaner separation between application and hardware

This results in a more robust and flexible audio subsystem.