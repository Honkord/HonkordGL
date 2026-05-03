/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — stable public API level for applications (no vendor / driver symbols)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPUAPIBOUNDARY_H
#define HONKORDGL_GPUAPIBOUNDARY_H

#include <HonkordGL/Config.h>

#include <cstdint>

namespace HonkordGL::Graphics {

/**
 * Packs semantic API version into one integer for comparisons at init / attach time.
 * Encoding: `(major << 16) | (minor << 8) | patch` (each component 0–255).
 * This value is derived from `HONKORDGL_VERSION__MAJOR` / `HONKORDGL_VERSION__MINOR` in `Config.h`
 * (patch is 0 until a patch digit is added to the public config surface).
 */
constexpr std::uint32_t HonkordGlMakeApiLevel(unsigned major, unsigned minor, unsigned patch) noexcept
{
    return (static_cast<std::uint32_t>(major & 0xFFu) << 16)
        | (static_cast<std::uint32_t>(minor & 0xFFu) << 8)
        | static_cast<std::uint32_t>(patch & 0xFFu);
}

/** API level implemented by this build of the library. */
constexpr std::uint32_t HonkordGlLibraryApiLevel() noexcept
{
    return HonkordGlMakeApiLevel(
        static_cast<unsigned>(HONKORDGL_VERSION__MAJOR),
        static_cast<unsigned>(HONKORDGL_VERSION__MINOR),
        0u);
}

/**
 * Returns true when this library’s API level is at least `app_minimum_level`.
 * Applications pass the minimum level they were written against (from `HonkordGlMakeApiLevel`).
 */
constexpr bool HonkordGlApiLevelCompatible(std::uint32_t app_minimum_level) noexcept
{
    return HonkordGlLibraryApiLevel() >= app_minimum_level;
}

} // namespace HonkordGL::Graphics

#endif
