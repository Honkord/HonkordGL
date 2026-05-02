/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — joystick discovery (connected devices, paths, IDs, player index)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_JOYSTICK_H
#define HONKORDGL_JOYSTICK_H

#include <HonkordGL/Config.h>

#include <cstddef>
#include <cstdint>

namespace HonkordGL::Joystick {

enum class JoystickType : unsigned int {
    Unknown = 0,
    Gamepad = 1,
    Wheel = 2,
    Arcade_Stick = 3,
    Flight_Stick = 4,
    Dance_Pad = 5,
    Guitar = 6,
    Drum_Kit = 7,
    MIDI_Controller = 8,
    Other = 9,
};

enum class JoystickSensorKind : unsigned int {
    Unknown = 0,
    Accelerometer = 1,
    Gyroscope = 2,
    Magnetometer = 3,
};

/**
 * Describes one touch surface (trackpad-style) on a composite game controller.
 * Physical size fields are optional; 0 means unknown / not reported.
 */
struct JoystickTouchpadDescriptor {
    int id{0};
    std::uint32_t widthMicrons{0};
    std::uint32_t heightMicrons{0};
    bool supportsPressure{false};
};

/**
 * Describes one motion or environmental sensor exposed by the device.
 */
struct JoystickSensorDescriptor {
    JoystickSensorKind kind{JoystickSensorKind::Unknown};
    float maxRange{0.f};
};

/**
 * High-level layout summary for a joystick or gamepad (counts and USB-style ids when known).
 */
struct JoystickDescriptor {
    JoystickType type{JoystickType::Unknown};
    std::uint16_t vendorId{0};
    std::uint16_t productId{0};
    std::uint16_t versionBcd{0};
    int buttonCount{0};
    int axisCount{0};
    int hatCount{0};
    int touchpadCount{0};
    int sensorCount{0};
};

/**
 * Opaque stable identifier for a connected joystick at the time of the query.
 * Value 0 means invalid / unknown.
 */
struct JoystickId {
    std::uint64_t value{0};
};

enum class JoystickHotplugEvent : unsigned int {
    Connected = 1,
    Disconnected = 2,
};

using JoystickHotplugCallback = void (*)(JoystickHotplugEvent event, JoystickId id, int index_hint, void * user);

enum class JoystickMappingInputKind : unsigned int {
    Button = 0,
    Axis = 1,
    Hat = 2,
};

enum class JoystickLogicalInput : unsigned int {
    Unknown = 0,
    FaceDown,
    FaceRight,
    FaceLeft,
    FaceUp,
    Start,
    Select,
    Guide,
    LeftShoulder,
    RightShoulder,
    LeftStickPress,
    RightStickPress,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    LeftTrigger,
    RightTrigger,
};

struct JoystickMappingEntry {
    JoystickLogicalInput logical{JoystickLogicalInput::Unknown};
    JoystickMappingInputKind source_kind{JoystickMappingInputKind::Button};
    int source_index{-1};
    float scale{1.f};
    float deadzone{0.f};
};

struct JoystickMapping {
    JoystickMappingEntry entries[64]{};
    int entry_count{0};
};

/**
 * Filters SDL `gamecontrollerdb` lines that include a `platform:…` tag (vendor / OS-specific community DB rows).
 * `Any` keeps every line; otherwise lines whose platform tag does not match are skipped. Lines with **no** platform tag
 * are kept for every filter (generic mappings).
 */
enum class GameControllerDbPlatformFilter : unsigned int {
    Any = 0,
    Windows = 1,
    Mac = 2,
    Linux = 3,
    Android = 4,
    IOS = 5,
};

struct JoystickCapabilityDepth {
    bool is_virtual{false};
    bool supports_led{false};
    bool supports_trigger_rumble{false};
    bool supports_battery_query{false};
    bool supports_mapping_db{true};
    bool supports_hotplug_callback{true};
    /** True when `PollJoystickInputSnapshot` returns live physical (or opened virtual axis) data on this target. */
    bool supports_input_snapshot{false};
    /** Linux: list includes evdev-only gamepads discovered via `/proc/bus/input/devices` (no companion `js*` node). */
    bool lists_evdev_only_gamepads{false};
    int axis_buffer_depth{0};
    int button_buffer_depth{0};
    int hat_buffer_depth{0};
    int touchpad_buffer_depth{0};
    int sensor_buffer_depth{0};
};

/** Max axes returned by `PollJoystickInputSnapshot` (packed snapshot). */
constexpr int kJoystickInputSnapshotMaxAxes = 16;

/**
 * Normalized-ish snapshot of sticks, triggers, hat, and up to 64 face buttons.
 * Axes are raw driver units scaled to approximately [-1, 1] where the backend can do so.
 */
struct JoystickInputSnapshot {
    int axis_count{0};
    float axes[kJoystickInputSnapshotMaxAxes]{};
    int hat_x{0};
    int hat_y{0};
    std::uint64_t buttons_pressed_mask{0};
    int button_count{0};
};

/** Bitmask for `JoystickOutputPacket::flags`. */
enum JoystickOutputPacketFlags : std::uint32_t {
    JoystickOutputPacketFlag_None = 0,
    JoystickOutputPacketFlag_Led = 1u << 0,
    JoystickOutputPacketFlag_TriggerRumble = 1u << 1,
    JoystickOutputPacketFlag_CloseAfter = 1u << 2,
};

/**
 * Batched output / haptic fields for `SendJoystickOutputPacket`.
 * Only fields whose bits are set in `flags` are applied. Virtual devices honor LED and trigger rumble in software;
 * physical devices honor fields reported by `GetJoystickCapabilityDepth` for the current platform.
 */
struct JoystickOutputPacket {
    std::uint32_t flags{0};
    std::uint8_t led_r{0};
    std::uint8_t led_g{0};
    std::uint8_t led_b{0};
    float trigger_rumble_left{0.f};
    float trigger_rumble_right{0.f};
};

enum class JoystickPowerState : unsigned int {
    Unknown = 0,
    /** Powered over USB / AC (no meaningful battery percentage). */
    Wired = 1,
    Discharging = 2,
    Charging = 3,
    /** Battery reported as full on battery power. */
    Full = 4,
    /** Battery effectively empty. */
    Empty = 5,
};

struct JoystickBatteryInfo {
    /** Approximate charge 0–100, or -1 if unknown / not applicable (e.g. wired). */
    int level_percent{-1};
    JoystickPowerState power{JoystickPowerState::Unknown};
};

/**
 * Fills `out` with battery / power hints when available.
 * Returns false if `out` is null, `index` is invalid, or the platform cannot query this device.
 * Virtual joysticks report `Wired` with `level_percent` -1. Windows uses XInput battery when available;
 * Android uses `InputDevice` battery on API 31+. Other targets (Linux js*, macOS IOKit, Emscripten, dummy) typically
 * return false for physical devices unless extended.
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept;

/**
 * Refreshes internal state and returns how many joysticks are currently connected.
 * Indices for the other functions are always in the range [0, count).
 *
 * Implementation is chosen by the compiler target for `src/Joystick/Joystick.cpp`, not by `WindowBackendKind`
 * (X11, Wayland, GDK, Cocoa, Win32, EGL/Khronos-only, Vivante SoCs, etc. share the same OS joystick stack when
 * they share the same OS ABI). Mapping: Windows / “DirectX” builds → XInput; macOS / Cocoa → IOKit HID
 * (gamepad / joystick / multi-axis); Linux and BSD (including Raspberry Pi OS, typical embedded Vivante/EGL
 * Linux images) → `/dev/input/js*` when present; Android → `InputDevice`; Emscripten → HTML5 Gamepad API;
 * `HONKORDGL_JOYSTICK_DUMMY` → virtual devices only; any other target → virtual devices only until extended.
 * On Linux, evdev-only gamepads (no `js*` handler in the same kernel input device block) are also listed from
 * `/proc/bus/input/devices`. Other Unix-like builds continue to use `/dev/input/js*` glob discovery.
 */
HONKORDGL_API HONKORDGL_ND int GetJoystickCount() noexcept;

/**
 * True if `index` is in range for the last enumeration and the device is still present.
 * Equivalent to 0 <= index < GetJoystickCount() after a refresh.
 */
HONKORDGL_API HONKORDGL_ND bool IsJoystickConnected(int index) noexcept;

/**
 * Stable id for the joystick at `index` (platform-specific; e.g. dev+inode hash on Linux, XInput slot on Windows).
 * Returns {0} if `index` is out of range or unavailable.
 */
HONKORDGL_API HONKORDGL_ND JoystickId GetJoystickId(int index) noexcept;

/**
 * Implementation-dependent path or name (UTF-8), e.g. `/dev/input/js0` on Linux, `XInput:0` on Windows.
 * Writes a null-terminated string if `buffer_size > 0`; returns false if the path does not fit or is unavailable.
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept;

/**
 * Human-readable product / display name (UTF-8) when the platform exposes one.
 * On Linux (evdev js*), may come from the device name ioctl; on Android, `InputDevice.getName()`; on Windows, a generic XInput label.
 * Virtual devices use the same string as `GetJoystickPath` (attach label or `virtual:<id>`).
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept;

/**
 * Fills `out` with best-effort USB ids and axis/button counts when the platform exposes them.
 * Returns false if `out` is null, `index` is invalid, or metadata is unavailable.
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept;

/**
 * Player / user index when the platform exposes one (e.g. XInput slots 0–3).
 * On Linux js* enumeration, returns the enumeration index (0, 1, …) as a logical player order.
 * Returns -1 if unknown or not applicable.
 */
HONKORDGL_API HONKORDGL_ND int GetJoystickPlayerIndex(int index) noexcept;

/**
 * Marks a joystick as opened for use by the application.
 * On Android, tracks the underlying input device id for lifecycle; on Windows/Linux, succeeds if the device is connected.
 * Returns false if `index` is invalid or the device cannot be opened.
 */
HONKORDGL_API HONKORDGL_ND bool OpenJoystick(int index) noexcept;

/** Releases a previous `OpenJoystick` for `index` (no-op if not opened). */
HONKORDGL_API void CloseJoystick(int index) noexcept;

/**
 * Closes the joystick identified by `id` when `id` is a virtual joystick id; no-op for physical or unknown ids.
 */
HONKORDGL_API void CloseJoystickById(JoystickId id) noexcept;

/** Releases all joysticks opened with `OpenJoystick`. */
HONKORDGL_API void CloseAllJoysticks() noexcept;

/**
 * Applies a batched output packet to `index` (LED / trigger rumble for opened virtual devices).
 * If `JoystickOutputPacketFlag_CloseAfter` is set in `packet->flags`, calls `CloseJoystick(index)` after applying other fields.
 * Returns false if `packet` is null, or if a requested LED / rumble field could not be applied.
 */
HONKORDGL_API HONKORDGL_ND bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept;

/**
 * Sets an RGB LED color for the joystick when the platform or device supports it.
 * For opened virtual joysticks, stores the color for application-driven UI.
 * Physical support is platform-dependent (query with `GetJoystickCapabilityDepth`).
 */
HONKORDGL_API HONKORDGL_ND bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;

/**
 * Starts trigger rumble: `left` / `right` are normalized intensity in [0, 1] (0 = off).
 * For opened virtual joysticks, values are stored for application-driven output.
 * Physical support is platform-dependent (query with `GetJoystickCapabilityDepth`).
 */
HONKORDGL_API HONKORDGL_ND bool StartJoystickTriggerRumble(int index, float left, float right) noexcept;

/** Stops trigger rumble for `index` (clears virtual trigger rumble state when `index` is a virtual slot). */
HONKORDGL_API void StopJoystickTriggerRumble(int index) noexcept;

/**
 * Appends a software-only joystick after physical devices in enumeration order.
 * Returns the new global index (appended after physical devices).
 * `descriptor` may be null (unknown / empty layout). `label` may be null for an auto path `virtual:<id>` (UTF-8).
 */
HONKORDGL_API HONKORDGL_ND int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept;

/**
 * Removes a virtual joystick created with `AttachVirtualJoystick`.
 * No-op if `index` is not a virtual slot. Later virtual indices shift down after removal.
 */
HONKORDGL_API void DetachVirtualJoystick(int index) noexcept;

/** True if `index` refers to a virtual device (not a physical enumerated joystick). */
HONKORDGL_API HONKORDGL_ND bool IsVirtualJoystick(int index) noexcept;

/** True if `id` is a Honkord virtual joystick id (from `GetJoystickId` on a virtual slot or after attach). */
HONKORDGL_API HONKORDGL_ND bool IsVirtualJoystickId(JoystickId id) noexcept;

/** Removes a virtual joystick by id; returns true if one was removed. Indices of remaining virtual devices may shift. */
HONKORDGL_API HONKORDGL_ND bool DetachVirtualJoystickById(JoystickId id) noexcept;

/**
 * Sets an axis value for an opened virtual joystick (`OpenJoystick` was successful for that device).
 * Typical range is [-1, 1]. Returns false if `id` is not virtual, not opened, or `axis_index` is out of range.
 */
HONKORDGL_API HONKORDGL_ND bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept;

/**
 * Sets trackball-style relative motion for an opened virtual joystick (replaces any previous delta for that `ball_index`).
 * `ball_index` selects which logical ball (0-based). Returns false if not applicable.
 */
HONKORDGL_API HONKORDGL_ND bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept;

/**
 * Updates one touchpad finger for an opened virtual joystick.
 * `x` and `y` are normalized surface coordinates (e.g. 0–1). `pressure` in [0, 1]. `pressed` false lifts the finger.
 */
HONKORDGL_API HONKORDGL_ND bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y,
    float pressure, bool pressed) noexcept;

/**
 * Updates a 3-axis sensor sample for an opened virtual joystick (`kind` selects which logical sensor).
 * Components are implementation-defined units (e.g. m/s², rad/s, µT) chosen by the application.
 */
HONKORDGL_API HONKORDGL_ND bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept;

/**
 * Sets/clears a global hotplug callback.
 * When a callback is registered, `GetJoystickCount()` also performs an internal hotplug diff on each refresh.
 * `PollJoystickHotplug()` remains available for explicit polling without relying on count queries.
 */
HONKORDGL_API void SetJoystickHotplugCallback(JoystickHotplugCallback fn, void * user) noexcept;
HONKORDGL_API void ClearJoystickHotplugCallback() noexcept;

/**
 * Polls the current joystick id list and emits callback events for connect/disconnect deltas.
 * Returns the number of events emitted in this poll.
 */
HONKORDGL_API HONKORDGL_ND int PollJoystickHotplug() noexcept;

/**
 * Fills `out` with a polled snapshot for `index` after `OpenJoystick(index)` succeeded.
 * Physical coverage is platform-dependent (see `GetJoystickCapabilityDepth::supports_input_snapshot`).
 */
HONKORDGL_API HONKORDGL_ND bool PollJoystickInputSnapshot(int index, JoystickInputSnapshot * out) noexcept;

/** Runtime capability/depth summary for one joystick index. */
HONKORDGL_API HONKORDGL_ND bool GetJoystickCapabilityDepth(int index, JoystickCapabilityDepth * out) noexcept;

/** Public mapping database (runtime in-memory) keyed by joystick id. */
HONKORDGL_API HONKORDGL_ND bool SetJoystickMapping(JoystickId id, JoystickMapping const * mapping) noexcept;
HONKORDGL_API HONKORDGL_ND bool GetJoystickMapping(JoystickId id, JoystickMapping * out) noexcept;
HONKORDGL_API HONKORDGL_ND bool ClearJoystickMapping(JoystickId id) noexcept;
HONKORDGL_API HONKORDGL_ND int GetJoystickMappingCount() noexcept;

/**
 * Serializes mappings as SDL2 `gamecontrollerdb` lines (`guid,name,bindings...`).
 * This intentionally uses SDL text format only (no Honkord-specific file dialect) to avoid merge/interop conflicts.
 * Runtime `JoystickId` mappings are exported when they can be resolved to a connected SDL GUID.
 */
HONKORDGL_API HONKORDGL_ND bool SaveJoystickMappingsToFile(const char * path_utf8) noexcept;
/** Loads SDL2 `gamecontrollerdb` lines; `merge=false` clears both the id DB and GUID store first. */
HONKORDGL_API HONKORDGL_ND bool LoadJoystickMappingsFromFile(const char * path_utf8, bool merge) noexcept;

/**
 * Parses one SDL2 `gamecontrollerdb` mapping line (`guid,name,comma-separated pairs`) into `out_map`.
 * `aN` / `bN` map to axis/button entries; `hN.mask` (SDL hat bitmask) maps to `JoystickMappingInputKind::Hat` with `source_index` encoding `(hatIndex<<8)|mask`.
 */
HONKORDGL_API HONKORDGL_ND bool ParseGameControllerDbMappingLine(const char * line_utf8, JoystickMapping * out_map) noexcept;

/** Loads SDL `gamecontrollerdb` lines into an in-memory store keyed by the first-field GUID (32 hex, normalized lowercase). */
HONKORDGL_API HONKORDGL_ND bool LoadGameControllerDbMappingsFromFile(const char * path_utf8, bool merge,
    GameControllerDbPlatformFilter platform_filter = GameControllerDbPlatformFilter::Any) noexcept;

/** `platform_filter` suitable for the current build target (OS) when ingesting vendor-split community databases. */
HONKORDGL_API GameControllerDbPlatformFilter DefaultGameControllerDbPlatformFilter() noexcept;

/** Number of GUID-keyed rows in the SDL mapping store (after load or v2 file load). */
HONKORDGL_API HONKORDGL_ND int GetGameControllerDbMappingCount() noexcept;

/**
 * Writes a 32-character lowercase hex SDL2 / `gamecontrollerdb` joystick GUID when the device reports USB-style
 * vendor/product ids (layout and name CRC match SDL 2.26 `SDL_CreateJoystickGUID` for USB bus type 3).
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickGameControllerDbGuid(int index, char * buffer_utf8, std::size_t buffer_size) noexcept;

/** For each connected non-virtual joystick, applies a matching entry from the GUID store to `SetJoystickMapping`. Returns how many were applied. */
HONKORDGL_API HONKORDGL_ND int ApplyLoadedGameControllerDbMappingsToConnectedJoysticks() noexcept;

/**
 * Converts/filter-copies an SDL `gamecontrollerdb` text file into another SDL-format file (normalized GUID casing).
 * Use `DefaultGameControllerDbPlatformFilter()` to keep rows tagged for this OS/vendor database split.
 */
HONKORDGL_API HONKORDGL_ND bool ConvertGameControllerDbFileToHonkordMappingsFile(const char * src_gamecontrollerdb_utf8,
    const char * dest_honkord_mappings_utf8, GameControllerDbPlatformFilter platform_filter) noexcept;

/** Applies a previously stored GUID entry from `LoadGameControllerDbMappingsFromFile` to `GetJoystickId(index)`. */
HONKORDGL_API HONKORDGL_ND bool ApplyGameControllerDbMappingForJoystickIndex(int index, const char * sdl_guid_utf8) noexcept;

/** Convenience: parse `line_utf8` and assign the mapping to the joystick at `index`. */
HONKORDGL_API HONKORDGL_ND bool ImportGameControllerDbMappingForJoystickIndex(int index, const char * line_utf8) noexcept;

/**
 * Resolves one logical control using `mapping` and the latest `snapshot` (button mask / axes).
 * For axes, contributions from multiple mapping rows are summed; for buttons, the maximum pressed scalar is used.
 */
HONKORDGL_API HONKORDGL_ND bool GetJoystickLogicalScalar(const JoystickInputSnapshot * snapshot, const JoystickMapping * mapping, JoystickLogicalInput logical,
    float * out_value) noexcept;

/** True when every listed logical reads as pressed (scalar >= `threshold`), using the same rules as `GetJoystickLogicalScalar`. */
HONKORDGL_API HONKORDGL_ND bool AreJoystickLogicalChordPressed(const JoystickInputSnapshot * snapshot, const JoystickMapping * mapping,
    const JoystickLogicalInput * logicals, int logical_count, float threshold) noexcept;

struct JoystickActionBinding {
    char action_name[48]{};
    JoystickLogicalInput logicals[8]{};
    int logical_count{0};
    float threshold{0.5f};
};

/** Stores or replaces one action binding for `id` (runtime only). */
HONKORDGL_API HONKORDGL_ND bool SetJoystickActionBinding(JoystickId id, const JoystickActionBinding * binding) noexcept;
/** Removes all action bindings for `id`. */
HONKORDGL_API void ClearJoystickActionBindings(JoystickId id) noexcept;
/** Returns true if the named action is currently pressed (all bound logicals meet threshold). */
HONKORDGL_API HONKORDGL_ND bool IsJoystickActionPressed(JoystickId id, const JoystickInputSnapshot * snapshot, const char * action_name_utf8) noexcept;

/**
 * Applies built-in canonical profiles (without SDL) using joystick name heuristics.
 * Returns true if a canonical mapping exists and is now stored for the joystick id.
 */
HONKORDGL_API HONKORDGL_ND bool EnsureCanonicalJoystickMapping(int index) noexcept;
HONKORDGL_API HONKORDGL_ND int GetCanonicalJoystickProfileCount() noexcept;

#if defined(__ANDROID__)
/** Feed motion events from the Android input queue so analog sticks/triggers can populate `PollJoystickInputSnapshot`. Pass `AInputEvent*`. */
HONKORDGL_API void FeedAndroidJoystickMotionEvent(void const * ainput_event) noexcept;
/** Feed key events from the Android input queue so face/shoulder button state is reflected in `PollJoystickInputSnapshot`. Pass `AInputEvent*`. */
HONKORDGL_API void FeedAndroidJoystickKeyEvent(void const * ainput_event) noexcept;
#endif

} // namespace HonkordGL::Joystick

#endif