/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — joystick discovery (implementation)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Joystick.h>

#include <cstring>

#include <cstdlib>

#include <cmath>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using HonkordGL::Joystick::JoystickBatteryInfo;
using HonkordGL::Joystick::JoystickDescriptor;
using HonkordGL::Joystick::JoystickCapabilityDepth;
using HonkordGL::Joystick::JoystickHotplugCallback;
using HonkordGL::Joystick::JoystickHotplugEvent;
using HonkordGL::Joystick::JoystickId;
using HonkordGL::Joystick::JoystickMapping;
using HonkordGL::Joystick::JoystickOutputPacket;
using HonkordGL::Joystick::JoystickPowerState;
using HonkordGL::Joystick::JoystickSensorKind;

constexpr int kVirtualMaxAxes = 128;
constexpr int kVirtualMaxBalls = 8;
constexpr int kVirtualMaxTouchpads = 8;
constexpr int kVirtualMaxFingersPerPad = 16;

constexpr std::uint64_t kVirtualIdPrefix = 0x564A000000000000ULL;

struct VirtualTouchFinger {
    bool pressed{false};
    float x{0.f};
    float y{0.f};
    float pressure{0.f};
};

struct VirtualSensorSample {
    float x{0.f};
    float y{0.f};
    float z{0.f};
    bool valid{false};
};

struct VirtualEntry {
    std::uint64_t id{};
    JoystickDescriptor desc{};
    std::string path;
    int player_index{-1};
    std::vector<float> axis_values;
    std::vector<std::pair<float, float>> ball_deltas;
    std::vector<std::vector<VirtualTouchFinger>> touchpads;
    VirtualSensorSample sensor_by_kind[4]{};
    std::uint8_t led_r{0};
    std::uint8_t led_g{0};
    std::uint8_t led_b{0};
    bool led_valid{false};
    float trigger_rumble_left{0.f};
    float trigger_rumble_right{0.f};
    bool trigger_rumble_active{false};
};

std::vector<VirtualEntry> g_virtual;
std::uint64_t g_virtual_next_id{1};
std::unordered_set<std::uint64_t> g_virtual_open;
std::map<std::uint64_t, JoystickMapping> g_mapping_db;
std::unordered_map<std::string, JoystickMapping> g_gamecontrollerdb_by_guid;
std::unordered_map<std::uint64_t, std::vector<HonkordGL::Joystick::JoystickActionBinding>> g_action_bindings_db;
std::vector<JoystickId> g_hotplug_snapshot;
JoystickHotplugCallback g_hotplug_cb{nullptr};
void * g_hotplug_user{nullptr};
bool g_hotplug_explicit_mode{false};

bool MappingEquals(const JoystickMapping& a, const JoystickMapping& b) noexcept
{
    if (a.entry_count != b.entry_count)
        return false;
    for (int i = 0; i < a.entry_count; ++i) {
        const auto& ea = a.entries[static_cast<std::size_t>(i)];
        const auto& eb = b.entries[static_cast<std::size_t>(i)];
        if (ea.logical != eb.logical || ea.source_kind != eb.source_kind || ea.source_index != eb.source_index || ea.scale != eb.scale || ea.deadzone != eb.deadzone)
            return false;
    }
    return true;
}

void AddMapButton(JoystickMapping& m, HonkordGL::Joystick::JoystickLogicalInput logical, int index) noexcept
{
    constexpr int kMax = static_cast<int>(sizeof(m.entries) / sizeof(m.entries[0]));
    if (m.entry_count >= kMax)
        return;
    auto& e = m.entries[m.entry_count++];
    e.logical = logical;
    e.source_kind = HonkordGL::Joystick::JoystickMappingInputKind::Button;
    e.source_index = index;
    e.scale = 1.f;
    e.deadzone = 0.f;
}

void AddMapAxis(JoystickMapping& m, HonkordGL::Joystick::JoystickLogicalInput logical, int index, float deadzone) noexcept
{
    constexpr int kMax = static_cast<int>(sizeof(m.entries) / sizeof(m.entries[0]));
    if (m.entry_count >= kMax)
        return;
    auto& e = m.entries[m.entry_count++];
    e.logical = logical;
    e.source_kind = HonkordGL::Joystick::JoystickMappingInputKind::Axis;
    e.source_index = index;
    e.scale = 1.f;
    e.deadzone = deadzone;
}

JoystickMapping BuildCanonicalXboxStyleMapping() noexcept
{
    using HonkordGL::Joystick::JoystickLogicalInput;
    JoystickMapping m{};
    AddMapButton(m, JoystickLogicalInput::FaceDown, 0); // A
    AddMapButton(m, JoystickLogicalInput::FaceRight, 1); // B
    AddMapButton(m, JoystickLogicalInput::FaceLeft, 2); // X
    AddMapButton(m, JoystickLogicalInput::FaceUp, 3); // Y
    AddMapButton(m, JoystickLogicalInput::LeftShoulder, 4);
    AddMapButton(m, JoystickLogicalInput::RightShoulder, 5);
    AddMapButton(m, JoystickLogicalInput::Select, 6);
    AddMapButton(m, JoystickLogicalInput::Start, 7);
    AddMapButton(m, JoystickLogicalInput::LeftStickPress, 8);
    AddMapButton(m, JoystickLogicalInput::RightStickPress, 9);
    AddMapButton(m, JoystickLogicalInput::DpadUp, 10);
    AddMapButton(m, JoystickLogicalInput::DpadDown, 11);
    AddMapButton(m, JoystickLogicalInput::DpadLeft, 12);
    AddMapButton(m, JoystickLogicalInput::DpadRight, 13);
    AddMapAxis(m, JoystickLogicalInput::LeftStickX, 0, 0.1f);
    AddMapAxis(m, JoystickLogicalInput::LeftStickY, 1, 0.1f);
    AddMapAxis(m, JoystickLogicalInput::RightStickX, 2, 0.1f);
    AddMapAxis(m, JoystickLogicalInput::RightStickY, 3, 0.1f);
    AddMapAxis(m, JoystickLogicalInput::LeftTrigger, 4, 0.05f);
    AddMapAxis(m, JoystickLogicalInput::RightTrigger, 5, 0.05f);
    return m;
}

JoystickMapping BuildCanonicalPlayStationStyleMapping() noexcept
{
    using HonkordGL::Joystick::JoystickLogicalInput;
    JoystickMapping m = BuildCanonicalXboxStyleMapping();
    if (m.entry_count >= 4) {
        m.entries[0].logical = JoystickLogicalInput::FaceDown; // Cross
        m.entries[1].logical = JoystickLogicalInput::FaceRight; // Circle
        m.entries[2].logical = JoystickLogicalInput::FaceLeft; // Square
        m.entries[3].logical = JoystickLogicalInput::FaceUp; // Triangle
    }
    return m;
}

JoystickMapping BuildCanonicalNintendoStyleMapping() noexcept
{
    using HonkordGL::Joystick::JoystickLogicalInput;
    JoystickMapping m = BuildCanonicalXboxStyleMapping();
    if (m.entry_count >= 4) {
        m.entries[0].logical = JoystickLogicalInput::FaceRight; // B
        m.entries[1].logical = JoystickLogicalInput::FaceDown; // A
        m.entries[2].logical = JoystickLogicalInput::FaceUp; // Y
        m.entries[3].logical = JoystickLogicalInput::FaceLeft; // X
    }
    return m;
}

std::string ToLowerAscii(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool BuildCanonicalByName(const char * name_utf8, JoystickMapping * out) noexcept
{
    if (!name_utf8 || !out)
        return false;
    const std::string name = ToLowerAscii(name_utf8);
    if (name.find("xinput") != std::string::npos || name.find("xbox") != std::string::npos) {
        *out = BuildCanonicalXboxStyleMapping();
        return true;
    }
    if (name.find("dualshock") != std::string::npos || name.find("dualsense") != std::string::npos
        || name.find("playstation") != std::string::npos || name.find("sony") != std::string::npos) {
        *out = BuildCanonicalPlayStationStyleMapping();
        return true;
    }
    if (name.find("switch") != std::string::npos || name.find("nintendo") != std::string::npos
        || name.find("joy-con") != std::string::npos || name.find("pro controller") != std::string::npos) {
        *out = BuildCanonicalNintendoStyleMapping();
        return true;
    }
    if (name.find("gamepad") != std::string::npos || name.find("controller") != std::string::npos) {
        *out = BuildCanonicalXboxStyleMapping();
        return true;
    }
    return false;
}

bool BuildCanonicalByVidPid(std::uint16_t vendor_id, std::uint16_t product_id, JoystickMapping * out) noexcept
{
    (void)product_id;
    if (!out)
        return false;
    switch (vendor_id) {
    case 0x045Eu: // Microsoft
        *out = BuildCanonicalXboxStyleMapping();
        return true;
    case 0x054Cu: // Sony
        *out = BuildCanonicalPlayStationStyleMapping();
        return true;
    case 0x057Eu: // Nintendo
        *out = BuildCanonicalNintendoStyleMapping();
        return true;
    default:
        return false;
    }
}

int VirtualCount() noexcept
{
    return static_cast<int>(g_virtual.size());
}

int PushVirtual(const JoystickDescriptor * descriptor, const char * label, int physical_count) noexcept
{
    VirtualEntry e{};
    e.id = g_virtual_next_id++;
    if (descriptor)
        e.desc = *descriptor;
    if (label && label[0] != '\0')
        e.path = label;
    else {
        e.path = "virtual:";
        e.path += std::to_string(e.id);
    }
    if (e.desc.axisCount > 0 && e.desc.axisCount <= kVirtualMaxAxes)
        e.axis_values.assign(static_cast<std::size_t>(e.desc.axisCount), 0.f);
    g_virtual.push_back(std::move(e));
    return physical_count + VirtualCount() - 1;
}

bool VirtualIsIndex(int index, int physical_count) noexcept
{
    const int vi = index - physical_count;
    return vi >= 0 && vi < static_cast<int>(g_virtual.size());
}

const VirtualEntry * VirtualPtr(int index, int physical_count) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return nullptr;
    return &g_virtual[static_cast<std::size_t>(index - physical_count)];
}

JoystickId VirtualId(int index, int physical_count) noexcept
{
    const VirtualEntry * const e = VirtualPtr(index, physical_count);
    if (!e)
        return {};
    JoystickId id{};
    id.value = kVirtualIdPrefix | (e->id & 0x0000FFFFFFFFFFFFULL);
    return id;
}
bool VirtualPath(int index, int physical_count, char * buffer, std::size_t buffer_size) noexcept
{
    const VirtualEntry * const e = VirtualPtr(index, physical_count);
    if (!e || !buffer || buffer_size == 0)
        return false;
    if (e->path.size() + 1u > buffer_size)
        return false;
    std::memcpy(buffer, e->path.c_str(), e->path.size() + 1u);
    return true;
}
int VirtualPlayerIndex(int index, int physical_count) noexcept
{
    const VirtualEntry * const e = VirtualPtr(index, physical_count);
    if (!e)
        return -1;
    if (e->player_index >= 0)
        return e->player_index;
    return index;
}
bool VirtualOpen(int index, int physical_count) noexcept
{
    const VirtualEntry * const e = VirtualPtr(index, physical_count);
    if (!e)
        return false;
    g_virtual_open.insert(e->id);
    return true;
}
void VirtualClose(int index, int physical_count) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return;
    VirtualEntry& e = g_virtual[static_cast<std::size_t>(index - physical_count)];
    g_virtual_open.erase(e.id);
    e.trigger_rumble_left = 0.f;
    e.trigger_rumble_right = 0.f;
    e.trigger_rumble_active = false;
}
bool VirtualRemove(int index, int physical_count) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return false;
    const int vi = index - physical_count;
    g_virtual_open.erase(g_virtual[static_cast<std::size_t>(vi)].id);
    g_virtual.erase(g_virtual.begin() + vi);
    return true;
}
void VirtualClearOpen() noexcept
{
    g_virtual_open.clear();
    for (auto& e : g_virtual) {
        e.trigger_rumble_left = 0.f;
        e.trigger_rumble_right = 0.f;
        e.trigger_rumble_active = false;
    }
}

bool VirtualIdMatches(JoystickId jid) noexcept
{
    if (jid.value == 0)
        return false;
    return (jid.value & 0xFFFF000000000000ULL) == kVirtualIdPrefix;
}
VirtualEntry * VirtualMutableByJid(JoystickId jid) noexcept
{
    if (!VirtualIdMatches(jid))
        return nullptr;
    const std::uint64_t seq = jid.value & 0x0000FFFFFFFFFFFFULL;
    for (auto& e : g_virtual) {
        if (e.id == seq)
            return &e;
    }
    return nullptr;
}
bool VirtualDetachByIdImpl(JoystickId jid) noexcept
{
    if (!VirtualIdMatches(jid))
        return false;
    const std::uint64_t seq = jid.value & 0x0000FFFFFFFFFFFFULL;
    for (std::size_t i = 0; i < g_virtual.size(); ++i) {
        if (g_virtual[i].id != seq)
            continue;
        g_virtual_open.erase(g_virtual[i].id);
        g_virtual.erase(g_virtual.begin() + static_cast<std::ptrdiff_t>(i));
        return true;
    }
    return false;
}
bool VirtualIsIdImpl(JoystickId jid) noexcept
{
    return VirtualIdMatches(jid);
}
bool VirtualSetAxisImpl(JoystickId jid, int axis_index, float value) noexcept
{
    VirtualEntry * const e = VirtualMutableByJid(jid);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    if (axis_index < 0 || axis_index >= kVirtualMaxAxes)
        return false;
    const auto need = static_cast<std::size_t>(axis_index) + 1u;
    if (e->axis_values.size() < need)
        e->axis_values.resize(need, 0.f);
    e->axis_values[static_cast<std::size_t>(axis_index)] = value;
    return true;
}
bool VirtualSetBallMotionImpl(JoystickId jid, int ball_index, float delta_x, float delta_y) noexcept
{
    VirtualEntry * const e = VirtualMutableByJid(jid);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    if (ball_index < 0 || ball_index >= kVirtualMaxBalls)
        return false;
    const auto need = static_cast<std::size_t>(ball_index) + 1u;
    if (e->ball_deltas.size() < need)
        e->ball_deltas.resize(need, std::pair<float, float>{0.f, 0.f});
    e->ball_deltas[static_cast<std::size_t>(ball_index)].first = delta_x;
    e->ball_deltas[static_cast<std::size_t>(ball_index)].second = delta_y;
    return true;
}
bool VirtualSetTouchpadFingerImpl(JoystickId jid, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    VirtualEntry * const e = VirtualMutableByJid(jid);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    if (touchpad_index < 0 || touchpad_index >= kVirtualMaxTouchpads)
        return false;
    if (finger_index < 0 || finger_index >= kVirtualMaxFingersPerPad)
        return false;
    const auto tpi = static_cast<std::size_t>(touchpad_index);
    if (e->touchpads.size() <= tpi)
        e->touchpads.resize(tpi + 1u);
    auto& pad = e->touchpads[tpi];
    const auto fi = static_cast<std::size_t>(finger_index);
    if (pad.size() <= fi)
        pad.resize(fi + 1u);
    pad[fi].pressed = pressed;
    pad[fi].x = x;
    pad[fi].y = y;
    pad[fi].pressure = pressure;
    return true;
}

bool VirtualSetSensorImpl(JoystickId jid, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    VirtualEntry * const e = VirtualMutableByJid(jid);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    const auto ki = static_cast<unsigned>(kind);
    if (ki >= 4u)
        return false;
    VirtualSensorSample& s = e->sensor_by_kind[ki];
    s.x = x;
    s.y = y;
    s.z = z;
    s.valid = true;
    return true;
}

bool VirtualSetLedColorImpl(int index, int physical_count, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return false;
    VirtualEntry& e = g_virtual[static_cast<std::size_t>(index - physical_count)];
    if (g_virtual_open.count(e.id) == 0)
        return false;
    e.led_r = r;
    e.led_g = g;
    e.led_b = b;
    e.led_valid = true;
    return true;
}

float ClampUnitFloat(float v) noexcept
{
    if (v < 0.f)
        return 0.f;
    if (v > 1.f)
        return 1.f;
    return v;
}
bool VirtualStartTriggerRumbleImpl(int index, int physical_count, float left, float right) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return false;
    VirtualEntry& e = g_virtual[static_cast<std::size_t>(index - physical_count)];
    if (g_virtual_open.count(e.id) == 0)
        return false;
    e.trigger_rumble_left = ClampUnitFloat(left);
    e.trigger_rumble_right = ClampUnitFloat(right);
    e.trigger_rumble_active = (e.trigger_rumble_left > 0.f || e.trigger_rumble_right > 0.f);
    return true;
}
void VirtualStopTriggerRumbleImpl(int index, int physical_count) noexcept
{
    if (!VirtualIsIndex(index, physical_count))
        return;
    VirtualEntry& e = g_virtual[static_cast<std::size_t>(index - physical_count)];
    e.trigger_rumble_left = 0.f;
    e.trigger_rumble_right = 0.f;
    e.trigger_rumble_active = false;
}
bool VirtualSendOutputPacketImpl(int index, int physical_count, const JoystickOutputPacket * packet) noexcept
{
    using HonkordGL::Joystick::JoystickOutputPacketFlag_Led;
    using HonkordGL::Joystick::JoystickOutputPacketFlag_TriggerRumble;
    if (!packet)
        return false;
    const std::uint32_t want = static_cast<std::uint32_t>(JoystickOutputPacketFlag_Led)
        | static_cast<std::uint32_t>(JoystickOutputPacketFlag_TriggerRumble);
    if ((packet->flags & want) == 0)
        return true;
    bool ok = true;
    if (packet->flags & static_cast<std::uint32_t>(JoystickOutputPacketFlag_Led)) {
        if (!VirtualSetLedColorImpl(index, physical_count, packet->led_r, packet->led_g, packet->led_b))
            ok = false;
    }
    if (packet->flags & static_cast<std::uint32_t>(JoystickOutputPacketFlag_TriggerRumble)) {
        if (!VirtualStartTriggerRumbleImpl(index, physical_count, packet->trigger_rumble_left, packet->trigger_rumble_right))
            ok = false;
    }
    return ok;
}
void VirtualCloseByIdImpl(JoystickId jid, int physical_count) noexcept
{
    if (!VirtualIdMatches(jid))
        return;
    const std::uint64_t seq = jid.value & 0x0000FFFFFFFFFFFFULL;
    for (std::size_t vi = 0; vi < g_virtual.size(); ++vi) {
        if (g_virtual[vi].id != seq)
            continue;
        VirtualClose(static_cast<int>(physical_count + static_cast<int>(vi)), physical_count);
        return;
    }
}
bool VirtualGetBatteryImpl(int index, int physical_count, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    if (!VirtualIsIndex(index, physical_count))
        return false;
    out->level_percent = -1;
    out->power = JoystickPowerState::Wired;
    return true;
}

std::uint16_t HonkordSdlCrc16(std::uint16_t crc, const unsigned char * data, std::size_t len) noexcept
{
    auto crc16_for_byte = [](unsigned char r) -> std::uint16_t {
        std::uint16_t c = 0;
        for (int i = 0; i < 8; ++i) {
            c = static_cast<std::uint16_t>(((c ^ r) & 1 ? 0xA001u : 0u) ^ (c >> 1));
            r = static_cast<unsigned char>(r >> 1);
        }
        return c;
    };
    for (std::size_t i = 0; i < len; ++i)
        crc = static_cast<std::uint16_t>(crc16_for_byte(static_cast<unsigned char>(static_cast<unsigned char>(crc) ^ data[i])) ^ (crc >> 8));
    return crc;
}

void BuildSdlUsbJoystickGuidBytes(std::uint16_t bus, std::uint16_t vendor, std::uint16_t product, std::uint16_t version, const char * product_name_utf8,
    unsigned char out16[16]) noexcept
{
    std::memset(out16, 0, 16);
    if (vendor == 0 && product == 0)
        return;
    std::uint16_t crc = 0;
    if (product_name_utf8 && product_name_utf8[0] != '\0')
        crc = HonkordSdlCrc16(0, reinterpret_cast<const unsigned char *>(product_name_utf8), std::strlen(product_name_utf8));
    auto put16 = [](unsigned char * p, std::uint16_t v) noexcept {
        p[0] = static_cast<unsigned char>(v & 0xFFu);
        p[1] = static_cast<unsigned char>((v >> 8) & 0xFFu);
    };
    put16(out16 + 0, bus);
    put16(out16 + 2, crc);
    put16(out16 + 4, vendor);
    put16(out16 + 6, 0);
    put16(out16 + 8, product);
    put16(out16 + 10, 0);
    put16(out16 + 12, version);
    out16[14] = 0;
    out16[15] = 0;
}

void WriteJoystickMappingEntryLines(std::ostream & out, JoystickMapping const & mapping) noexcept
{
    for (int i = 0; i < mapping.entry_count; ++i) {
        const auto& e = mapping.entries[static_cast<std::size_t>(i)];
        out << static_cast<unsigned>(e.logical) << " " << static_cast<unsigned>(e.source_kind) << " " << e.source_index << " " << e.scale << " " << e.deadzone
            << "\n";
    }
}

void HonkordTrimAscii(std::string& s) noexcept
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
}

std::string ExtractGameControllerDbPlatformTagOuter(std::string mapping_tail) noexcept
{
    while (!mapping_tail.empty()) {
        std::string token;
        const auto comma = mapping_tail.find(',');
        if (comma == std::string::npos) {
            token = std::move(mapping_tail);
            mapping_tail.clear();
        } else {
            token = mapping_tail.substr(0, comma);
            mapping_tail.erase(0, comma + 1);
        }
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))
            token.erase(token.begin());
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
            token.pop_back();
        const auto colon = token.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = token.substr(0, colon);
        std::string val = token.substr(colon + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front())))
            key.erase(key.begin());
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())))
            key.pop_back();
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front())))
            val.erase(val.begin());
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back())))
            val.pop_back();
        if (key == "platform")
            return val;
    }
    return {};
}

bool GameControllerDbPlatformTagMatchesOuter(HonkordGL::Joystick::GameControllerDbPlatformFilter filter, const std::string& platform_tag) noexcept
{
    using HonkordGL::Joystick::GameControllerDbPlatformFilter;
    if (filter == GameControllerDbPlatformFilter::Any)
        return true;
    if (platform_tag.empty())
        return true;
    std::string u = platform_tag;
    for (char& c : u)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto contains = [&](const char * sub) noexcept { return u.find(sub) != std::string::npos; };
    switch (filter) {
    case GameControllerDbPlatformFilter::Windows:
        return contains("windows") || contains("win32") || contains("xbox");
    case GameControllerDbPlatformFilter::Mac:
        return contains("mac") || contains("darwin");
    case GameControllerDbPlatformFilter::Linux:
        return contains("linux");
    case GameControllerDbPlatformFilter::Android:
        return contains("android");
    case GameControllerDbPlatformFilter::IOS:
        return contains("ios") || contains("iphone") || contains("ipad");
    default:
        return true;
    }
}

bool HatMaskActiveOuter(const HonkordGL::Joystick::JoystickInputSnapshot * snap, int mask) noexcept
{
    if (!snap || mask <= 0)
        return false;
    const bool up = snap->hat_y > 0;
    const bool down = snap->hat_y < 0;
    const bool left = snap->hat_x < 0;
    const bool right = snap->hat_x > 0;
    if ((mask & 1) && !up)
        return false;
    if ((mask & 4) && !down)
        return false;
    if ((mask & 8) && !left)
        return false;
    if ((mask & 2) && !right)
        return false;
    return true;
}

const char * LogicalToSdlField(HonkordGL::Joystick::JoystickLogicalInput logical) noexcept
{
    using HonkordGL::Joystick::JoystickLogicalInput;
    switch (logical) {
    case JoystickLogicalInput::FaceDown: return "a";
    case JoystickLogicalInput::FaceRight: return "b";
    case JoystickLogicalInput::FaceLeft: return "x";
    case JoystickLogicalInput::FaceUp: return "y";
    case JoystickLogicalInput::Select: return "back";
    case JoystickLogicalInput::Guide: return "guide";
    case JoystickLogicalInput::Start: return "start";
    case JoystickLogicalInput::LeftShoulder: return "leftshoulder";
    case JoystickLogicalInput::RightShoulder: return "rightshoulder";
    case JoystickLogicalInput::LeftStickPress: return "leftstick";
    case JoystickLogicalInput::RightStickPress: return "rightstick";
    case JoystickLogicalInput::LeftTrigger: return "lefttrigger";
    case JoystickLogicalInput::RightTrigger: return "righttrigger";
    case JoystickLogicalInput::LeftStickX: return "leftx";
    case JoystickLogicalInput::LeftStickY: return "lefty";
    case JoystickLogicalInput::RightStickX: return "rightx";
    case JoystickLogicalInput::RightStickY: return "righty";
    case JoystickLogicalInput::DpadUp: return "dpup";
    case JoystickLogicalInput::DpadDown: return "dpdown";
    case JoystickLogicalInput::DpadLeft: return "dpleft";
    case JoystickLogicalInput::DpadRight: return "dpright";
    default: return nullptr;
    }
}

bool AppendMappingEntryAsSdlToken(const HonkordGL::Joystick::JoystickMappingEntry& e, std::string * out) noexcept
{
    if (!out)
        return false;
    const char * const field = LogicalToSdlField(e.logical);
    if (!field)
        return false;
    std::string token(field);
    token.push_back(':');
    if (e.source_kind == HonkordGL::Joystick::JoystickMappingInputKind::Button) {
        token.push_back('b');
        token += std::to_string(e.source_index);
    } else if (e.source_kind == HonkordGL::Joystick::JoystickMappingInputKind::Axis) {
        token.push_back('a');
        token += std::to_string(e.source_index);
    } else if (e.source_kind == HonkordGL::Joystick::JoystickMappingInputKind::Hat) {
        const int hat_i = (e.source_index >> 8) & 0xFF;
        const int mask = e.source_index & 0xFF;
        token.push_back('h');
        token += std::to_string(hat_i);
        token.push_back('.');
        token += std::to_string(mask);
    } else
        return false;
    if (e.scale < 0.f)
        token.push_back('~');
    out->append(token);
    return true;
}

bool BuildSdlMappingLine(const std::string& guid, const HonkordGL::Joystick::JoystickMapping& mapping, std::string * out_line) noexcept
{
    if (!out_line || guid.empty())
        return false;
    out_line->clear();
    *out_line = guid + ",HonkordGL Mapping";
    for (int i = 0; i < mapping.entry_count; ++i) {
        const auto& e = mapping.entries[static_cast<std::size_t>(i)];
        std::string token;
        if (!AppendMappingEntryAsSdlToken(e, &token))
            continue;
        out_line->push_back(',');
        out_line->append(token);
    }
    return true;
}

void FormatSdlJoystickGuidHexLower(const unsigned char b[16], char * out33) noexcept
{
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) {
        out33[i * 2] = hexd[b[i] >> 4];
        out33[i * 2 + 1] = hexd[b[i] & 15];
    }
    out33[32] = '\0';
}

} // namespace

namespace HonkordGL::Joystick {

JoystickId GetJoystickId(int index) noexcept;
int GetJoystickCount() noexcept;

namespace {

thread_local int g_joystick_hotplug_tls_depth = 0;

int EmitJoystickHotplugDeltas(int count) noexcept
{
    std::vector<JoystickId> now;
    now.reserve(static_cast<std::size_t>(count > 0 ? count : 0));
    for (int i = 0; i < count; ++i) {
        const JoystickId id = GetJoystickId(i);
        if (id.value != 0)
            now.push_back(id);
    }

    int emitted = 0;
    if (g_hotplug_cb) {
        for (const JoystickId& id : now) {
            bool found = false;
            for (const JoystickId& old : g_hotplug_snapshot) {
                if (old.value == id.value) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                int idx = -1;
                for (int i = 0; i < count; ++i) {
                    if (GetJoystickId(i).value == id.value) {
                        idx = i;
                        break;
                    }
                }
                g_hotplug_cb(JoystickHotplugEvent::Connected, id, idx, g_hotplug_user);
                ++emitted;
            }
        }
        for (const JoystickId& old : g_hotplug_snapshot) {
            bool found = false;
            for (const JoystickId& id : now) {
                if (old.value == id.value) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                g_hotplug_cb(JoystickHotplugEvent::Disconnected, old, -1, g_hotplug_user);
                ++emitted;
            }
        }
    }

    g_hotplug_snapshot = std::move(now);
    return emitted;
}

} // namespace

void SetJoystickHotplugCallback(JoystickHotplugCallback fn, void * user) noexcept
{
    g_hotplug_cb = fn;
    g_hotplug_user = user;
    g_hotplug_snapshot.clear();
    g_hotplug_explicit_mode = false;
}

void ClearJoystickHotplugCallback() noexcept
{
    g_hotplug_cb = nullptr;
    g_hotplug_user = nullptr;
    g_hotplug_snapshot.clear();
    g_hotplug_explicit_mode = false;
}

int PollJoystickHotplug() noexcept
{
    g_hotplug_explicit_mode = true;
    ++g_joystick_hotplug_tls_depth;
    const int count = GetJoystickCount();
    const int emitted = EmitJoystickHotplugDeltas(count);
    --g_joystick_hotplug_tls_depth;
    return emitted;
}

bool GetJoystickCapabilityDepth(int index, JoystickCapabilityDepth * out) noexcept
{
    if (!out || !IsJoystickConnected(index))
        return false;

    out->is_virtual = IsVirtualJoystick(index);
    out->supports_mapping_db = true;
    out->supports_hotplug_callback = true;
    out->supports_led = out->is_virtual;
    out->supports_trigger_rumble = out->is_virtual;
    out->supports_battery_query = out->is_virtual;
    out->supports_input_snapshot = out->is_virtual;
    out->lists_evdev_only_gamepads = false;
    out->axis_buffer_depth = out->is_virtual ? kVirtualMaxAxes : 8;
    out->button_buffer_depth = out->is_virtual ? 64 : 32;
    out->hat_buffer_depth = out->is_virtual ? 8 : 4;
    out->touchpad_buffer_depth = out->is_virtual ? kVirtualMaxTouchpads : 0;
    out->sensor_buffer_depth = out->is_virtual ? 4 : 0;

#if defined(_WIN32)
    if (!out->is_virtual) {
        out->supports_trigger_rumble = true;
        out->supports_battery_query = true;
        out->supports_input_snapshot = true;
        out->axis_buffer_depth = 6;
        out->button_buffer_depth = 14;
        out->hat_buffer_depth = 1;
    }
#elif defined(__ANDROID__)
    if (!out->is_virtual) {
        out->supports_input_snapshot = true;
        out->supports_battery_query = true;
        out->axis_buffer_depth = 12;
        out->button_buffer_depth = 32;
        out->hat_buffer_depth = 2;
        out->sensor_buffer_depth = 2;
    }
#elif defined(__APPLE__)
    if (!out->is_virtual) {
        out->supports_input_snapshot = true;
        out->axis_buffer_depth = 12;
        out->button_buffer_depth = 32;
        out->hat_buffer_depth = 2;
    }
#elif defined(__EMSCRIPTEN__)
    if (!out->is_virtual) {
        out->supports_input_snapshot = true;
        out->axis_buffer_depth = 8;
        out->button_buffer_depth = 18;
        out->hat_buffer_depth = 1;
    }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    if (!out->is_virtual) {
        out->supports_input_snapshot = true;
        out->axis_buffer_depth = 8;
        out->button_buffer_depth = 32;
        out->hat_buffer_depth = 2;
    }
#if defined(__linux__)
    if (!out->is_virtual)
        out->lists_evdev_only_gamepads = true;
#endif
#endif
    return true;
}

bool SetJoystickMapping(JoystickId id, JoystickMapping const * mapping) noexcept
{
    if (id.value == 0 || !mapping)
        return false;
    constexpr int kMaxMappingEntries = static_cast<int>(sizeof(mapping->entries) / sizeof(mapping->entries[0]));
    if (mapping->entry_count < 0 || mapping->entry_count > kMaxMappingEntries)
        return false;
    g_mapping_db[id.value] = *mapping;
    return true;
}

bool GetJoystickMapping(JoystickId id, JoystickMapping * out) noexcept
{
    if (id.value == 0 || !out)
        return false;
    const auto it = g_mapping_db.find(id.value);
    if (it == g_mapping_db.end())
        return false;
    *out = it->second;
    return true;
}

bool ClearJoystickMapping(JoystickId id) noexcept
{
    if (id.value == 0)
        return false;
    return g_mapping_db.erase(id.value) > 0;
}

int GetJoystickMappingCount() noexcept
{
    return static_cast<int>(g_mapping_db.size());
}

bool SaveJoystickMappingsToFile(const char * path_utf8) noexcept
{
    if (!path_utf8 || path_utf8[0] == '\0')
        return false;
    std::ofstream out(path_utf8, std::ios::out | std::ios::trunc);
    if (!out)
        return false;
    std::unordered_map<std::string, JoystickMapping> emitted = g_gamecontrollerdb_by_guid;
    // Export GUID-backed community mappings first.
    for (const auto& kv : g_gamecontrollerdb_by_guid) {
        std::string line;
        if (BuildSdlMappingLine(kv.first, kv.second, &line))
            out << line << "\n";
    }
    // Export per-device runtime mappings only if we can resolve a connected SDL GUID.
    for (const auto& kv : g_mapping_db) {
        int match_index = -1;
        const int count = GetJoystickCount();
        for (int i = 0; i < count; ++i) {
            if (GetJoystickId(i).value == kv.first) {
                match_index = i;
                break;
            }
        }
        if (match_index < 0)
            continue;
        char guid[33]{};
        if (!GetJoystickGameControllerDbGuid(match_index, guid, sizeof(guid)))
            continue;
        std::string guid_key(guid);
        const auto eit = emitted.find(guid_key);
        if (eit != emitted.end()) {
            if (!MappingEquals(eit->second, kv.second))
                return false;
            continue;
        }
        emitted[guid_key] = kv.second;
        std::string line;
        if (BuildSdlMappingLine(guid, kv.second, &line))
            out << line << "\n";
    }
    return out.good();
}

bool LoadJoystickMappingsFromFile(const char * path_utf8, bool merge) noexcept
{
    if (!path_utf8 || path_utf8[0] == '\0')
        return false;
    std::ifstream in(path_utf8);
    if (!in)
        return false;
    if (!merge) {
        g_mapping_db.clear();
        g_gamecontrollerdb_by_guid.clear();
    }
    std::string line;
    while (std::getline(in, line)) {
        HonkordTrimAscii(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto c1 = line.find(',');
        const auto c2 = (c1 == std::string::npos) ? std::string::npos : line.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos)
            continue;
        std::string guid = line.substr(0, c1);
        HonkordTrimAscii(guid);
        for (char& c : guid)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        JoystickMapping mapping{};
        if (!ParseGameControllerDbMappingLine(line.c_str(), &mapping))
            continue;
        const auto it = g_gamecontrollerdb_by_guid.find(guid);
        if (it != g_gamecontrollerdb_by_guid.end() && !MappingEquals(it->second, mapping))
            return false;
        g_gamecontrollerdb_by_guid[guid] = mapping;
    }
    return true;
}

namespace {

using HonkordGL::Joystick::JoystickInputSnapshot;
using HonkordGL::Joystick::JoystickLogicalInput;
using HonkordGL::Joystick::JoystickMappingInputKind;

void TrimAscii(std::string& s) noexcept
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
}

bool ParseSdlHardwareBinding(std::string value, JoystickMappingInputKind * kind, int * index, float * scale) noexcept
{
    if (!kind || !index || !scale)
        return false;
    *scale = 1.f;
    TrimAscii(value);
    while (!value.empty() && (value.back() == '~' || value.back() == '+' || value.back() == '*')) {
        if (value.back() == '~')
            *scale *= -1.f;
        value.pop_back();
        TrimAscii(value);
    }
    if (value.empty())
        return false;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
    if (c == 'h') {
        const auto dot = value.find('.');
        if (dot == std::string::npos || dot < 2u)
            return false;
        char * endp = nullptr;
        const long hat_i = std::strtol(value.c_str() + 1, &endp, 10);
        if (endp != value.c_str() + static_cast<std::ptrdiff_t>(dot))
            return false;
        const long maskv = std::strtol(value.c_str() + dot + 1, &endp, 0);
        if (endp == value.c_str() + dot + 1 || maskv < 1 || maskv > 255)
            return false;
        const long hat_clamped = std::max(0L, std::min(hat_i, 15L));
        *kind = JoystickMappingInputKind::Hat;
        *index = static_cast<int>((hat_clamped << 8) | maskv);
        return true;
    }
    if (c != 'a' && c != 'b')
        return false;
    const char * const tail = value.c_str() + 1;
    char * endp = nullptr;
    const long idxl = std::strtol(tail, &endp, 10);
    if (endp == tail || idxl < 0 || idxl > 4096)
        return false;
    const int idx = static_cast<int>(idxl);
    *kind = (c == 'a') ? JoystickMappingInputKind::Axis : JoystickMappingInputKind::Button;
    *index = idx;
    return true;
}

JoystickLogicalInput SdlFieldToLogical(const std::string& key) noexcept
{
    if (key == "a")
        return JoystickLogicalInput::FaceDown;
    if (key == "b")
        return JoystickLogicalInput::FaceRight;
    if (key == "x")
        return JoystickLogicalInput::FaceLeft;
    if (key == "y")
        return JoystickLogicalInput::FaceUp;
    if (key == "back")
        return JoystickLogicalInput::Select;
    if (key == "guide")
        return JoystickLogicalInput::Guide;
    if (key == "start")
        return JoystickLogicalInput::Start;
    if (key == "leftshoulder")
        return JoystickLogicalInput::LeftShoulder;
    if (key == "rightshoulder")
        return JoystickLogicalInput::RightShoulder;
    if (key == "leftstick")
        return JoystickLogicalInput::LeftStickPress;
    if (key == "rightstick")
        return JoystickLogicalInput::RightStickPress;
    if (key == "lefttrigger")
        return JoystickLogicalInput::LeftTrigger;
    if (key == "righttrigger")
        return JoystickLogicalInput::RightTrigger;
    if (key == "leftx")
        return JoystickLogicalInput::LeftStickX;
    if (key == "lefty")
        return JoystickLogicalInput::LeftStickY;
    if (key == "rightx")
        return JoystickLogicalInput::RightStickX;
    if (key == "righty")
        return JoystickLogicalInput::RightStickY;
    if (key == "dpup")
        return JoystickLogicalInput::DpadUp;
    if (key == "dpdown")
        return JoystickLogicalInput::DpadDown;
    if (key == "dpleft")
        return JoystickLogicalInput::DpadLeft;
    if (key == "dpright")
        return JoystickLogicalInput::DpadRight;
    return JoystickLogicalInput::Unknown;
}

bool AppendParsedGameControllerPair(const std::string& token, JoystickMapping * out) noexcept
{
    if (!out)
        return false;
    const auto colon = token.find(':');
    if (colon == std::string::npos)
        return false;
    std::string key = token.substr(0, colon);
    std::string val = token.substr(colon + 1);
    TrimAscii(key);
    TrimAscii(val);
    if (key.empty() || val.empty())
        return false;
    if (key == "platform" || key == "hint")
        return true;
    if (key.size() >= 4 && key.compare(0, 4, "sdk_") == 0)
        return true;
    JoystickMappingInputKind kind{};
    int src = 0;
    float scale = 1.f;
    if (!ParseSdlHardwareBinding(std::move(val), &kind, &src, &scale))
        return true;
    const JoystickLogicalInput logical = SdlFieldToLogical(key);
    if (logical == JoystickLogicalInput::Unknown)
        return true;
    constexpr int kMax = static_cast<int>(sizeof(out->entries) / sizeof(out->entries[0]));
    if (out->entry_count >= kMax)
        return true;
    auto& e = out->entries[static_cast<std::size_t>(out->entry_count++)];
    e.logical = logical;
    e.source_kind = kind;
    e.source_index = src;
    e.scale = scale;
    e.deadzone = 0.f;
    return true;
}

bool ParseGameControllerMappingTail(std::string mapping_tail, JoystickMapping * out) noexcept
{
    if (!out)
        return false;
    out->entry_count = 0;
    TrimAscii(mapping_tail);
    if (mapping_tail.empty())
        return false;
    while (!mapping_tail.empty()) {
        std::string token;
        const auto comma = mapping_tail.find(',');
        if (comma == std::string::npos) {
            token = std::move(mapping_tail);
            mapping_tail.clear();
        } else {
            token = mapping_tail.substr(0, comma);
            mapping_tail.erase(0, comma + 1);
        }
        TrimAscii(token);
        if (!token.empty())
            (void)AppendParsedGameControllerPair(token, out);
    }
    return out->entry_count > 0;
}

} // namespace

bool ParseGameControllerDbMappingLine(const char * line_utf8, JoystickMapping * out_map) noexcept
{
    if (!line_utf8 || !out_map)
        return false;
    std::string line(line_utf8);
    TrimAscii(line);
    if (line.empty() || line[0] == '#')
        return false;
    const auto c1 = line.find(',');
    if (c1 == std::string::npos)
        return false;
    const auto c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos)
        return false;
    return ParseGameControllerMappingTail(line.substr(c2 + 1), out_map);
}

bool LoadGameControllerDbMappingsFromFile(const char * path_utf8, bool merge, GameControllerDbPlatformFilter platform_filter) noexcept
{
    if (!path_utf8 || path_utf8[0] == '\0')
        return false;
    std::ifstream in(path_utf8);
    if (!in)
        return false;
    if (!merge)
        g_gamecontrollerdb_by_guid.clear();
    std::string line;
    while (std::getline(in, line)) {
        TrimAscii(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto c1 = line.find(',');
        if (c1 == std::string::npos)
            continue;
        const auto c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos)
            continue;
        std::string guid = line.substr(0, c1);
        TrimAscii(guid);
        if (guid.empty())
            continue;
        for (char& c : guid)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const std::string mapping_tail = line.substr(c2 + 1);
        const std::string plat = ExtractGameControllerDbPlatformTagOuter(mapping_tail);
        if (!GameControllerDbPlatformTagMatchesOuter(platform_filter, plat))
            continue;
        JoystickMapping m{};
        if (!ParseGameControllerMappingTail(mapping_tail, &m))
            continue;
        const auto it = g_gamecontrollerdb_by_guid.find(guid);
        if (it != g_gamecontrollerdb_by_guid.end() && !MappingEquals(it->second, m))
            return false;
        g_gamecontrollerdb_by_guid[std::move(guid)] = m;
    }
    return true;
}

bool ApplyGameControllerDbMappingForJoystickIndex(int index, const char * sdl_guid_utf8) noexcept
{
    if (!sdl_guid_utf8 || sdl_guid_utf8[0] == '\0')
        return false;
    std::string guid(sdl_guid_utf8);
    TrimAscii(guid);
    for (char& c : guid)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const auto it = g_gamecontrollerdb_by_guid.find(guid);
    if (it == g_gamecontrollerdb_by_guid.end())
        return false;
    const JoystickId id = GetJoystickId(index);
    if (id.value == 0)
        return false;
    return SetJoystickMapping(id, &it->second);
}

bool ImportGameControllerDbMappingForJoystickIndex(int index, const char * line_utf8) noexcept
{
    JoystickMapping m{};
    if (!ParseGameControllerDbMappingLine(line_utf8, &m))
        return false;
    const JoystickId id = GetJoystickId(index);
    if (id.value == 0)
        return false;
    return SetJoystickMapping(id, &m);
}

bool GetJoystickLogicalScalar(const JoystickInputSnapshot * snapshot, const JoystickMapping * mapping, JoystickLogicalInput logical, float * out_value) noexcept
{
    if (!snapshot || !mapping || !out_value)
        return false;
    *out_value = 0.f;
    bool any = false;
    float axis_sum = 0.f;
    float button_best = 0.f;
    bool saw_axis = false;
    bool saw_button = false;
    for (int i = 0; i < mapping->entry_count; ++i) {
        const auto& e = mapping->entries[static_cast<std::size_t>(i)];
        if (e.logical != logical)
            continue;
        any = true;
        if (e.source_kind == JoystickMappingInputKind::Button) {
            saw_button = true;
            if (e.source_index >= 0 && e.source_index < 64) {
                const bool pressed = (snapshot->buttons_pressed_mask & (1ull << static_cast<unsigned>(e.source_index))) != 0;
                const float v = pressed ? e.scale : 0.f;
                button_best = std::max(button_best, v);
            }
        } else if (e.source_kind == JoystickMappingInputKind::Axis) {
            saw_axis = true;
            if (e.source_index >= 0 && e.source_index < snapshot->axis_count && e.source_index < HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes) {
                float v = snapshot->axes[static_cast<std::size_t>(e.source_index)] * e.scale;
                if (std::abs(v) < e.deadzone)
                    v = 0.f;
                axis_sum += v;
            }
        } else if (e.source_kind == JoystickMappingInputKind::Hat) {
            saw_button = true;
            const int mask = e.source_index & 0xFF;
            const bool on = HatMaskActiveOuter(snapshot, mask);
            const float v = on ? e.scale : 0.f;
            button_best = std::max(button_best, v);
        }
    }
    if (!any)
        return false;
    if (saw_axis && !saw_button)
        *out_value = axis_sum;
    else if (!saw_axis && saw_button)
        *out_value = button_best;
    else if (saw_axis && saw_button)
        *out_value = axis_sum + button_best;
    else
        *out_value = 0.f;
    return true;
}

bool AreJoystickLogicalChordPressed(const JoystickInputSnapshot * snapshot, const JoystickMapping * mapping, const JoystickLogicalInput * logicals, int logical_count,
    float threshold) noexcept
{
    if (!snapshot || !mapping || !logicals || logical_count <= 0)
        return false;
    for (int i = 0; i < logical_count; ++i) {
        float v = 0.f;
        if (!GetJoystickLogicalScalar(snapshot, mapping, logicals[i], &v) || v < threshold)
            return false;
    }
    return true;
}

bool SetJoystickActionBinding(JoystickId id, const JoystickActionBinding * binding) noexcept
{
    if (id.value == 0 || !binding || binding->action_name[0] == '\0' || binding->logical_count <= 0)
        return false;
    constexpr int kMaxLogicals = static_cast<int>(sizeof(binding->logicals) / sizeof(binding->logicals[0]));
    if (binding->logical_count > kMaxLogicals)
        return false;
    auto& list = g_action_bindings_db[id.value];
    for (auto& row : list) {
        if (std::strncmp(row.action_name, binding->action_name, sizeof(row.action_name)) == 0) {
            row = *binding;
            return true;
        }
    }
    list.push_back(*binding);
    return true;
}

void ClearJoystickActionBindings(JoystickId id) noexcept
{
    if (id.value == 0)
        return;
    g_action_bindings_db.erase(id.value);
}

bool IsJoystickActionPressed(JoystickId id, const JoystickInputSnapshot * snapshot, const char * action_name_utf8) noexcept
{
    if (id.value == 0 || !snapshot || !action_name_utf8 || action_name_utf8[0] == '\0')
        return false;
    const auto it = g_action_bindings_db.find(id.value);
    if (it == g_action_bindings_db.end())
        return false;
    JoystickMapping mapping{};
    if (!GetJoystickMapping(id, &mapping))
        return false;
    for (const auto& bind : it->second) {
        if (std::strncmp(bind.action_name, action_name_utf8, sizeof(bind.action_name)) != 0)
            continue;
        return AreJoystickLogicalChordPressed(snapshot, &mapping, bind.logicals, bind.logical_count, bind.threshold);
    }
    return false;
}

GameControllerDbPlatformFilter DefaultGameControllerDbPlatformFilter() noexcept
{
#if defined(__ANDROID__)
    return GameControllerDbPlatformFilter::Android;
#elif defined(__IPHONEOS__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
    return GameControllerDbPlatformFilter::IOS;
#elif defined(_WIN32)
    return GameControllerDbPlatformFilter::Windows;
#elif defined(__linux__)
    return GameControllerDbPlatformFilter::Linux;
#elif defined(__APPLE__)
    return GameControllerDbPlatformFilter::Mac;
#else
    return GameControllerDbPlatformFilter::Any;
#endif
}

int GetGameControllerDbMappingCount() noexcept
{
    return static_cast<int>(g_gamecontrollerdb_by_guid.size());
}

bool GetJoystickGameControllerDbGuid(int index, char * buffer_utf8, std::size_t buffer_size) noexcept
{
    if (!buffer_utf8 || buffer_size < 33)
        return false;
    if (!IsJoystickConnected(index) || IsVirtualJoystick(index))
        return false;
    JoystickDescriptor desc{};
    if (!GetJoystickDescriptor(index, &desc))
        return false;
    if (desc.vendorId == 0 && desc.productId == 0)
        return false;
    char name[256]{};
    (void)GetJoystickName(index, name, sizeof(name));
    unsigned char bytes[16]{};
    constexpr std::uint16_t kSdlUsbBus = 3;
    BuildSdlUsbJoystickGuidBytes(kSdlUsbBus, desc.vendorId, desc.productId, desc.versionBcd != 0 ? desc.versionBcd : 0, name, bytes);
    FormatSdlJoystickGuidHexLower(bytes, buffer_utf8);
    return true;
}

int ApplyLoadedGameControllerDbMappingsToConnectedJoysticks() noexcept
{
    int n = GetJoystickCount();
    int applied = 0;
    for (int i = 0; i < n; ++i) {
        if (IsVirtualJoystick(i))
            continue;
        char guid[40]{};
        if (!GetJoystickGameControllerDbGuid(i, guid, sizeof(guid)))
            continue;
        if (ApplyGameControllerDbMappingForJoystickIndex(i, guid))
            ++applied;
    }
    return applied;
}

bool ConvertGameControllerDbFileToHonkordMappingsFile(const char * src_gamecontrollerdb_utf8, const char * dest_honkord_mappings_utf8,
    GameControllerDbPlatformFilter platform_filter) noexcept
{
    if (!src_gamecontrollerdb_utf8 || !dest_honkord_mappings_utf8 || src_gamecontrollerdb_utf8[0] == '\0' || dest_honkord_mappings_utf8[0] == '\0')
        return false;
    std::ifstream in(src_gamecontrollerdb_utf8);
    std::ofstream out(dest_honkord_mappings_utf8, std::ios::out | std::ios::trunc);
    if (!in || !out)
        return false;
    std::string line;
    while (std::getline(in, line)) {
        HonkordTrimAscii(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto c1 = line.find(',');
        if (c1 == std::string::npos)
            continue;
        const auto c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos)
            continue;
        std::string guid = line.substr(0, c1);
        HonkordTrimAscii(guid);
        if (guid.empty())
            continue;
        for (char& c : guid)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const std::string mapping_tail = line.substr(c2 + 1);
        const std::string plat = ExtractGameControllerDbPlatformTagOuter(mapping_tail);
        if (!GameControllerDbPlatformTagMatchesOuter(platform_filter, plat))
            continue;
        out << line << "\n";
    }
    return out.good();
}

} // namespace HonkordGL::Joystick

#if defined(HONKORDGL_JOYSTICK_DUMMY)

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    const int n = VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    return index >= 0 && index < VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    return VirtualId(index, 0);
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    return VirtualPath(index, 0, buffer, buffer_size);
}
bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    return VirtualPath(index, 0, buffer, buffer_size);
}
bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    return VirtualGetBatteryImpl(index, 0, out);
}
int GetJoystickPlayerIndex(int index) noexcept
{
    return VirtualPlayerIndex(index, 0);
}
bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    return VirtualOpen(index, 0);
}
void CloseJoystick(int index) noexcept
{
    VirtualClose(index, 0);
}
void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
}
bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return VirtualSetLedColorImpl(index, 0, r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    return VirtualStartTriggerRumbleImpl(index, 0, left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    VirtualStopTriggerRumbleImpl(index, 0);
}
bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, 0, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}
void CloseJoystickById(JoystickId id) noexcept
{
    VirtualCloseByIdImpl(id, 0);
}
int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    return PushVirtual(descriptor, label, 0);
}
void DetachVirtualJoystick(int index) noexcept
{
    (void)VirtualRemove(index, 0);
}
bool IsVirtualJoystick(int index) noexcept
{
    return VirtualIsIndex(index, 0);
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}
bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}
bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}
bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    if (!VirtualIsIndex(index, 0))
        return false;
    const VirtualEntry * const e = VirtualPtr(index, 0);
    if (!e)
        return false;
    *out = e->desc;
    return true;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    if (!VirtualIsIndex(index, 0))
        return false;
    const VirtualEntry * const e = VirtualPtr(index, 0);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    std::memset(out, 0, sizeof(*out));
    const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
    out->axis_count = na;
    for (int ai = 0; ai < na; ++ai)
        out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
    return true;
}

} // namespace HonkordGL::Joystick

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>

namespace {

struct XInputSlot {
    unsigned slot{0};
};

std::vector<XInputSlot> g_xinput_slots;
std::vector<unsigned char> g_xinput_slot_open;

using XInputGetStateFn = DWORD(WINAPI *)(DWORD, void *);
using XInputSetStateFn = DWORD(WINAPI *)(DWORD, void *);

#pragma pack(push, 1)
struct XInputBatteryInfoWin {
    unsigned char BatteryType;
    unsigned char BatteryLevel;
};
#pragma pack(pop)

using XInputGetBatteryInformationFn = DWORD(WINAPI *)(DWORD, unsigned char, XInputBatteryInfoWin *);

bool QueryXInputBattery(unsigned user_index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    static XInputGetBatteryInformationFn batFn = nullptr;
    static bool triedBat = false;
    if (!triedBat) {
        triedBat = true;
        HMODULE h = LoadLibraryW(L"XInput1_4.dll");
        if (!h)
            h = LoadLibraryW(L"XInput1_3.dll");
        if (h)
            batFn = reinterpret_cast<XInputGetBatteryInformationFn>(GetProcAddress(h, "XInputGetBatteryInformation"));
    }
    if (!batFn)
        return false;
    XInputBatteryInfoWin bi{};
    constexpr unsigned char kDevGamepad = 0;
    if (batFn(static_cast<DWORD>(user_index), kDevGamepad, &bi) != ERROR_SUCCESS)
        return false;
    constexpr unsigned char kTypeWired = 1;
    constexpr unsigned char kTypeUnknown = 0xFF;
    constexpr unsigned char kTypeEmpty = 0;
    if (bi.BatteryType == kTypeWired) {
        out->power = JoystickPowerState::Wired;
        out->level_percent = -1;
        return true;
    }
    if (bi.BatteryType == kTypeUnknown) {
        out->power = JoystickPowerState::Unknown;
        out->level_percent = -1;
        return true;
    }
    if (bi.BatteryType == kTypeEmpty) {
        out->power = JoystickPowerState::Empty;
        out->level_percent = 0;
        return true;
    }
    switch (bi.BatteryLevel) {
    case 0:
        out->power = JoystickPowerState::Empty;
        out->level_percent = 0;
        return true;
    case 1:
        out->power = JoystickPowerState::Discharging;
        out->level_percent = 25;
        return true;
    case 2:
        out->power = JoystickPowerState::Discharging;
        out->level_percent = 50;
        return true;
    case 3:
        out->power = JoystickPowerState::Full;
        out->level_percent = 100;
        return true;
    default:
        out->power = JoystickPowerState::Unknown;
        out->level_percent = -1;
        return true;
    }
}
bool LoadXInput(XInputGetStateFn * out_fn) noexcept
{
    *out_fn = nullptr;
    HMODULE h2 = LoadLibraryW(L"XInput1_4.dll");
    if (!h2)
        h2 = LoadLibraryW(L"XInput1_3.dll");
    if (!h2)
        h2 = LoadLibraryW(L"XInput9_1_0.dll");
    if (!h2)
        return false;
    auto * const p = reinterpret_cast<XInputGetStateFn>(GetProcAddress(h2, "XInputGetState"));
    if (!p)
        return false;
    *out_fn = p;
    return true;
}

bool SetXInputRumble(unsigned user_index, float left, float right) noexcept
{
    static XInputSetStateFn set_fn = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE h = LoadLibraryW(L"XInput1_4.dll");
        if (!h)
            h = LoadLibraryW(L"XInput1_3.dll");
        if (!h)
            h = LoadLibraryW(L"XInput9_1_0.dll");
        if (h)
            set_fn = reinterpret_cast<XInputSetStateFn>(GetProcAddress(h, "XInputSetState"));
    }
    if (!set_fn)
        return false;

    struct XInputVibration {
        unsigned short left_motor_speed;
        unsigned short right_motor_speed;
    } vib{};
    const float l = ClampUnitFloat(left);
    const float r = ClampUnitFloat(right);
    vib.left_motor_speed = static_cast<unsigned short>(l * 65535.f);
    vib.right_motor_speed = static_cast<unsigned short>(r * 65535.f);
    return set_fn(static_cast<DWORD>(user_index), &vib) == ERROR_SUCCESS;
}
void RefreshXInput() noexcept
{
    g_xinput_slots.clear();
    g_xinput_slot_open.clear();
    XInputGetStateFn fn = nullptr;
    if (!LoadXInput(&fn) || !fn)
        return;

    alignas(void *) unsigned char state[64]{};

    for (unsigned i = 0; i < 4; ++i) {
        std::memset(state, 0, sizeof(state));
        if (fn(i, state) == ERROR_SUCCESS)
            g_xinput_slots.push_back(XInputSlot{i});
    }
    g_xinput_slot_open.assign(g_xinput_slots.size(), 0);
}

} // namespace

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    RefreshXInput();
    const int n = static_cast<int>(g_xinput_slots.size()) + VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    return index >= 0 && index < pc + VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualId(index, pc);
    if (index < 0 || index >= pc)
        return {};
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    JoystickId id{};
    id.value = 0x58494E00u | static_cast<std::uint64_t>(s);
    return id;
}

bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    const int n = std::snprintf(buffer, buffer_size, "XInput:%u", s);
    if (n < 0 || static_cast<std::size_t>(n) >= buffer_size)
        return false;
    return true;
}

bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    const int n = std::snprintf(buffer, buffer_size, "XInput Controller %u", s);
    if (n < 0 || static_cast<std::size_t>(n) >= buffer_size)
        return false;
    return true;
}

bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualGetBatteryImpl(index, pc, out);
    if (index < 0 || index >= pc)
        return false;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    return QueryXInputBattery(s, out);
}

int GetJoystickPlayerIndex(int index) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPlayerIndex(index, pc);
    if (index < 0 || index >= pc)
        return -1;
    return static_cast<int>(g_xinput_slots[static_cast<std::size_t>(index)].slot);
}

bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualOpen(index, pc);
    if (index >= 0 && index < pc && index < static_cast<int>(g_xinput_slot_open.size()))
        g_xinput_slot_open[static_cast<std::size_t>(index)] = 1;
    return true;
}

void CloseJoystick(int index) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        VirtualClose(index, pc);
    else if (index >= 0 && index < pc && index < static_cast<int>(g_xinput_slot_open.size()))
        g_xinput_slot_open[static_cast<std::size_t>(index)] = 0;
}

void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
    std::fill(g_xinput_slot_open.begin(), g_xinput_slot_open.end(), static_cast<unsigned char>(0));
}

bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    RefreshXInput();
    return VirtualSetLedColorImpl(index, static_cast<int>(g_xinput_slots.size()), r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc))
        return VirtualStartTriggerRumbleImpl(index, pc, left, right);
    if (index < 0 || index >= pc)
        return false;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    return SetXInputRumble(s, left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc)) {
        VirtualStopTriggerRumbleImpl(index, pc);
        return;
    }
    if (index < 0 || index >= pc)
        return;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
    (void)SetXInputRumble(s, 0.f, 0.f);
}

bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (!packet)
        return false;
    bool ok = true;
    if (VirtualIsIndex(index, pc)) {
        ok = VirtualSendOutputPacketImpl(index, pc, packet);
    } else if (index < 0 || index >= pc) {
        ok = false;
    } else {
        using HonkordGL::Joystick::JoystickOutputPacketFlag_Led;
        using HonkordGL::Joystick::JoystickOutputPacketFlag_TriggerRumble;
        if (packet->flags & static_cast<std::uint32_t>(JoystickOutputPacketFlag_Led))
            ok = false;
        if (packet->flags & static_cast<std::uint32_t>(JoystickOutputPacketFlag_TriggerRumble)) {
            const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
            if (!SetXInputRumble(s, packet->trigger_rumble_left, packet->trigger_rumble_right))
                ok = false;
        }
    }
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}

void CloseJoystickById(JoystickId id) noexcept
{
    RefreshXInput();
    VirtualCloseByIdImpl(id, static_cast<int>(g_xinput_slots.size()));
}

int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    RefreshXInput();
    return PushVirtual(descriptor, label, static_cast<int>(g_xinput_slots.size()));
}

void DetachVirtualJoystick(int index) noexcept
{
    RefreshXInput();
    (void)VirtualRemove(index, static_cast<int>(g_xinput_slots.size()));
}
bool IsVirtualJoystick(int index) noexcept
{
    RefreshXInput();
    return VirtualIsIndex(index, static_cast<int>(g_xinput_slots.size()));
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}
bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}
bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}

bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e)
            return false;
        *out = e->desc;
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    out->type = JoystickType::Gamepad;
    out->vendorId = 0x045E;
    out->productId = 0;
    out->versionBcd = 0;
    out->buttonCount = 14;
    out->axisCount = 6;
    out->hatCount = 1;
    out->touchpadCount = 0;
    out->sensorCount = 0;
    return true;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    RefreshXInput();
    const int pc = static_cast<int>(g_xinput_slots.size());
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e || g_virtual_open.count(e->id) == 0)
            return false;
        std::memset(out, 0, sizeof(*out));
        const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        out->axis_count = na;
        for (int i = 0; i < na; ++i)
            out->axes[static_cast<std::size_t>(i)] = e->axis_values[static_cast<std::size_t>(i)];
        out->button_count = 0;
        return true;
    }
    if (index < 0 || index >= pc || index >= static_cast<int>(g_xinput_slot_open.size()) || !g_xinput_slot_open[static_cast<std::size_t>(index)])
        return false;

    XInputGetStateFn fn = nullptr;
    if (!LoadXInput(&fn) || !fn)
        return false;
    const unsigned s = g_xinput_slots[static_cast<std::size_t>(index)].slot;
#pragma pack(push, 1)
    struct XInputGamepadPacked {
        std::uint16_t wButtons;
        std::uint8_t bLeftTrigger;
        std::uint8_t bRightTrigger;
        std::int16_t sThumbLX;
        std::int16_t sThumbLY;
        std::int16_t sThumbRX;
        std::int16_t sThumbRY;
    };
    struct XInputStatePacked {
        std::uint32_t dwPacketNumber;
        XInputGamepadPacked Gamepad;
    };
#pragma pack(pop)
    XInputStatePacked st{};
    if (fn(s, &st) != ERROR_SUCCESS)
        return false;

    std::memset(out, 0, sizeof(*out));
    out->axis_count = 6;
    out->axes[0] = std::max(-1.f, std::min(1.f, static_cast<float>(st.Gamepad.sThumbLX) / 32768.f));
    out->axes[1] = std::max(-1.f, std::min(1.f, static_cast<float>(st.Gamepad.sThumbLY) / 32768.f));
    out->axes[2] = std::max(-1.f, std::min(1.f, static_cast<float>(st.Gamepad.sThumbRX) / 32768.f));
    out->axes[3] = std::max(-1.f, std::min(1.f, static_cast<float>(st.Gamepad.sThumbRY) / 32768.f));
    out->axes[4] = static_cast<float>(st.Gamepad.bLeftTrigger) / 255.f;
    out->axes[5] = static_cast<float>(st.Gamepad.bRightTrigger) / 255.f;

    constexpr std::uint16_t kUp = 0x0001;
    constexpr std::uint16_t kDown = 0x0002;
    constexpr std::uint16_t kLeft = 0x0004;
    constexpr std::uint16_t kRight = 0x0008;
    const std::uint16_t w = st.Gamepad.wButtons;
    if ((w & kUp) && !(w & kDown))
        out->hat_y = 1;
    else if ((w & kDown) && !(w & kUp))
        out->hat_y = -1;
    if ((w & kLeft) && !(w & kRight))
        out->hat_x = -1;
    else if ((w & kRight) && !(w & kLeft))
        out->hat_x = 1;

    std::uint64_t mask = 0;
    int bi = 0;
    auto addBtn = [&](bool pressed) {
        if (pressed && bi < 64)
            mask |= (1ull << static_cast<unsigned>(bi));
        ++bi;
    };
    addBtn((w & 0x1000u) != 0);
    addBtn((w & 0x2000u) != 0);
    addBtn((w & 0x4000u) != 0);
    addBtn((w & 0x8000u) != 0);
    addBtn((w & 0x0100u) != 0);
    addBtn((w & 0x0200u) != 0);
    addBtn((w & 0x0020u) != 0);
    addBtn((w & 0x0010u) != 0);
    addBtn((w & 0x0040u) != 0);
    addBtn((w & 0x0080u) != 0);
    addBtn((w & kUp) != 0);
    addBtn((w & kDown) != 0);
    addBtn((w & kLeft) != 0);
    addBtn((w & kRight) != 0);
    out->buttons_pressed_mask = mask;
    out->button_count = bi;
    return true;
}

} // namespace HonkordGL::Joystick

#elif defined(__EMSCRIPTEN__)

#include <cstdio>

#include <emscripten/html5.h>

namespace {

std::vector<int> g_ems_browser_index;
std::vector<std::string> g_ems_ids;
std::unordered_set<int> g_ems_open_browser_indices;

std::uint64_t HashUtf8Id(const char * s) noexcept
{
    if (!s)
        s = "";
    std::uint64_t h = 14695981039346656037ull;
    for (const unsigned char * p = reinterpret_cast<const unsigned char *>(s); *p; ++p) {
        h ^= static_cast<std::uint64_t>(*p);
        h *= 1099511628211ull;
    }
    h |= 0x454D530000000000ULL;
    return h;
}

void RefreshEmscriptenGamepads() noexcept
{
    g_ems_browser_index.clear();
    g_ems_ids.clear();
    emscripten_sample_gamepad_data();
    const int n = emscripten_get_num_gamepads();
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i) {
        EmscriptenGamepadEvent ev{};
        if (emscripten_get_gamepad_status(i, &ev) != 0)
            continue;
        if (!ev.connected)
            continue;
        g_ems_browser_index.push_back(i);
        g_ems_ids.emplace_back(ev.id[0] ? ev.id : "gamepad");
    }
}

int PhysicalEmCount() noexcept
{
    RefreshEmscriptenGamepads();
    return static_cast<int>(g_ems_browser_index.size());
}

} // namespace

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    const int n = PhysicalEmCount() + VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    const int pc = PhysicalEmCount();
    return index >= 0 && index < pc + VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualId(index, pc);
    if (index < 0 || index >= pc)
        return {};
    JoystickId id{};
    id.value = HashUtf8Id(g_ems_ids[static_cast<std::size_t>(index)].c_str());
    return id;
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    const int bi = g_ems_browser_index[static_cast<std::size_t>(index)];
    const int nw = std::snprintf(buffer, buffer_size, "emscripten:%d", bi);
    if (nw < 0 || static_cast<std::size_t>(nw) >= buffer_size)
        return false;
    return true;
}
bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    const std::string& s = g_ems_ids[static_cast<std::size_t>(index)];
    if (s.size() + 1u > buffer_size)
        return false;
    std::memcpy(buffer, s.c_str(), s.size() + 1u);
    return true;
}
bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualGetBatteryImpl(index, pc, out);
    return false;
}
int GetJoystickPlayerIndex(int index) noexcept
{
    if (index < 0)
        return -1;
    if (!IsJoystickConnected(index))
        return -1;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPlayerIndex(index, pc);
    return index;
}
bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc))
        return VirtualOpen(index, pc);
    if (index < 0 || index >= pc)
        return false;
    const int bi = g_ems_browser_index[static_cast<std::size_t>(index)];
    g_ems_open_browser_indices.insert(bi);
    return true;
}
void CloseJoystick(int index) noexcept
{
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc)) {
        VirtualClose(index, pc);
        return;
    }
    if (index >= 0 && index < pc) {
        const int bi = g_ems_browser_index[static_cast<std::size_t>(index)];
        g_ems_open_browser_indices.erase(bi);
    }
}
void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
    g_ems_open_browser_indices.clear();
}
bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return VirtualSetLedColorImpl(index, PhysicalEmCount(), r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    return VirtualStartTriggerRumbleImpl(index, PhysicalEmCount(), left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    VirtualStopTriggerRumbleImpl(index, PhysicalEmCount());
}
bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    const int pc = PhysicalEmCount();
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, pc, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}
void CloseJoystickById(JoystickId id) noexcept
{
    VirtualCloseByIdImpl(id, PhysicalEmCount());
}
int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    return PushVirtual(descriptor, label, PhysicalEmCount());
}
void DetachVirtualJoystick(int index) noexcept
{
    (void)VirtualRemove(index, PhysicalEmCount());
}
bool IsVirtualJoystick(int index) noexcept
{
    return VirtualIsIndex(index, PhysicalEmCount());
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}
bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}
bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}
bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e)
            return false;
        *out = e->desc;
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    const int bi = g_ems_browser_index[static_cast<std::size_t>(index)];
    emscripten_sample_gamepad_data();
    EmscriptenGamepadEvent ev{};
    if (emscripten_get_gamepad_status(bi, &ev) != 0 || !ev.connected)
        return false;
    std::memset(out, 0, sizeof(*out));
    out->type = JoystickType::Gamepad;
    out->buttonCount = std::min(ev.numButtons, 64);
    out->axisCount = std::min(ev.numAxes, HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
    out->hatCount = 1;
    return true;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalEmCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e || g_virtual_open.count(e->id) == 0)
            return false;
        std::memset(out, 0, sizeof(*out));
        const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        out->axis_count = na;
        for (int ai = 0; ai < na; ++ai)
            out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    const int bi = g_ems_browser_index[static_cast<std::size_t>(index)];
    if (g_ems_open_browser_indices.count(bi) == 0)
        return false;
    emscripten_sample_gamepad_data();
    EmscriptenGamepadEvent ev{};
    if (emscripten_get_gamepad_status(bi, &ev) != 0 || !ev.connected)
        return false;
    std::memset(out, 0, sizeof(*out));
    out->axis_count = std::min(ev.numAxes, HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
    for (int ai = 0; ai < out->axis_count; ++ai)
        out->axes[static_cast<std::size_t>(ai)] = static_cast<float>(ev.axis[static_cast<std::size_t>(ai)]);
    const int nb = std::min(ev.numButtons, 64);
    std::uint64_t mask = 0;
    int bi_btn = 0;
    for (int i = 0; i < nb; ++i) {
        bool pressed = ev.digitalButton[static_cast<std::size_t>(i)] != 0;
        if (!pressed && ev.analogButton[static_cast<std::size_t>(i)] > 0.15)
            pressed = true;
        if (pressed && bi_btn < 64)
            mask |= (1ull << static_cast<unsigned>(bi_btn));
        ++bi_btn;
    }
    out->buttons_pressed_mask = mask;
    out->button_count = bi_btn;
    return true;
}

} // namespace HonkordGL::Joystick

#elif (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) \
    || defined(__OpenBSD__)) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
# include <linux/input.h>
# include <linux/joystick.h>
# include <linux/input-event-codes.h>
# include <sys/ioctl.h>
#endif

namespace {

struct LinuxDevPath {
    std::string path;
    bool is_evdev{false};
};

std::vector<LinuxDevPath> g_linux_paths;
std::vector<int> g_linux_fds;
struct LinuxLiveState {
    std::uint64_t buttons{0};
    float axes[HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes]{};
    int axis_count{0};
};
std::vector<LinuxLiveState> g_linux_live;

void LinuxCloseAllFds() noexcept
{
    for (int& fd : g_linux_fds) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
}

bool PathOpenable(const std::string& p) noexcept
{
    const int fd = ::open(p.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return false;
    ::close(fd);
    return true;
}
std::uint64_t IdForPath(const std::string& p) noexcept
{
    struct stat st {};
    if (::stat(p.c_str(), &st) == 0) {
        std::uint64_t v = static_cast<std::uint64_t>(static_cast<std::uint32_t>(st.st_dev));
        v <<= 32u;
        v |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(st.st_ino));
        return v;
    }
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : p) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}

#if defined(__linux__)
static std::string LinuxToLowerAscii(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool LinuxHandlersHasJs(const std::string& handlers) noexcept
{
    const std::size_t pos = handlers.find("js");
    if (pos == std::string::npos)
        return false;
    const std::size_t after = pos + 2;
    return after < handlers.size() && std::isdigit(static_cast<unsigned char>(handlers[after])) != 0;
}

static bool LinuxHandlersHasEvent(const std::string& handlers) noexcept
{
    const std::size_t pos = handlers.find("event");
    if (pos == std::string::npos)
        return false;
    const std::size_t after = pos + 5;
    return after < handlers.size() && std::isdigit(static_cast<unsigned char>(handlers[after])) != 0;
}

static void LinuxAppendProcBusDevices(std::vector<LinuxDevPath>& out, std::unordered_set<std::string>& seen) noexcept
{
    std::ifstream in("/proc/bus/input/devices");
    if (!in)
        return;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::size_t pos = 0;
    while (pos < content.size()) {
        std::size_t end = content.find("\n\n", pos);
        if (end == std::string::npos)
            end = content.size();
        const std::string block = content.substr(pos, end - pos);
        pos = end + 2;

        std::string name;
        std::string handlers;
        unsigned long ev_mask = 0;
        bool have_ev = false;
        std::istringstream bss(block);
        std::string line;
        while (std::getline(bss, line)) {
            if (line.size() >= 2 && line[0] == 'N' && line[1] == ':') {
                const std::size_t q1 = line.find('"');
                const std::size_t q2 = q1 == std::string::npos ? std::string::npos : line.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
                    name = line.substr(q1 + 1, q2 - q1 - 1);
            } else if (line.size() >= 2 && line[0] == 'H' && line[1] == ':') {
                const std::size_t eq = line.find('=');
                if (eq != std::string::npos)
                    handlers = line.substr(eq + 1);
            } else if (line.size() >= 5 && line[0] == 'B' && line[1] == ':' && line.find("EV=") != std::string::npos) {
                const std::size_t evp = line.find("EV=");
                if (evp != std::string::npos) {
                    const std::string hex = line.substr(evp + 3);
                    try {
                        ev_mask = std::stoul(hex, nullptr, 16);
                        have_ev = true;
                    } catch (...) {
                        have_ev = false;
                    }
                }
            }
        }

        const bool has_js = LinuxHandlersHasJs(handlers);
        const bool has_event = LinuxHandlersHasEvent(handlers);
        const std::string lname = LinuxToLowerAscii(name);
        const bool name_hint = lname.find("gamepad") != std::string::npos || lname.find("controller") != std::string::npos
            || lname.find("joystick") != std::string::npos || lname.find("xbox") != std::string::npos
            || lname.find("playstation") != std::string::npos || lname.find("sony") != std::string::npos
            || lname.find("dualshock") != std::string::npos || lname.find("dualsense") != std::string::npos
            || lname.find("nintendo") != std::string::npos || lname.find("switch") != std::string::npos;
        const bool ev_gamepadish = have_ev && ((ev_mask & (1ul << EV_KEY)) != 0) && ((ev_mask & (1ul << EV_ABS)) != 0);

        if (has_js) {
            const std::size_t jsp = handlers.find("js");
            if (jsp != std::string::npos && jsp + 2 < handlers.size()) {
                std::size_t j = jsp + 2;
                while (j < handlers.size() && std::isdigit(static_cast<unsigned char>(handlers[j])))
                    ++j;
                const std::string dev = "/dev/input/" + handlers.substr(jsp, j - jsp);
                if (seen.insert(dev).second)
                    out.push_back(LinuxDevPath{dev, false});
            }
        } else if (has_event && ev_gamepadish && name_hint) {
            const std::size_t ep = handlers.find("event");
            if (ep != std::string::npos && ep + 5 < handlers.size()) {
                std::size_t j = ep + 5;
                while (j < handlers.size() && std::isdigit(static_cast<unsigned char>(handlers[j])))
                    ++j;
                const std::string dev = "/dev/input/" + handlers.substr(ep, j - ep);
                if (seen.insert(dev).second)
                    out.push_back(LinuxDevPath{dev, true});
            }
        }
    }
}
#endif

static bool LinuxSamePathTable(const std::vector<LinuxDevPath>& a, const std::vector<LinuxDevPath>& b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].path != b[i].path || a[i].is_evdev != b[i].is_evdev)
            return false;
    }
    return true;
}

void RefreshInputDevices() noexcept
{
    std::vector<LinuxDevPath> next;
    std::unordered_set<std::string> seen;

#if defined(__linux__)
    LinuxAppendProcBusDevices(next, seen);
#endif
    glob_t g{};
    if (glob("/dev/input/js*", 0, nullptr, &g) == 0) {
        for (std::size_t i = 0; i < g.gl_pathc; ++i) {
            if (!g.gl_pathv[i])
                continue;
            const std::string dev = g.gl_pathv[i];
            if (seen.insert(dev).second)
                next.push_back(LinuxDevPath{dev, false});
        }
        globfree(&g);
    }
    std::sort(next.begin(), next.end(), [](const LinuxDevPath& a, const LinuxDevPath& b) { return a.path < b.path; });

    next.erase(std::remove_if(next.begin(), next.end(), [](const LinuxDevPath& d) { return !PathOpenable(d.path); }), next.end());

    if (LinuxSamePathTable(g_linux_paths, next))
        return;

    LinuxCloseAllFds();
    g_linux_paths = std::move(next);
    g_linux_fds.assign(g_linux_paths.size(), -1);
    g_linux_live.assign(g_linux_paths.size(), LinuxLiveState{});
#if defined(__linux__)
    for (std::size_t si = 0; si < g_linux_paths.size(); ++si) {
        if (g_linux_paths[si].is_evdev)
            continue;
        const int tfd = ::open(g_linux_paths[si].path.c_str(), O_RDONLY | O_NONBLOCK);
        if (tfd < 0)
            continue;
        char ax = 0;
        if (::ioctl(tfd, JSIOCGAXES, &ax) == 0)
            g_linux_live[si].axis_count = std::min(static_cast<int>(static_cast<unsigned char>(ax)), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        ::close(tfd);
    }
#endif
}

int PhysicalJsCount() noexcept
{
    RefreshInputDevices();
    return static_cast<int>(g_linux_paths.size());
}

bool CopyJoystickNameForPath(const LinuxDevPath& dev, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
#if defined(__linux__)
    const int fd = ::open(dev.path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        char name[256]{};
        int ir = -1;
        if (!dev.is_evdev) {
            ir = ::ioctl(fd, JSIOCGNAME(sizeof(name)), name);
        } else {
            ir = ::ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        }
        ::close(fd);
        if (ir >= 0) {
            name[sizeof(name) - 1u] = '\0';
            const std::size_t L = std::strlen(name);
            if (L + 1u > buffer_size)
                return false;
            std::memcpy(buffer, name, L + 1u);
            return true;
        }
    }
#endif
    const char * const path = dev.path.c_str();
    const char * const slash = std::strrchr(path, '/');
    const char * const base = slash ? slash + 1 : path;
    const std::size_t L = std::strlen(base);
    if (L + 1u > buffer_size)
        return false;
    std::memcpy(buffer, base, L + 1u);
    return true;
}

#if defined(__linux__)
static bool LinuxEnsureFd(int phys_index) noexcept
{
    if (phys_index < 0 || phys_index >= static_cast<int>(g_linux_paths.size()))
        return false;
    if (g_linux_fds[static_cast<std::size_t>(phys_index)] >= 0)
        return true;
    const int fd = ::open(g_linux_paths[static_cast<std::size_t>(phys_index)].path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return false;
    g_linux_fds[static_cast<std::size_t>(phys_index)] = fd;
    return true;
}

static void LinuxDrainJs(int phys_index) noexcept
{
    if (phys_index < 0 || phys_index >= static_cast<int>(g_linux_paths.size()))
        return;
    if (g_linux_paths[static_cast<std::size_t>(phys_index)].is_evdev)
        return;
    const int fd = g_linux_fds[static_cast<std::size_t>(phys_index)];
    if (fd < 0)
        return;
    LinuxLiveState& st = g_linux_live[static_cast<std::size_t>(phys_index)];
    js_event ev{};
    while (::read(fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
        if (ev.type & JS_EVENT_INIT)
            continue;
        if (ev.type == JS_EVENT_BUTTON) {
            if (ev.number >= 0 && ev.number < 64) {
                if (ev.value)
                    st.buttons |= (1ull << static_cast<unsigned>(ev.number));
                else
                    st.buttons &= ~(1ull << static_cast<unsigned>(ev.number));
            }
        } else if (ev.type == JS_EVENT_AXIS) {
            const int ai = static_cast<int>(ev.number);
            if (ai >= 0 && ai < HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes) {
                st.axis_count = std::max(st.axis_count, ai + 1);
                st.axes[static_cast<std::size_t>(ai)] = std::max(-1.f, std::min(1.f, static_cast<float>(ev.value) / 32767.f));
            }
        }
    }
}

static void LinuxSyncEvdevAbsAxes(int phys_index) noexcept
{
    if (phys_index < 0 || phys_index >= static_cast<int>(g_linux_paths.size()))
        return;
    if (!g_linux_paths[static_cast<std::size_t>(phys_index)].is_evdev)
        return;
    const int fd = g_linux_fds[static_cast<std::size_t>(phys_index)];
    if (fd < 0)
        return;
    LinuxLiveState& st = g_linux_live[static_cast<std::size_t>(phys_index)];
    static const int kCodes[] = {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
    for (std::size_t k = 0; k < sizeof(kCodes) / sizeof(kCodes[0]); ++k) {
        struct input_absinfo absinfo {};
        if (::ioctl(fd, EVIOCGABS(kCodes[k]), &absinfo) != 0)
            continue;
        const int ai = static_cast<int>(k);
        if (ai >= HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes)
            break;
        st.axis_count = std::max(st.axis_count, ai + 1);
        const float span = static_cast<float>(absinfo.maximum - absinfo.minimum);
        if (span > 1.f) {
            const float mid = static_cast<float>(absinfo.minimum + absinfo.maximum) * 0.5f;
            st.axes[static_cast<std::size_t>(ai)] = (static_cast<float>(absinfo.value) - mid) / (span * 0.5f);
        } else
            st.axes[static_cast<std::size_t>(ai)] = 0.f;
    }
}

static void LinuxDrainEvdev(int phys_index) noexcept
{
    if (phys_index < 0 || phys_index >= static_cast<int>(g_linux_paths.size()))
        return;
    if (!g_linux_paths[static_cast<std::size_t>(phys_index)].is_evdev)
        return;
    const int fd = g_linux_fds[static_cast<std::size_t>(phys_index)];
    if (fd < 0)
        return;
    LinuxLiveState& st = g_linux_live[static_cast<std::size_t>(phys_index)];
    struct input_event ev {};
    while (::read(fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
        if (ev.type == EV_KEY) {
            const unsigned bit = static_cast<unsigned>(ev.code) & 63u;
            if (ev.value)
                st.buttons |= (1ull << bit);
            else
                st.buttons &= ~(1ull << bit);
        }
    }
    LinuxSyncEvdevAbsAxes(phys_index);
}

static bool LinuxSysfsReadHex(const std::string& sysfs_path, unsigned * out) noexcept
{
    std::ifstream f(sysfs_path.c_str());
    if (!f || !out)
        return false;
    std::string s;
    f >> s;
    if (s.empty())
        return false;
    try {
        *out = static_cast<unsigned>(std::stoul(s, nullptr, 16));
        return true;
    } catch (...) {
        return false;
    }
}

static bool LinuxFillDescriptorFromSysfs(int phys_index, HonkordGL::Joystick::JoystickDescriptor * out) noexcept
{
    if (!out || phys_index < 0 || phys_index >= static_cast<int>(g_linux_paths.size()))
        return false;
    const std::string& devpath = g_linux_paths[static_cast<std::size_t>(phys_index)].path;
    const std::size_t slash = devpath.find_last_of('/');
    if (slash == std::string::npos)
        return false;
    const std::string base = devpath.substr(slash + 1);
    const std::string sysfs = "/sys/class/input/" + base + "/device/id/";
    unsigned vid = 0;
    unsigned pid = 0;
    if (!LinuxSysfsReadHex(sysfs + "vendor", &vid) || !LinuxSysfsReadHex(sysfs + "product", &pid))
        return false;
    out->type = HonkordGL::Joystick::JoystickType::Gamepad;
    out->vendorId = static_cast<std::uint16_t>(vid & 0xFFFFu);
    out->productId = static_cast<std::uint16_t>(pid & 0xFFFFu);
    out->versionBcd = 0;
    out->buttonCount = 16;
    out->axisCount = g_linux_live[static_cast<std::size_t>(phys_index)].axis_count;
    if (out->axisCount <= 0)
        out->axisCount = 8;
    out->hatCount = 1;
    out->touchpadCount = 0;
    out->sensorCount = 0;
    return true;
}
#endif

} // namespace

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    const int pc = PhysicalJsCount();
    const int n = pc + VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    const int pc = PhysicalJsCount();
    return index >= 0 && index < pc + VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualId(index, pc);
    if (index < 0 || index >= pc)
        return {};
    int i = 0;
    for (const auto& dev : g_linux_paths) {
        if (!PathOpenable(dev.path))
            continue;
        if (i == index) {
            JoystickId id{};
            id.value = IdForPath(dev.path);
            return id;
        }
        ++i;
    }
    return {};
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    int i = 0;
    for (const auto& dev : g_linux_paths) {
        if (!PathOpenable(dev.path))
            continue;
        if (i == index) {
            const std::size_t need = dev.path.size() + 1u;
            if (need > buffer_size)
                return false;
            std::memcpy(buffer, dev.path.c_str(), need);
            return true;
        }
        ++i;
    }
    return false;
}

bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    int i = 0;
    for (const auto& dev : g_linux_paths) {
        if (!PathOpenable(dev.path))
            continue;
        if (i == index)
            return CopyJoystickNameForPath(dev, buffer, buffer_size);
        ++i;
    }
    return false;
}

bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualGetBatteryImpl(index, pc, out);
    return false;
}

int GetJoystickPlayerIndex(int index) noexcept
{
    if (index < 0)
        return -1;
    if (!IsJoystickConnected(index))
        return -1;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPlayerIndex(index, pc);
    return index;
}

bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        return VirtualOpen(index, pc);
#if defined(__linux__)
    {
        int i = 0;
        for (std::size_t si = 0; si < g_linux_paths.size(); ++si) {
            if (!PathOpenable(g_linux_paths[si].path))
                continue;
            if (i == index)
                return LinuxEnsureFd(static_cast<int>(si));
            ++i;
        }
    }
#endif
    return true;
}

void CloseJoystick(int index) noexcept
{
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc))
        VirtualClose(index, pc);
    else {
#if defined(__linux__)
        int i = 0;
        for (std::size_t si = 0; si < g_linux_paths.size(); ++si) {
            if (!PathOpenable(g_linux_paths[si].path))
                continue;
            if (i == index) {
                int& fd = g_linux_fds[si];
                if (fd >= 0) {
                    ::close(fd);
                    fd = -1;
                }
                break;
            }
            ++i;
        }
#endif
    }
}

void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
    LinuxCloseAllFds();
}

bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return VirtualSetLedColorImpl(index, PhysicalJsCount(), r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    return VirtualStartTriggerRumbleImpl(index, PhysicalJsCount(), left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    VirtualStopTriggerRumbleImpl(index, PhysicalJsCount());
}

bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    const int pc = PhysicalJsCount();
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, pc, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}

void CloseJoystickById(JoystickId id) noexcept
{
    VirtualCloseByIdImpl(id, PhysicalJsCount());
}

int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    return PushVirtual(descriptor, label, PhysicalJsCount());
}

void DetachVirtualJoystick(int index) noexcept
{
    (void)VirtualRemove(index, PhysicalJsCount());
}

bool IsVirtualJoystick(int index) noexcept
{
    return VirtualIsIndex(index, PhysicalJsCount());
}

bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}

bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}

bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}

bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}

bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}

bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e)
            return false;
        *out = e->desc;
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    int i = 0;
    for (std::size_t si = 0; si < g_linux_paths.size(); ++si) {
        if (!PathOpenable(g_linux_paths[si].path))
            continue;
        if (i != index) {
            ++i;
            continue;
        }
#if defined(__linux__)
        if (LinuxFillDescriptorFromSysfs(static_cast<int>(si), out))
            return true;
#endif
        out->type = JoystickType::Gamepad;
        out->vendorId = 0;
        out->productId = 0;
        out->versionBcd = 0;
        if (!g_linux_paths[si].is_evdev) {
            const int tfd = ::open(g_linux_paths[si].path.c_str(), O_RDONLY | O_NONBLOCK);
            if (tfd >= 0) {
                char buttons = 0;
                char axes = 0;
                (void)::ioctl(tfd, JSIOCGBUTTONS, &buttons);
                (void)::ioctl(tfd, JSIOCGAXES, &axes);
                ::close(tfd);
                out->buttonCount = static_cast<int>(static_cast<unsigned char>(buttons));
                out->axisCount = static_cast<int>(static_cast<unsigned char>(axes));
            } else {
                out->buttonCount = 0;
                out->axisCount = 0;
            }
        } else {
            out->buttonCount = 16;
            out->axisCount = 8;
        }
        out->hatCount = 1;
        out->touchpadCount = 0;
        out->sensorCount = 0;
        return true;
    }
    return false;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalJsCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e || g_virtual_open.count(e->id) == 0)
            return false;
        std::memset(out, 0, sizeof(*out));
        const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        out->axis_count = na;
        for (int ai = 0; ai < na; ++ai)
            out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
#if defined(__linux__)
    int i = 0;
    for (std::size_t si = 0; si < g_linux_paths.size(); ++si) {
        if (!PathOpenable(g_linux_paths[si].path))
            continue;
        if (i != index) {
            ++i;
            continue;
        }
        if (g_linux_fds[si] < 0)
            return false;
        if (g_linux_paths[si].is_evdev)
            LinuxDrainEvdev(static_cast<int>(si));
        else
            LinuxDrainJs(static_cast<int>(si));
        const LinuxLiveState& st = g_linux_live[si];
        std::memset(out, 0, sizeof(*out));
        out->axis_count = std::min(st.axis_count, HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        for (int a = 0; a < out->axis_count; ++a)
            out->axes[static_cast<std::size_t>(a)] = st.axes[static_cast<std::size_t>(a)];
        if (g_linux_paths[si].is_evdev && out->axis_count > 7) {
            const float hx = st.axes[6];
            const float hy = st.axes[7];
            out->hat_x = hx > 0.5f ? 1 : (hx < -0.5f ? -1 : 0);
            out->hat_y = hy > 0.5f ? 1 : (hy < -0.5f ? -1 : 0);
        }
        out->buttons_pressed_mask = st.buttons;
        out->button_count = 64;
        return true;
    }
#endif
    return false;
}

} // namespace HonkordGL::Joystick

#elif defined(__ANDROID__)

#include <HonkordGL/AndroidNativeApp.h>

#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>
#include <jni.h>

namespace {

struct AndroidMotionSnap {
    float axes[6]{};
    int hat_x{0};
    int hat_y{0};
    std::uint64_t buttons{0};
};

std::vector<int> g_android_device_ids;
std::vector<std::string> g_android_descriptors;
std::vector<std::string> g_android_names;
std::vector<int> g_android_controller_numbers;
std::vector<int> g_android_battery_percent;
std::vector<int> g_android_battery_power;
std::vector<std::uint16_t> g_android_vendor_ids;
std::vector<std::uint16_t> g_android_product_ids;
std::unordered_set<int> g_android_open_ids;
std::unordered_map<int, AndroidMotionSnap> g_android_motion_by_device;

constexpr jint kAndroidPollKeyCodes[] = { AKEYCODE_BUTTON_A, AKEYCODE_BUTTON_B, AKEYCODE_BUTTON_X, AKEYCODE_BUTTON_Y, AKEYCODE_BUTTON_L1, AKEYCODE_BUTTON_R1,
    AKEYCODE_BUTTON_SELECT, AKEYCODE_BUTTON_START, AKEYCODE_BUTTON_MODE, AKEYCODE_BUTTON_THUMBL, AKEYCODE_BUTTON_THUMBR, AKEYCODE_DPAD_UP, AKEYCODE_DPAD_DOWN,
    AKEYCODE_DPAD_LEFT, AKEYCODE_DPAD_RIGHT };

bool AndroidPollPhysicalSnapshot(int index, int physical_count, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out || index < 0 || index >= physical_count)
        return false;
    const int devId = g_android_device_ids[static_cast<std::size_t>(index)];
    if (g_android_open_ids.count(devId) == 0)
        return false;

    std::memset(out, 0, sizeof(*out));
    out->axis_count = 6;
    out->button_count = static_cast<int>(sizeof(kAndroidPollKeyCodes) / sizeof(kAndroidPollKeyCodes[0]));
    const auto mit = g_android_motion_by_device.find(devId);
    if (mit != g_android_motion_by_device.end()) {
        for (int i = 0; i < 6; ++i)
            out->axes[static_cast<std::size_t>(i)] = mit->second.axes[i];
        out->hat_x = mit->second.hat_x;
        out->hat_y = mit->second.hat_y;
        out->buttons_pressed_mask = mit->second.buttons;
    }
    return true;
}

void RefreshAndroidDevices() noexcept
{
    g_android_device_ids.clear();
    g_android_descriptors.clear();
    g_android_names.clear();
    g_android_controller_numbers.clear();
    g_android_battery_percent.clear();
    g_android_battery_power.clear();
    g_android_vendor_ids.clear();
    g_android_product_ids.clear();

    android_app * const app = HonkordGL::Graphics::Android::GetNativeApp();
    if (!app || !app->activity)
        return;

    JNIEnv * const env = app->activity->env;
    jobject const activity = app->activity->clazz;
    if (!env || !activity)
        return;

    if (env->ExceptionCheck())
        env->ExceptionClear();

    jclass const contextClass = env->FindClass("android/content/Context");
    if (!contextClass)
        return;
    jfieldID const fid = env->GetStaticFieldID(contextClass, "INPUT_SERVICE", "Ljava/lang/String;");
    if (!fid) {
        env->DeleteLocalRef(contextClass);
        return;
    }
    jstring const inputServiceName = static_cast<jstring>(env->GetStaticObjectField(contextClass, fid));
    if (!inputServiceName) {
        env->DeleteLocalRef(contextClass);
        return;
    }

    jclass const actClass = env->GetObjectClass(activity);
    jmethodID const getSys = env->GetMethodID(actClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!getSys) {
        env->DeleteLocalRef(inputServiceName);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }

    jobject const inputMgr = env->CallObjectMethod(activity, getSys, inputServiceName);
    env->DeleteLocalRef(inputServiceName);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }
    if (!inputMgr) {
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }

    jclass const imClass = env->FindClass("android/view/InputManager");
    jclass const idClass = env->FindClass("android/view/InputDevice");
    if (!imClass || !idClass) {
        env->DeleteLocalRef(inputMgr);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        if (imClass)
            env->DeleteLocalRef(imClass);
        if (idClass)
            env->DeleteLocalRef(idClass);
        return;
    }

    jmethodID const getIds = env->GetMethodID(imClass, "getInputDeviceIds", "()[I");
    jmethodID const getDev = env->GetMethodID(imClass, "getInputDevice", "(I)Landroid/view/InputDevice;");
    jmethodID const getSources = env->GetMethodID(idClass, "getSources", "()I");
    jmethodID const getDesc = env->GetMethodID(idClass, "getDescriptor", "()Ljava/lang/String;");
    jmethodID const getCtrlNum = env->GetMethodID(idClass, "getControllerNumber", "()I");
    jmethodID const getName = env->GetMethodID(idClass, "getName", "()Ljava/lang/String;");
    if (!getIds || !getDev || !getSources) {
        env->DeleteLocalRef(imClass);
        env->DeleteLocalRef(idClass);
        env->DeleteLocalRef(inputMgr);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }

    jintArray const idArr = static_cast<jintArray>(env->CallObjectMethod(inputMgr, getIds));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(imClass);
        env->DeleteLocalRef(idClass);
        env->DeleteLocalRef(inputMgr);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }
    if (!idArr) {
        env->DeleteLocalRef(imClass);
        env->DeleteLocalRef(idClass);
        env->DeleteLocalRef(inputMgr);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }

    const jsize n = env->GetArrayLength(idArr);
    jint * const ids = env->GetIntArrayElements(idArr, nullptr);
    if (!ids) {
        env->DeleteLocalRef(idArr);
        env->DeleteLocalRef(imClass);
        env->DeleteLocalRef(idClass);
        env->DeleteLocalRef(inputMgr);
        env->DeleteLocalRef(actClass);
        env->DeleteLocalRef(contextClass);
        return;
    }

    constexpr jint kSrcGamepad = 0x00000401;
    constexpr jint kSrcJoystick = static_cast<jint>(AINPUT_SOURCE_JOYSTICK);

    for (jsize i = 0; i < n; ++i) {
        const jint devId = ids[i];
        jobject const devObj = env->CallObjectMethod(inputMgr, getDev, devId);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            continue;
        }
        if (!devObj)
            continue;

        const jint sources = env->CallIntMethod(devObj, getSources);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(devObj);
            continue;
        }

        const bool hasGamepadBits = (sources & kSrcGamepad) != 0;
        const bool hasJoystickBits = (sources & kSrcJoystick) != 0;
        if (!hasGamepadBits && !hasJoystickBits) {
            env->DeleteLocalRef(devObj);
            continue;
        }

        std::string desc;
        if (getDesc) {
            jstring const jdesc = static_cast<jstring>(env->CallObjectMethod(devObj, getDesc));
            if (jdesc && !env->ExceptionCheck()) {
                const char * const utf = env->GetStringUTFChars(jdesc, nullptr);
                if (utf) {
                    desc.assign(utf);
                    env->ReleaseStringUTFChars(jdesc, utf);
                }
                env->DeleteLocalRef(jdesc);
            } else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
        if (desc.empty()) {
            desc = "android:";
            desc += std::to_string(static_cast<int>(devId));
        }

        std::string displayName;
        if (getName) {
            jstring const jnm = static_cast<jstring>(env->CallObjectMethod(devObj, getName));
            if (jnm && !env->ExceptionCheck()) {
                const char * const nutf = env->GetStringUTFChars(jnm, nullptr);
                if (nutf) {
                    displayName.assign(nutf);
                    env->ReleaseStringUTFChars(jnm, nutf);
                }
                env->DeleteLocalRef(jnm);
            } else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
        if (displayName.empty())
            displayName = desc;

        int ctrlNum = -1;
        if (getCtrlNum) {
            const jint cn = env->CallIntMethod(devObj, getCtrlNum);
            if (!env->ExceptionCheck())
                ctrlNum = static_cast<int>(cn);
            else
                env->ExceptionClear();
        }

        int batPct = -1;
        int batPowInt = static_cast<int>(HonkordGL::Joystick::JoystickPowerState::Unknown);
        {
            jmethodID const getBatteryLevel = env->GetMethodID(idClass, "getBatteryLevel", "()F");
            jmethodID const getBatteryState = env->GetMethodID(idClass, "getBatteryState", "()I");
            if (getBatteryLevel) {
                const jfloat blv = env->CallFloatMethod(devObj, getBatteryLevel);
                if (!env->ExceptionCheck()) {
                    if (blv >= 0.f && blv <= 1.f)
                        batPct = static_cast<int>(blv * 100.f + 0.5f);
                } else {
                    env->ExceptionClear();
                }
            }
            if (getBatteryState) {
                const jint bst = env->CallIntMethod(devObj, getBatteryState);
                if (!env->ExceptionCheck()) {
                    using HonkordGL::Joystick::JoystickPowerState;
                    switch (bst) {
                    case 1:
                        batPowInt = static_cast<int>(JoystickPowerState::Charging);
                        break;
                    case 2:
                        batPowInt = static_cast<int>(JoystickPowerState::Discharging);
                        break;
                    case 3:
                        batPowInt = static_cast<int>((batPct >= 90) ? JoystickPowerState::Full : JoystickPowerState::Discharging);
                        break;
                    case 4:
                        batPowInt = static_cast<int>(JoystickPowerState::Full);
                        if (batPct < 0)
                            batPct = 100;
                        break;
                    default:
                        break;
                    }
                } else {
                    env->ExceptionClear();
                }
            }
        }

        std::uint16_t vid16 = 0;
        std::uint16_t pid16 = 0;
        jmethodID const getVendorId = env->GetMethodID(idClass, "getVendorId", "()I");
        jmethodID const getProductId = env->GetMethodID(idClass, "getProductId", "()I");
        if (getVendorId && getProductId) {
            const jint vj = env->CallIntMethod(devObj, getVendorId);
            if (!env->ExceptionCheck())
                vid16 = static_cast<std::uint16_t>(vj & 0xFFFF);
            else
                env->ExceptionClear();
            const jint pj = env->CallIntMethod(devObj, getProductId);
            if (!env->ExceptionCheck())
                pid16 = static_cast<std::uint16_t>(pj & 0xFFFF);
            else
                env->ExceptionClear();
        }

        env->DeleteLocalRef(devObj);

        g_android_device_ids.push_back(static_cast<int>(devId));
        g_android_descriptors.push_back(std::move(desc));
        g_android_names.push_back(std::move(displayName));
        g_android_controller_numbers.push_back(ctrlNum);
        g_android_battery_percent.push_back(batPct);
        g_android_battery_power.push_back(batPowInt);
        g_android_vendor_ids.push_back(vid16);
        g_android_product_ids.push_back(pid16);
    }

    env->ReleaseIntArrayElements(idArr, ids, JNI_ABORT);
    env->DeleteLocalRef(idArr);
    env->DeleteLocalRef(imClass);
    env->DeleteLocalRef(idClass);
    env->DeleteLocalRef(inputMgr);
    env->DeleteLocalRef(actClass);
    env->DeleteLocalRef(contextClass);
}

} // namespace

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    RefreshAndroidDevices();
    const int n = static_cast<int>(g_android_device_ids.size()) + VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}

bool IsJoystickConnected(int index) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    return index >= 0 && index < pc + VirtualCount();
}

JoystickId GetJoystickId(int index) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualId(index, pc);
    if (index < 0 || index >= pc)
        return {};
    JoystickId id{};
    id.value = static_cast<std::uint64_t>(static_cast<std::uint32_t>(g_android_device_ids[static_cast<std::size_t>(index)]));
    return id;
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= static_cast<int>(g_android_descriptors.size()))
        return false;
    const std::string& s = g_android_descriptors[static_cast<std::size_t>(index)];
    if (s.size() + 1 > buffer_size)
        return false;
    std::memcpy(buffer, s.c_str(), s.size() + 1);
    return true;
}

bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= static_cast<int>(g_android_names.size()))
        return false;
    const std::string& s = g_android_names[static_cast<std::size_t>(index)];
    if (s.size() + 1 > buffer_size)
        return false;
    std::memcpy(buffer, s.c_str(), s.size() + 1);
    return true;
}

bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualGetBatteryImpl(index, pc, out);
    if (index < 0 || index >= static_cast<int>(g_android_battery_percent.size()))
        return false;
    out->level_percent = g_android_battery_percent[static_cast<std::size_t>(index)];
    out->power = static_cast<JoystickPowerState>(g_android_battery_power[static_cast<std::size_t>(index)]);
    return (out->power != JoystickPowerState::Unknown) || (out->level_percent >= 0);
}

int GetJoystickPlayerIndex(int index) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualPlayerIndex(index, pc);
    if (index < 0 || index >= static_cast<int>(g_android_controller_numbers.size()))
        return -1;
    const int cn = g_android_controller_numbers[static_cast<std::size_t>(index)];
    if (cn > 0)
        return cn - 1;
    return index;
}
bool OpenJoystick(int index) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc))
        return VirtualOpen(index, pc);
    if (index < 0 || index >= pc)
        return false;
    g_android_open_ids.insert(g_android_device_ids[static_cast<std::size_t>(index)]);
    return true;
}
void CloseJoystick(int index) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc)) {
        VirtualClose(index, pc);
        return;
    }
    if (index < 0 || index >= pc)
        return;
    g_android_open_ids.erase(g_android_device_ids[static_cast<std::size_t>(index)]);
}
void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
    g_android_open_ids.clear();
    g_android_motion_by_device.clear();
}
bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    RefreshAndroidDevices();
    return VirtualSetLedColorImpl(index, static_cast<int>(g_android_device_ids.size()), r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    RefreshAndroidDevices();
    return VirtualStartTriggerRumbleImpl(index, static_cast<int>(g_android_device_ids.size()), left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    RefreshAndroidDevices();
    VirtualStopTriggerRumbleImpl(index, static_cast<int>(g_android_device_ids.size()));
}
bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, pc, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}
void CloseJoystickById(JoystickId id) noexcept
{
    RefreshAndroidDevices();
    VirtualCloseByIdImpl(id, static_cast<int>(g_android_device_ids.size()));
}

int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    RefreshAndroidDevices();
    return PushVirtual(descriptor, label, static_cast<int>(g_android_device_ids.size()));
}
void DetachVirtualJoystick(int index) noexcept
{
    RefreshAndroidDevices();
    (void)VirtualRemove(index, static_cast<int>(g_android_device_ids.size()));
}
bool IsVirtualJoystick(int index) noexcept
{
    RefreshAndroidDevices();
    return VirtualIsIndex(index, static_cast<int>(g_android_device_ids.size()));
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}

bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}

bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}

bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e)
            return false;
        *out = e->desc;
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    std::memset(out, 0, sizeof(*out));
    out->type = JoystickType::Gamepad;
    out->buttonCount = 14;
    out->axisCount = 6;
    out->hatCount = 1;
    if (index < static_cast<int>(g_android_vendor_ids.size()))
        out->vendorId = g_android_vendor_ids[static_cast<std::size_t>(index)];
    if (index < static_cast<int>(g_android_product_ids.size()))
        out->productId = g_android_product_ids[static_cast<std::size_t>(index)];
    return true;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    RefreshAndroidDevices();
    const int pc = static_cast<int>(g_android_device_ids.size());
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e || g_virtual_open.count(e->id) == 0)
            return false;
        std::memset(out, 0, sizeof(*out));
        const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        out->axis_count = na;
        for (int ai = 0; ai < na; ++ai)
            out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
        return true;
    }
    return AndroidPollPhysicalSnapshot(index, pc, out);
}

void FeedAndroidJoystickMotionEvent(void const * ainput_event) noexcept
{
    auto * const ev = static_cast<AInputEvent const *>(ainput_event);
    if (!ev || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION)
        return;
    const int devId = AInputEvent_getDeviceId(ev);
    AndroidMotionSnap snap = g_android_motion_by_device[devId];
    auto ax = [&](int axis) { return AMotionEvent_getAxisValue(ev, axis, 0); };
    snap.axes[0] = ax(AMOTION_EVENT_AXIS_X);
    snap.axes[1] = ax(AMOTION_EVENT_AXIS_Y);
    snap.axes[2] = ax(AMOTION_EVENT_AXIS_RX);
    if (snap.axes[2] == 0.f)
        snap.axes[2] = ax(AMOTION_EVENT_AXIS_Z);
    snap.axes[3] = ax(AMOTION_EVENT_AXIS_RY);
    if (snap.axes[3] == 0.f)
        snap.axes[3] = ax(AMOTION_EVENT_AXIS_RZ);
    snap.axes[4] = ax(AMOTION_EVENT_AXIS_LTRIGGER);
    if (snap.axes[4] == 0.f)
        snap.axes[4] = ax(AMOTION_EVENT_AXIS_BRAKE);
    snap.axes[5] = ax(AMOTION_EVENT_AXIS_RTRIGGER);
    if (snap.axes[5] == 0.f)
        snap.axes[5] = ax(AMOTION_EVENT_AXIS_GAS);
    const float hx = ax(AMOTION_EVENT_AXIS_HAT_X);
    const float hy = ax(AMOTION_EVENT_AXIS_HAT_Y);
    snap.hat_x = std::abs(hx) > 0.5f ? (hx > 0.f ? 1 : -1) : 0;
    snap.hat_y = std::abs(hy) > 0.5f ? (hy > 0.f ? 1 : -1) : 0;
    g_android_motion_by_device[devId] = snap;
}

void FeedAndroidJoystickKeyEvent(void const * ainput_event) noexcept
{
    auto * const ev = static_cast<AInputEvent const *>(ainput_event);
    if (!ev || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_KEY)
        return;
    const int devId = AInputEvent_getDeviceId(ev);
    const int key = AKeyEvent_getKeyCode(ev);
    const int action = AKeyEvent_getAction(ev);
    int bit_index = -1;
    for (int i = 0; i < static_cast<int>(sizeof(kAndroidPollKeyCodes) / sizeof(kAndroidPollKeyCodes[0])); ++i) {
        if (kAndroidPollKeyCodes[i] == key) {
            bit_index = i;
            break;
        }
    }
    if (bit_index < 0 || bit_index >= 64)
        return;
    AndroidMotionSnap snap = g_android_motion_by_device[devId];
    const std::uint64_t bit = (1ull << static_cast<unsigned>(bit_index));
    if (action == AKEY_EVENT_ACTION_UP)
        snap.buttons &= ~bit;
    else if (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_MULTIPLE)
        snap.buttons |= bit;
    g_android_motion_by_device[devId] = snap;
}

} // namespace HonkordGL::Joystick

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include <cstdio>

namespace {

struct MacHidSlot {
    IOHIDDeviceRef device{};
};

std::vector<MacHidSlot> g_mac_slots;
std::unordered_set<std::uint64_t> g_mac_open_ids;

void MacDecodePov(int v, int * hx, int * hy) noexcept
{
    if (!hx || !hy)
        return;
    *hx = *hy = 0;
    if (v < 0 || v > 8)
        return;
    static const int dx[] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dy[] = { 0, 1, 1, 0, -1, -1, -1, 0, 1 };
    *hx = dx[static_cast<std::size_t>(v)];
    *hy = dy[static_cast<std::size_t>(v)];
}

void MacPollHidSnapshot(IOHIDDeviceRef dev, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!dev || !out)
        return;
    std::memset(out, 0, sizeof(*out));
    CFArrayRef const elems = IOHIDDeviceCopyMatchingElements(dev, nullptr, kIOHIDOptionsTypeNone);
    if (!elems)
        return;
    const CFIndex n = CFArrayGetCount(elems);
    std::uint64_t mask = 0;
    int max_btn_usage = 0;
    float axis_vals[6]{};
    int hat_value = -1;
    for (CFIndex i = 0; i < n; ++i) {
        auto * const el = static_cast<IOHIDElementRef>(CFArrayGetValueAtIndex(elems, i));
        const uint32_t type = IOHIDElementGetType(el);
        if (type != kIOHIDElementTypeInput_Button && type != kIOHIDElementTypeInput_Axis && type != kIOHIDElementTypeInput_Misc
            && type != kIOHIDElementTypeInput_Scaler)
            continue;
        IOHIDValueRef const val = IOHIDDeviceGetValue(dev, el);
        if (!val)
            continue;
        const uint32_t page = IOHIDElementGetUsagePage(el);
        const uint32_t usage = IOHIDElementGetUsage(el);
        const CFIndex iv = IOHIDValueGetIntegerValue(val);
        if (page == kHIDPage_Button && usage >= 1u && usage <= 64u) {
            if (iv != 0)
                mask |= (1ull << static_cast<unsigned>(usage - 1u));
            max_btn_usage = std::max(max_btn_usage, static_cast<int>(usage));
        } else if (page == kHIDPage_GenericDesktop) {
            if (usage == kHIDUsage_GD_Hatswitch) {
                const CFIndex hlo = IOHIDElementGetLogicalMin(el);
                hat_value = static_cast<int>(iv - hlo);
                if (hat_value < 0 || hat_value > 8)
                    hat_value = 0;
                continue;
            }
            const CFIndex lo = IOHIDElementGetLogicalMin(el);
            const CFIndex hi = IOHIDElementGetLogicalMax(el);
            if (hi <= lo)
                continue;
            float f = 0.f;
            if (lo >= 0 && (usage == kHIDUsage_GD_Z || usage == kHIDUsage_GD_Rz))
                f = static_cast<float>(iv - lo) / static_cast<float>(hi - lo);
            else
                f = (2.f * static_cast<float>(iv - lo) / static_cast<float>(hi - lo)) - 1.f;
            int slot = -1;
            if (usage == kHIDUsage_GD_X)
                slot = 0;
            else if (usage == kHIDUsage_GD_Y)
                slot = 1;
            else if (usage == kHIDUsage_GD_Rx)
                slot = 2;
            else if (usage == kHIDUsage_GD_Ry)
                slot = 3;
            else if (usage == kHIDUsage_GD_Z)
                slot = 4;
            else if (usage == kHIDUsage_GD_Rz)
                slot = 5;
            if (slot >= 0 && slot < 6)
                axis_vals[static_cast<std::size_t>(slot)] = f;
        }
    }
    CFRelease(elems);
    out->axis_count = 6;
    for (int s = 0; s < 6; ++s)
        out->axes[static_cast<std::size_t>(s)] = axis_vals[static_cast<std::size_t>(s)];
    if (hat_value >= 0)
        MacDecodePov(hat_value, &out->hat_x, &out->hat_y);
    out->buttons_pressed_mask = mask;
    out->button_count = mask != 0 ? std::min(64, std::max(max_btn_usage, 1)) : 0;
}

bool MacFillPhysicalDescriptor(IOHIDDeviceRef dev, HonkordGL::Joystick::JoystickDescriptor * out) noexcept
{
    if (!dev || !out)
        return false;
    std::memset(out, 0, sizeof(*out));
    out->type = HonkordGL::Joystick::JoystickType::Gamepad;
    int64_t vid = 0;
    int64_t pid = 0;
    CFTypeRef o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDVendorIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &vid);
    o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDProductIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &pid);
    out->vendorId = static_cast<std::uint16_t>(vid & 0xFFFF);
    out->productId = static_cast<std::uint16_t>(pid & 0xFFFF);
    out->buttonCount = 32;
    out->axisCount = 12;
    out->hatCount = 1;
    return true;
}

void MacReleaseSlots() noexcept
{
    for (auto& s : g_mac_slots) {
        if (s.device) {
            CFRelease(s.device);
            s.device = nullptr;
        }
    }
    g_mac_slots.clear();
}

IOHIDManagerRef MacHidManagerInstance() noexcept
{
    static IOHIDManagerRef mgr = nullptr;
    if (mgr)
        return mgr;
    mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!mgr)
        return nullptr;

    CFMutableArrayRef criteria = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!criteria) {
        CFRelease(mgr);
        mgr = nullptr;
        return nullptr;
    }

    const int page = kHIDPage_GenericDesktop;
    const int usages[] = {kHIDUsage_GD_Joystick, kHIDUsage_GD_GamePad, kHIDUsage_GD_MultiAxisController};
    for (int usage : usages) {
        CFMutableDictionaryRef match = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!match)
            continue;
        CFNumberRef p = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
        CFNumberRef u = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
        if (p && u) {
            CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsagePageKey), p);
            CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsageKey), u);
        }
        if (p)
            CFRelease(p);
        if (u)
            CFRelease(u);
        CFArrayAppendValue(criteria, match);
        CFRelease(match);
    }

    IOHIDManagerSetDeviceMatchingMultiple(mgr, criteria);
    CFRelease(criteria);

    if (IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        CFRelease(mgr);
        mgr = nullptr;
    }
    return mgr;
}

std::uint64_t MacIdForDevice(IOHIDDeviceRef dev) noexcept
{
    int64_t vid = 0;
    int64_t pid = 0;
    int64_t loc = 0;
    CFTypeRef o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDVendorIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &vid);
    o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDProductIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &pid);
    o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDLocationIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &loc);
    std::uint64_t v = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(vid) & 0xFFFFu) << 48)
        | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(pid) & 0xFFFFu) << 32)
        | static_cast<std::uint64_t>(static_cast<std::uint32_t>(loc));
    if (v == 0) {
        std::uint64_t regId = 0;
        io_service_t const svc = IOHIDDeviceGetService(dev);
        if (svc && IORegistryEntryGetRegistryEntryID(svc, &regId) == kIOReturnSuccess)
            v = regId;
    }
    if (v != 0)
        v |= 0x4D41430000000000ULL;
    return v;
}

void RefreshMacJoysticks() noexcept
{
    MacReleaseSlots();
    IOHIDManagerRef const mgr = MacHidManagerInstance();
    if (!mgr)
        return;
    CFSetRef const set = IOHIDManagerCopyDevices(mgr);
    if (!set)
        return;
    const CFIndex n = CFSetGetCount(set);
    std::vector<const void *> vals;
    if (n > 0) {
        vals.resize(static_cast<std::size_t>(n));
        CFSetGetValues(set, vals.data());
    }
    CFRelease(set);
    for (CFIndex i = 0; i < n; ++i) {
        auto * const dev = reinterpret_cast<IOHIDDeviceRef>(const_cast<void *>(vals[static_cast<std::size_t>(i)]));
        if (!dev)
            continue;
        CFRetain(dev);
        g_mac_slots.push_back(MacHidSlot{dev});
    }
}

int PhysicalMacCount() noexcept
{
    RefreshMacJoysticks();
    return static_cast<int>(g_mac_slots.size());
}

bool MacCopyProductName(IOHIDDeviceRef dev, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    CFTypeRef const o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDProductKey));
    if (o && CFGetTypeID(o) == CFStringGetTypeID()) {
        if (CFStringGetCString(static_cast<CFStringRef>(o), buffer, buffer_size, kCFStringEncodingUTF8))
            return true;
    }
    static const char kFallback[] = "HID Controller";
    if (sizeof(kFallback) > buffer_size)
        return false;
    std::memcpy(buffer, kFallback, sizeof(kFallback));
    return true;
}

} // namespace

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    const int n = PhysicalMacCount() + VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    const int pc = PhysicalMacCount();
    return index >= 0 && index < pc + VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualId(index, pc);
    if (index < 0 || index >= pc)
        return {};
    IOHIDDeviceRef const dev = g_mac_slots[static_cast<std::size_t>(index)].device;
    JoystickId id{};
    id.value = MacIdForDevice(dev);
    return id;
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    IOHIDDeviceRef const dev = g_mac_slots[static_cast<std::size_t>(index)].device;
    int64_t loc = 0;
    CFTypeRef const o = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDLocationIDKey));
    if (o && CFGetTypeID(o) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(o), kCFNumberSInt64Type, &loc);
    const int nw = std::snprintf(buffer, buffer_size, "mac-hid:%08llx", static_cast<unsigned long long>(loc & 0xFFFFFFFFLL));
    if (nw < 0 || static_cast<std::size_t>(nw) >= buffer_size)
        return false;
    return true;
}
bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    if (!buffer || buffer_size == 0)
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPath(index, pc, buffer, buffer_size);
    if (index < 0 || index >= pc)
        return false;
    return MacCopyProductName(g_mac_slots[static_cast<std::size_t>(index)].device, buffer, buffer_size);
}
bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualGetBatteryImpl(index, pc, out);
    return false;
}
int GetJoystickPlayerIndex(int index) noexcept
{
    if (index < 0)
        return -1;
    if (!IsJoystickConnected(index))
        return -1;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualPlayerIndex(index, pc);
    return index;
}
bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc))
        return VirtualOpen(index, pc);
    if (index < 0 || index >= pc)
        return false;
    IOHIDDeviceRef const dev = g_mac_slots[static_cast<std::size_t>(index)].device;
    g_mac_open_ids.insert(MacIdForDevice(dev));
    return true;
}
void CloseJoystick(int index) noexcept
{
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc)) {
        VirtualClose(index, pc);
        return;
    }
    if (index >= 0 && index < pc) {
        IOHIDDeviceRef const dev = g_mac_slots[static_cast<std::size_t>(index)].device;
        g_mac_open_ids.erase(MacIdForDevice(dev));
    }
}
void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
    g_mac_open_ids.clear();
}
bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return VirtualSetLedColorImpl(index, PhysicalMacCount(), r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    return VirtualStartTriggerRumbleImpl(index, PhysicalMacCount(), left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    VirtualStopTriggerRumbleImpl(index, PhysicalMacCount());
}
bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    const int pc = PhysicalMacCount();
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, pc, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}
void CloseJoystickById(JoystickId id) noexcept
{
    VirtualCloseByIdImpl(id, PhysicalMacCount());
}
int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    return PushVirtual(descriptor, label, PhysicalMacCount());
}
void DetachVirtualJoystick(int index) noexcept
{
    (void)VirtualRemove(index, PhysicalMacCount());
}
bool IsVirtualJoystick(int index) noexcept
{
    return VirtualIsIndex(index, PhysicalMacCount());
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}
bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}
bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}
bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e)
            return false;
        *out = e->desc;
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    return MacFillPhysicalDescriptor(g_mac_slots[static_cast<std::size_t>(index)].device, out);
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    const int pc = PhysicalMacCount();
    if (VirtualIsIndex(index, pc)) {
        const VirtualEntry * const e = VirtualPtr(index, pc);
        if (!e || g_virtual_open.count(e->id) == 0)
            return false;
        std::memset(out, 0, sizeof(*out));
        const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
        out->axis_count = na;
        for (int ai = 0; ai < na; ++ai)
            out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
        return true;
    }
    if (index < 0 || index >= pc)
        return false;
    IOHIDDeviceRef const dev = g_mac_slots[static_cast<std::size_t>(index)].device;
    if (g_mac_open_ids.count(MacIdForDevice(dev)) == 0)
        return false;
    MacPollHidSnapshot(dev, out);
    return true;
}

} // namespace HonkordGL::Joystick

#else

namespace HonkordGL::Joystick {

int GetJoystickCount() noexcept
{
    const int n = VirtualCount();
    if (g_hotplug_cb && !g_hotplug_explicit_mode && g_joystick_hotplug_tls_depth == 0) {
        ++g_joystick_hotplug_tls_depth;
        (void)EmitJoystickHotplugDeltas(n);
        --g_joystick_hotplug_tls_depth;
    }
    return n;
}
bool IsJoystickConnected(int index) noexcept
{
    return index >= 0 && index < VirtualCount();
}
JoystickId GetJoystickId(int index) noexcept
{
    return VirtualId(index, 0);
}
bool GetJoystickPath(int index, char * buffer, std::size_t buffer_size) noexcept
{
    return VirtualPath(index, 0, buffer, buffer_size);
}
bool GetJoystickName(int index, char * buffer, std::size_t buffer_size) noexcept
{
    return VirtualPath(index, 0, buffer, buffer_size);
}
bool GetJoystickBattery(int index, JoystickBatteryInfo * out) noexcept
{
    if (!out)
        return false;
    return VirtualGetBatteryImpl(index, 0, out);
}
int GetJoystickPlayerIndex(int index) noexcept
{
    return VirtualPlayerIndex(index, 0);
}
bool OpenJoystick(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    return VirtualOpen(index, 0);
}
void CloseJoystick(int index) noexcept
{
    VirtualClose(index, 0);
}
void CloseAllJoysticks() noexcept
{
    VirtualClearOpen();
}
bool SetJoystickLedColor(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return VirtualSetLedColorImpl(index, 0, r, g, b);
}
bool StartJoystickTriggerRumble(int index, float left, float right) noexcept
{
    return VirtualStartTriggerRumbleImpl(index, 0, left, right);
}
void StopJoystickTriggerRumble(int index) noexcept
{
    VirtualStopTriggerRumbleImpl(index, 0);
}
bool SendJoystickOutputPacket(int index, const JoystickOutputPacket * packet) noexcept
{
    if (!packet)
        return false;
    const bool ok = VirtualSendOutputPacketImpl(index, 0, packet);
    if (packet->flags & static_cast<std::uint32_t>(HonkordGL::Joystick::JoystickOutputPacketFlag_CloseAfter))
        CloseJoystick(index);
    return ok;
}
void CloseJoystickById(JoystickId id) noexcept
{
    VirtualCloseByIdImpl(id, 0);
}
int AttachVirtualJoystick(const JoystickDescriptor * descriptor, const char * label) noexcept
{
    return PushVirtual(descriptor, label, 0);
}
void DetachVirtualJoystick(int index) noexcept
{
    (void)VirtualRemove(index, 0);
}
bool IsVirtualJoystick(int index) noexcept
{
    return VirtualIsIndex(index, 0);
}
bool IsVirtualJoystickId(JoystickId id) noexcept
{
    return VirtualIsIdImpl(id);
}
bool DetachVirtualJoystickById(JoystickId id) noexcept
{
    return VirtualDetachByIdImpl(id);
}
bool SetVirtualJoystickAxis(JoystickId id, int axis_index, float value) noexcept
{
    return VirtualSetAxisImpl(id, axis_index, value);
}
bool SetVirtualJoystickBallMotion(JoystickId id, int ball_index, float delta_x, float delta_y) noexcept
{
    return VirtualSetBallMotionImpl(id, ball_index, delta_x, delta_y);
}
bool SetVirtualJoystickTouchpadFinger(JoystickId id, int touchpad_index, int finger_index, float x, float y, float pressure, bool pressed) noexcept
{
    return VirtualSetTouchpadFingerImpl(id, touchpad_index, finger_index, x, y, pressure, pressed);
}
bool SetVirtualJoystickSensor(JoystickId id, JoystickSensorKind kind, float x, float y, float z) noexcept
{
    return VirtualSetSensorImpl(id, kind, x, y, z);
}

bool GetJoystickDescriptor(int index, JoystickDescriptor * out) noexcept
{
    if (!out)
        return false;
    if (!VirtualIsIndex(index, 0))
        return false;
    const VirtualEntry * const e = VirtualPtr(index, 0);
    if (!e)
        return false;
    *out = e->desc;
    return true;
}

bool PollJoystickInputSnapshot(int index, HonkordGL::Joystick::JoystickInputSnapshot * out) noexcept
{
    if (!out)
        return false;
    if (!VirtualIsIndex(index, 0))
        return false;
    const VirtualEntry * const e = VirtualPtr(index, 0);
    if (!e || g_virtual_open.count(e->id) == 0)
        return false;
    std::memset(out, 0, sizeof(*out));
    const int na = std::min(static_cast<int>(e->axis_values.size()), HonkordGL::Joystick::kJoystickInputSnapshotMaxAxes);
    out->axis_count = na;
    for (int ai = 0; ai < na; ++ai)
        out->axes[static_cast<std::size_t>(ai)] = e->axis_values[static_cast<std::size_t>(ai)];
    return true;
}

} // namespace HonkordGL::Joystick

#endif

namespace HonkordGL::Joystick {

bool EnsureCanonicalJoystickMapping(int index) noexcept
{
    if (!IsJoystickConnected(index))
        return false;
    const JoystickId id = GetJoystickId(index);
    if (id.value == 0)
        return false;
    if (g_mapping_db.find(id.value) != g_mapping_db.end())
        return true;
    JoystickDescriptor desc{};
    JoystickMapping mapping{};
    if (GetJoystickDescriptor(index, &desc) && BuildCanonicalByVidPid(desc.vendorId, desc.productId, &mapping)) {
        g_mapping_db[id.value] = mapping;
        return true;
    }
    char name[256]{};
    if (!GetJoystickName(index, name, sizeof(name)))
        return false;
    if (!BuildCanonicalByName(name, &mapping))
        return false;
    g_mapping_db[id.value] = mapping;
    return true;
}

int GetCanonicalJoystickProfileCount() noexcept
{
    return 3;
}

} // namespace HonkordGL::Joystick