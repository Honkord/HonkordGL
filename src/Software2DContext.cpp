/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Software2DContext implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Software2DContext.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace HonkordGL::Graphics {

struct Software2DContext::Impl {
    std::vector<std::uint8_t> pixels{};
    int w{0};
    int h{0};
};

Software2DContext::Software2DContext() noexcept
    : impl_(std::make_unique<Impl>())
{
}

Software2DContext::~Software2DContext() = default;

Software2DContext::Software2DContext(Software2DContext&&) noexcept = default;

Software2DContext& Software2DContext::operator=(Software2DContext&&) noexcept = default;

Software2DContext::Software2DContext(int width, int height)
    : impl_(std::make_unique<Impl>())
{
    (void)Resize(width, height);
}
bool Software2DContext::Resize(int width, int height) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (width <= 0 || height <= 0) {
        impl_->pixels.clear();
        impl_->w = 0;
        impl_->h = 0;
        return true;
    }
    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    try {
        impl_->pixels.resize(n);
    } catch (const std::bad_alloc&) {
        impl_->pixels.clear();
        impl_->w = 0;
        impl_->h = 0;
        return false;
    }
    impl_->w = width;
    impl_->h = height;
    return true;
}
void Software2DContext::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!impl_ || impl_->pixels.empty())
        return;
    std::uint8_t * p = impl_->pixels.data();
    const std::size_t n = impl_->pixels.size();
    for (std::size_t i = 0; i < n; i += 4) {
        p[i] = r;
        p[i + 1] = g;
        p[i + 2] = b;
        p[i + 3] = a;
    }
}
int Software2DContext::Width() const noexcept
{
    return impl_ ? impl_->w : 0;
}
int Software2DContext::Height() const noexcept
{
    return impl_ ? impl_->h : 0;
}
int Software2DContext::StrideBytes() const noexcept
{
    if (!impl_ || impl_->w <= 0)
        return 0;
    return impl_->w * 4;
}
std::size_t Software2DContext::ByteSize() const noexcept
{
    return impl_ ? impl_->pixels.size() : 0;
}
std::uint8_t * Software2DContext::Pixels() noexcept
{
    if (!impl_ || impl_->pixels.empty())
        return nullptr;
    return impl_->pixels.data();
}
const std::uint8_t * Software2DContext::Pixels() const noexcept
{
    if (!impl_ || impl_->pixels.empty())
        return nullptr;
    return impl_->pixels.data();
}
void Software2DContext::SetPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!impl_ || impl_->w <= 0 || impl_->h <= 0)
        return;
    if (x < 0 || y < 0 || x >= impl_->w || y >= impl_->h)
        return;
    std::uint8_t * const row = impl_->pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(impl_->w) * 4u;
    std::uint8_t * const px = row + static_cast<std::size_t>(x) * 4u;
    px[0] = r;
    px[1] = g;
    px[2] = b;
    px[3] = a;
}
void Software2DContext::FillSpan(int y, int x0, int x1, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!impl_ || impl_->w <= 0 || impl_->h <= 0)
        return;
    if (y < 0 || y >= impl_->h)
        return;
    int const xa = std::max(0, std::min(x0, x1));
    int const xb = std::min(impl_->w, std::max(x0, x1));
    if (xa >= xb)
        return;
    std::uint8_t * const row = impl_->pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(impl_->w) * 4u;
    for (int x = xa; x < xb; ++x) {
        std::uint8_t * const px = row + static_cast<std::size_t>(x) * 4u;
        px[0] = r;
        px[1] = g;
        px[2] = b;
        px[3] = a;
    }
}
void Software2DContext::FillRect(int x, int y, int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    if (!impl_ || w <= 0 || h <= 0 || impl_->w <= 0 || impl_->h <= 0)
        return;
    int const x1 = x + w;
    int const y1 = y + h;
    int const ya = std::max(0, y);
    int const yb = std::min(impl_->h, y1);
    for (int row = ya; row < yb; ++row)
        FillSpan(row, x, x1, r, g, b, a);
}

} // namespace HonkordGL::Graphics