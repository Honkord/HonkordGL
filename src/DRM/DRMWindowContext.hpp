/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — per-surface DRM/KMS state (implementation detail)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_DRM_WINDOWCONTEXT_HPP
#define HONKORDGL_DRM_WINDOWCONTEXT_HPP

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdint>
#include <deque>

namespace HonkordGL::Graphics::DRM::Internal {

struct DrmWindowState {
    ApplicationContextSettings * ctx{nullptr};
    std::deque<HonkordGL::Events::Event> pending;
    int drm_fd{-1};
    uint32_t crtc_id{0};
    uint32_t connector_id{0};
    uint32_t fb_id{0};
    uint32_t dumb_handle{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
};

} // namespace HonkordGL::Graphics::DRM::Internal

#endif