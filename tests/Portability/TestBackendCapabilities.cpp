/**
 * Portability: backend capability queries match the generated build feature set.
 */
#include <HonkordGL/BackendCapabilities.h>
#include <HonkordGL/BuildFeatures.h>
#include <cstdlib>

int main()
{
    using namespace HonkordGL::Graphics;
    const bool ogl = IsOpenGLBackendAvailable();
    const bool vk = IsVulkanBackendAvailable();
    const bool d3d = IsDirect3D11BackendAvailable();
    const bool mtl = IsMetalBackendAvailable();
    const bool aud = IsAudioBackendAvailable();
    if (ogl != (HONKORDGL_HAVE_OPENGL != 0))
        return 1;
    if (vk != (HONKORDGL_HAVE_VULKAN != 0))
        return 2;
    if (d3d != (HONKORDGL_HAVE_D3D11 != 0))
        return 3;
    if (mtl != (HONKORDGL_HAVE_METAL != 0))
        return 4;
    if (aud != (HONKORDGL_HAVE_AUDIO != 0))
        return 5;
    return 0;
}
