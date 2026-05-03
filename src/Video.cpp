/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — internal API error buffer (Video / Graphics)
    * Copyright (C) 2026 Honkord
    */

#include <HonkordGL/Video.h>

#include <HonkordGL/SoftwareRenderer.h>
#include <HonkordGL/WindowBackend.h>

#include <cstring>
#include <mutex>
#include <string>

namespace HonkordGL::Graphics {

struct Window::Impl {
    mutable ApplicationContextSettings settings;
    std::unique_ptr<WindowBackend> owned_backend;
    /** When non-null, `Window` borrows this backend (multi-window on one display); otherwise `owned_backend` is used. */
    WindowBackend * shared_backend{nullptr};
    SoftwareRenderer software;
    bool backend_initialized{false};

    WindowBackend & be() noexcept
    {
        return shared_backend ? *shared_backend : *owned_backend;
    }
    WindowBackend const & be() const noexcept
    {
        return shared_backend ? *shared_backend : *owned_backend;
    }
};

Window::Window() noexcept
    : impl_(std::make_unique<Impl>())
{
    impl_->owned_backend = std::make_unique<WindowBackend>();
}

Window::Window(ApplicationContextSettings& settings) noexcept
    : impl_(std::make_unique<Impl>())
{
    impl_->owned_backend = std::make_unique<WindowBackend>();
    impl_->settings = settings;
}

Window::Window(WindowBackend& shared_backend, ApplicationContextSettings& settings) noexcept
    : impl_(std::make_unique<Impl>())
{
    impl_->shared_backend = &shared_backend;
    impl_->settings = settings;
    impl_->backend_initialized = true;
}

Window::Window(Window&&) noexcept = default;

Window& Window::operator=(Window&&) noexcept = default;

Window::~Window() noexcept
{
    TerminateWindow();
}

bool Window::WindowIsOpen() const noexcept
{
    return impl_ && impl_->settings.window_handle != nullptr;
}

void Window::SetWindowSettings(ApplicationContextSettings& settings) noexcept
{
    if (!impl_)
        return;
    impl_->settings = settings;
}
void Window::CreateWindow() noexcept
{
    if (!impl_)
        return;
    if (!impl_->backend_initialized) {
        if (!impl_->shared_backend) {
            if (!impl_->be().Initialize(nullptr))
                return;
        }
	 
        impl_->backend_initialized = true;
    }
    if (impl_->settings.client_rect.w <= 0 || impl_->settings.client_rect.z <= 0)
        return;
    impl_->be().CreateWindow(impl_->settings);
    impl_->software.Bind(impl_->settings, impl_->be());
}
void Window::SetCursorKind(Graphics::CursorKind kind) noexcept
{
    if (!impl_ || !impl_->backend_initialized)
        return;
    impl_->be().SetCursorKind(impl_->settings, kind);
}
bool Window::SetCursorFromPixels(const Graphics::CursorImageParams& params) noexcept
{
    if (!impl_ || !impl_->backend_initialized)
        return false;
    return impl_->be().SetCursorFromPixels(impl_->settings, params);
}
bool Window::SetCursorFromImageFile(const char * path, const Graphics::CursorImageParams& params) noexcept
{
    if (!impl_ || !impl_->backend_initialized)
        return false;
    return impl_->be().SetCursorFromImageFile(impl_->settings, path, params);
}
void Window::ResetCursor() noexcept
{
    if (!impl_ || !impl_->backend_initialized)
        return;
    impl_->be().ResetCursor(impl_->settings);
}
void Window::ResetApplication(ApplicationContextSettings& settings) noexcept
{
    if (!impl_)
        return;
    if (!impl_->backend_initialized) {
        if (!impl_->shared_backend) {
            if (!impl_->be().Initialize(nullptr))
                return;
        }
        impl_->backend_initialized = true;
    }

    if (impl_->settings.window_handle) {
        impl_->be().ResetApplicationSettings(impl_->settings, settings);
        impl_->software.Bind(impl_->settings, impl_->be());
        return;
    }

    impl_->settings = settings;
    impl_->settings.window_handle = nullptr;
    impl_->settings.surface = nullptr;
    impl_->settings.egl_display = nullptr;
    impl_->settings.renderer_private = nullptr;
    impl_->settings.active_renderer = 0;
    if (impl_->settings.client_rect.w > 0 && impl_->settings.client_rect.z > 0) {
        impl_->be().CreateWindow(impl_->settings);
        impl_->software.Bind(impl_->settings, impl_->be());
    }
}
ApplicationContextSettings& Window::RetrieveCurrentSettings() const noexcept
{
    static thread_local ApplicationContextSettings empty{};
    if (!impl_)
        return empty;
    return impl_->settings;
}
void Window::TerminateWindow() noexcept
{
    if (!impl_)
        return;
    impl_->software.Unbind();
    if (impl_->settings.window_handle)
        impl_->be().DestroyWindow(impl_->settings);
    if (!impl_->shared_backend) {
        impl_->be().CloseDisplay();
        impl_->be().Shutdown();
    }
    impl_->backend_initialized = false;
}
void Window::SetTargetFPS(int fps) noexcept
{
    if (!impl_)
        return;
    impl_->be().SetTargetFrameRate(fps);
}
void Window::DelayFrame() noexcept
{
    if (!impl_)
        return;
    impl_->be().DelayFrame();
}
void Window::DelayFPS() noexcept
{
    DelayFrame();
}
std::uint64_t Window::GetTicks() const noexcept
{
    if (!impl_)
        return 0;
    return impl_->be().GetTicks();
}
void Window::SetTicks(std::uint64_t ticks) noexcept
{
    if (!impl_)
        return;
    impl_->be().SetTicks(ticks);
}
bool Window::ClearFrame(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!impl_)
        return false;
    EnsureSoftwareBound(*impl_);
    if (!impl_->software.EnsureSurface())
        return false;
    impl_->software.Surface().Clear(r, g, b, a);
    return true;
}
bool Window::DisplayFrame() noexcept
{
    if (!impl_)
        return false;
    EnsureSoftwareBound(*impl_);
    return impl_->software.Present();
}
Window& Window::operator=(const Window& other)
{
    if (this == &other)
        return *this;
    if (!impl_)
        return *this;
    ApplicationContextSettings s = other.RetrieveCurrentSettings();
    s.window_handle = nullptr;
    s.surface = nullptr;
    s.device = nullptr;
    s.device_context = nullptr;
    impl_->settings = s;
    return *this;
}
void Window::EnsureSoftwareBound(Impl& impl) noexcept
{
    if (!impl.backend_initialized)
        return;
    if (impl.software.App() != nullptr)
        return;
    impl.software.Bind(impl.settings, impl.be());
}
Window& InitializeWindowSubsystem()
{
    static Window instance;
    return instance;
}

std::unique_ptr<Window> CreateWindowOnSharedBackend(WindowBackend& shared_backend, ApplicationContextSettings& settings)
{
    return std::make_unique<Window>(shared_backend, settings);
}

namespace {

std::mutex g_error_mu;
std::string g_error;

} // namespace

void SetInternalApiError(const char * message) noexcept
{
    try {
        const std::lock_guard<std::mutex> lock(g_error_mu);
        if (!message || message[0] == '\0')
            g_error.clear();
        else
            g_error = message;
    } catch (...) {
    }
}
const char * GetInternalApiError() noexcept
{
    static thread_local std::string tls;
    try {
        const std::lock_guard<std::mutex> lock(g_error_mu);
        tls = g_error;
        return tls.c_str();
    } catch (...) {
        tls.clear();
        return tls.c_str();
    }
}
void ClearInternalApiError() noexcept
{
    SetInternalApiError(nullptr);
}
void ErrorCallback(int * status, char * log, unsigned int size) noexcept
{
    if (size == 0) {
        if (status)
            *status = 0;
        return;
    }
    try {
        std::string copy;
        {
            const std::lock_guard<std::mutex> lock(g_error_mu);
            copy = g_error;
        }
        if (status)
            *status = copy.empty() ? 0 : 1;
        if (!log)
            return;
        const unsigned int n = size - 1u;
        if (copy.empty()) {
            log[0] = '\0';
            return;
        }
        const std::size_t len = copy.size();
        const std::size_t w = len < static_cast<std::size_t>(n) ? len : static_cast<std::size_t>(n);
        std::memcpy(log, copy.c_str(), w);
        log[w] = '\0';
    } catch (...) {
        if (status)
            *status = 0;
        if (log && size > 0)
            log[0] = '\0';
    }
}

} // namespace HonkordGL::Graphics
