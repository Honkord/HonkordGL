/**
 * HonkordGL — Metal integration stubs on non-Apple targets
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/MetalIntegration.h>

#if !defined(__APPLE__)

namespace HonkordGL::Graphics {

int MetalIntegrationGetNativeHandles(ApplicationContextSettings const *, MetalNativeHandles *) noexcept
{
    return static_cast<int>(MetalIntegrationResult::UNSUPPORTED_PLATFORM);
}

} // namespace HonkordGL::Graphics

#endif
