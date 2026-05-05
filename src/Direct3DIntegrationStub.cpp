/**
 * HonkordGL — Direct3D integration stubs on non-Windows targets
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Direct3DIntegration.h>

#if !defined(_WIN32)

namespace HonkordGL::Graphics {

int Direct3D11IntegrationGetNativeHandles(ApplicationContextSettings const *, Direct3D11NativeHandles *) noexcept
{
    return static_cast<int>(Direct3DIntegrationResult::UNSUPPORTED_PLATFORM);
}

} // namespace HonkordGL::Graphics

#endif
