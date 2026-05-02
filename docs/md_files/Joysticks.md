### Changelog
- **2026-04-22** — Switched mapping persistence to SDL `gamecontrollerdb` text (no Honkord custom file format), added built-in action bindings (`SetJoystickActionBinding` / `IsJoystickActionPressed`), and documented Android key-event feeding (`FeedAndroidJoystickKeyEvent`) for accurate button snapshots.
- **2026-04-22** — SDL/community DB path: platform-filtered load/convert (`GameControllerDbPlatformFilter`, `DefaultGameControllerDbPlatformFilter`), `GetJoystickGameControllerDbGuid` (SDL 2.26–style USB GUID + name CRC), `ApplyLoadedGameControllerDbMappingsToConnectedJoysticks`, `GetGameControllerDbMappingCount`; hat `hN.mask` → `JoystickMapping` hat entries.
- **2026-04-22** — Replaced the limitations section to match current implementation: Linux evdev discovery, input snapshots, auto hotplug on `GetJoystickCount`, VID/PID canonical mapping, and capability flags.
- **2026-04-22** — Reworked limitations/usage notes to match implemented hotplug, mapping DB, and capability-depth behavior.
- **2026-04-22** — Added mapping DB API, hotplug callback polling API, and capability-depth query API.
- **2026-04-22** — Wired physical trigger rumble path on Windows XInput and retained virtual output paths.
- **2026-04-22** — Added full joystick API summary, strengths, and limitations.

# Joystick API overview

HonkordGL’s joystick layer is a **discovery, state, control, and light input-snapshot** abstraction focused on:

- Enumerating connected controllers (including Linux evdev-only gamepads when `/proc/bus/input/devices` reports them)
- Querying stable IDs, paths, names, player index, and USB-style descriptors when available
- Open/close lifecycle management
- Battery/power querying (where supported)
- Virtual joystick injection and output simulation
- Optional logical mapping database (runtime + file persistence) without SDL

Primary surface is in `include/HonkordGL/Joystick.h`, with platform implementations in `src/Joystick/Joystick.cpp`.

---

## Core API surface

- **Discovery and identity**
  - `GetJoystickCount`, `IsJoystickConnected`
  - `GetJoystickId`, `GetJoystickPath`, `GetJoystickName`
  - `GetJoystickPlayerIndex`
  - `GetJoystickDescriptor` (USB ids and counts when the OS exposes them)

- **Open/close lifecycle**
  - `OpenJoystick`, `CloseJoystick`, `CloseJoystickById`, `CloseAllJoysticks`

- **Power**
  - `GetJoystickBattery` with `JoystickBatteryInfo` and `JoystickPowerState`

- **Input snapshot (normalized-ish)**
  - `PollJoystickInputSnapshot` with `JoystickInputSnapshot` (axes, hat, button mask)
  - Coverage is reported per device via `GetJoystickCapabilityDepth::supports_input_snapshot`

- **Output packet / haptics / LEDs**
  - `SendJoystickOutputPacket`
  - `SetJoystickLedColor`
  - `StartJoystickTriggerRumble`, `StopJoystickTriggerRumble`

- **Hotplug callbacks**
  - `SetJoystickHotplugCallback`, `ClearJoystickHotplugCallback`
  - `PollJoystickHotplug` (explicit poll)
  - When a callback is registered, `GetJoystickCount()` also runs an internal connect/disconnect diff on each refresh (re-entrant safe).

- **Mapping database**
  - `SetJoystickMapping`, `GetJoystickMapping`, `ClearJoystickMapping`, `GetJoystickMappingCount`
  - `SaveJoystickMappingsToFile` / `LoadJoystickMappingsFromFile` — SDL `gamecontrollerdb` text format (`guid,name,bindings...`) for persistence
  - SDL2 `gamecontrollerdb` (no SDL link): `ParseGameControllerDbMappingLine`, `LoadGameControllerDbMappingsFromFile` (optional `GameControllerDbPlatformFilter` for `platform:`-split vendor/community DB lines), `GetGameControllerDbMappingCount`, `ApplyGameControllerDbMappingForJoystickIndex`, `ImportGameControllerDbMappingForJoystickIndex`
  - `DefaultGameControllerDbPlatformFilter`, `ConvertGameControllerDbFileToHonkordMappingsFile` (filter/copy SDL text), `GetJoystickGameControllerDbGuid`, `ApplyLoadedGameControllerDbMappingsToConnectedJoysticks`
  - Logical + actions: `GetJoystickLogicalScalar`, `AreJoystickLogicalChordPressed`, `SetJoystickActionBinding`, `ClearJoystickActionBindings`, `IsJoystickActionPressed`
  - `EnsureCanonicalJoystickMapping` (VID/PID first, then name heuristics), `GetCanonicalJoystickProfileCount`

- **Capability depth**
  - `GetJoystickCapabilityDepth` (includes `supports_input_snapshot`, `lists_evdev_only_gamepads` on Linux)

- **Virtual devices**
  - `AttachVirtualJoystick`, `DetachVirtualJoystick`, `DetachVirtualJoystickById`
  - `IsVirtualJoystick`, `IsVirtualJoystickId`
  - Virtual state feeds:
    - `SetVirtualJoystickAxis`
    - `SetVirtualJoystickBallMotion`
    - `SetVirtualJoystickTouchpadFinger`
    - `SetVirtualJoystickSensor`

---

## Strengths

- **Cross-platform coverage**
  - Windows (XInput), Linux and BSD (`/dev/input/js*` plus Linux evdev-only listings), Android (`InputDevice`), macOS (IOKit HID), Emscripten (HTML5 Gamepad), plus a virtual-only fallback path.

- **Consistent cross-platform API**
  - Same discovery, descriptor, open/close, mapping, and capability query API regardless of backend.

- **Rich virtual joystick support**
  - You can simulate axes, ball motion, touchpads, sensors, LEDs, and trigger rumble in software.
  - Useful for testing, deterministic replay, CI, and tooling.

- **Stable-ish per-device identity model**
  - `JoystickId` maps to platform-specific stable keys (e.g. dev+inode hash on Linux, XInput slot, Android device id, IOKit identity).

- **Battery integration where available**
  - Windows and Android have better physical battery integration; virtual devices always report wired-style defaults.

- **Clean separation of descriptor vs live state**
  - Descriptors (`JoystickDescriptor`, touchpad/sensor descriptors) are explicit and reusable for virtual devices.

---

## Limitations

- **Physical input snapshot coverage**
  - After `OpenJoystick`, live physical snapshots are implemented on:
    - Windows (XInput)
    - Linux (`js*` and listed evdev nodes)
    - Emscripten (HTML5 Gamepad API)
    - Android (poll + `FeedAndroidJoystickMotionEvent` for sticks/triggers/hat and `FeedAndroidJoystickKeyEvent` for face/shoulder buttons)
    - macOS (IOKit HID element values)
  - On platforms that use the generic joystick backend (no OS-specific enumeration), **physical** snapshots are unavailable (`supports_input_snapshot` is false for non-virtual indices). **Virtual** devices still expose axis snapshots after `OpenJoystick` — same as other backends. Always consult `GetJoystickCapabilityDepth::supports_input_snapshot` per index.

- **Physical HID output beyond common gamepad paths**
  - Virtual LED and trigger rumble are fully supported in software. Windows exposes physical motor rumble via XInput; per-LED RGB on many USB controllers is not wired here and remains vendor-specific.

- **Linux evdev-only discovery is heuristic**
  - `/proc/bus/input/devices` parsing uses EV masks plus name hints; unusual vendors or stripped names may still require manual `AttachVirtualJoystick` / custom tooling.

- **Mapping persistence vs community / vendor-split databases**
  - `GetJoystickGameControllerDbGuid` requires non-zero VID/PID from `GetJoystickDescriptor`; some backends (e.g. XInput with zero product id, browser environments without USB ids) may not resolve a community DB GUID until descriptor fidelity is richer.

---

## Practical usage notes

- Prefer `GetJoystickCapabilityDepth` before assuming snapshot, rumble, LED, or battery behavior on physical hardware.
- After `OpenJoystick(index)`, call `PollJoystickInputSnapshot` on a steady tick where `supports_input_snapshot` is true.
- On Android, forward both motion and key events (`FeedAndroidJoystickMotionEvent`, `FeedAndroidJoystickKeyEvent`) so analog + button state stays accurate between polls.
- For hotplug, calling `PollJoystickHotplug()` switches callback emission to explicit-poll mode; `GetJoystickCount()` no longer emits duplicate callback deltas in that mode.
- Use `EnsureCanonicalJoystickMapping` after enumeration so VID/PID-based profiles apply before name heuristics.
- To ship SDL/community mappings, keep SDL format end-to-end: optionally filter with `ConvertGameControllerDbFileToHonkordMappingsFile(..., DefaultGameControllerDbPlatformFilter())`, then `LoadJoystickMappingsFromFile` (or `LoadGameControllerDbMappingsFromFile` for GUID store only). At runtime, `ApplyLoadedGameControllerDbMappingsToConnectedJoysticks()` applies entries whose GUID matches `GetJoystickGameControllerDbGuid`.
- Use virtual joysticks for automated tests and deterministic simulation when the host has no physical controller.