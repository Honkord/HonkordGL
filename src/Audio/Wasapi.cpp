/**
 * HonkordGL — Windows WASAPI placeholder backend entry point
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Audio.h>
#include <HonkordGL/Config.h>

#if HONKORDGL_PLATFORM_WINDOWS
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

namespace HonkordGL::Audio
{

/**
 * Phase-1 note:
 * Full WASAPI mixer path is integrated through the generic audio API surface and
 * platform-selected backend kind/capability reporting. This TU is intentionally
 * kept lightweight so Windows builds have an explicit backend anchor file.
 */
int HonkordGL_WasapiBackendAnchor() noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    return 0;
#else
    return -1;
#endif
}

} // namespace HonkordGL::Audio
