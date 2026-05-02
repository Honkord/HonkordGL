/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Eclipse (ellipse / circle) implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Eclipse.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace HonkordGL::Graphics {
namespace eclipse_detail {

struct EllipseCmd {
    float cx{};
    float cy{};
    float rx{};
    float ry{};
    float rotDeg{};
    int segments{64};
    float fr{1.f};
    float fg{1.f};
    float fb{1.f};
    float fa{1.f};
    float br{1.f};
    float bg{1.f};
    float bb{1.f};
    float ba{1.f};
    float borderThick{};
};

inline void pixelTopLeftToNdc(float px, float pyTop, int fbw, int fbh, float * outX, float * outY) noexcept
{
    const float glY = static_cast<float>(fbh) - pyTop;
    *outX = (2.f * px / static_cast<float>(fbw)) - 1.f;
    *outY = (2.f * glY / static_cast<float>(fbh)) - 1.f;
}

inline void localToWorld(
    float lx,
    float ly,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float * wx,
    float * wy) noexcept
{
    const float ex = lx - pivotX;
    const float ey = ly - pivotY;
    const float lsx = ex * scaleX;
    const float lsy = ey * scaleY;
    const float rad = rotDeg * (3.14159265358979323846f / 180.f);
    const float cr = std::cos(rad);
    const float sr = std::sin(rad);
    const float rx = lsx * cr - lsy * sr;
    const float ry = lsx * sr + lsy * cr;
    *wx = pivotX + transX + rx;
    *wy = pivotY + transY + ry;
}

inline float thicknessScale(float scaleX, float scaleY) noexcept
{
    return 0.5f * (std::fabs(scaleX) + std::fabs(scaleY));
}

inline void ellipseLocalPoint(
    float ecx,
    float ecy,
    float erx,
    float ery,
    float axisRotDeg,
    float t,
    float * ox,
    float * oy) noexcept
{
    const float u = erx * std::cos(t);
    const float v = ery * std::sin(t);
    const float rd = axisRotDeg * (3.14159265358979323846f / 180.f);
    const float c0 = std::cos(rd);
    const float s0 = std::sin(rd);
    *ox = ecx + u * c0 - v * s0;
    *oy = ecy + u * s0 + v * c0;
}

inline void emitVert(
    std::vector<float> & out, float px, float py, int fbw, int fbh, float r, float g, float b, float a) noexcept
{
    float ndcX = 0.f;
    float ndcY = 0.f;
    pixelTopLeftToNdc(px, py, fbw, fbh, &ndcX, &ndcY);
    out.push_back(ndcX);
    out.push_back(ndcY);
    out.push_back(r);
    out.push_back(g);
    out.push_back(b);
    out.push_back(a);
}

inline void appendEllipseFilled(
    EllipseCmd const & c,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    int fbw,
    int fbh,
    std::vector<float> & out) noexcept
{
    if (c.fa <= 1e-6f || c.rx <= 0.f || c.ry <= 0.f)
        return;
    const int seg = std::max(8, std::min(256, c.segments));
    float wxc = 0.f;
    float wyc = 0.f;
    localToWorld(c.cx, c.cy, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wxc, &wyc);
    for (int i = 0; i < seg; ++i) {
        const float t0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float t1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        float lx0 = 0.f;
        float ly0 = 0.f;
        float lx1 = 0.f;
        float ly1 = 0.f;
        ellipseLocalPoint(c.cx, c.cy, c.rx, c.ry, c.rotDeg, t0, &lx0, &ly0);
        ellipseLocalPoint(c.cx, c.cy, c.rx, c.ry, c.rotDeg, t1, &lx1, &ly1);
        float wx0 = 0.f;
        float wy0 = 0.f;
        float wx1 = 0.f;
        float wy1 = 0.f;
        localToWorld(lx0, ly0, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wx0, &wy0);
        localToWorld(lx1, ly1, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wx1, &wy1);
        emitVert(out, wxc, wyc, fbw, fbh, c.fr, c.fg, c.fb, c.fa);
        emitVert(out, wx0, wy0, fbw, fbh, c.fr, c.fg, c.fb, c.fa);
        emitVert(out, wx1, wy1, fbw, fbh, c.fr, c.fg, c.fb, c.fa);
    }
}

inline void appendEllipseBorderRing(
    EllipseCmd const & c,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    float borderPx,
    int fbw,
    int fbh,
    std::vector<float> & out) noexcept
{
    if (borderPx <= 1e-6f || c.ba <= 1e-6f || c.rx <= 0.f || c.ry <= 0.f)
        return;
    const int seg = std::max(8, std::min(256, c.segments));
    const float ri_x = std::max(0.25f, c.rx - borderPx * 0.5f);
    const float ri_y = std::max(0.25f, c.ry - borderPx * 0.5f);
    const float ro_x = c.rx + borderPx * 0.5f;
    const float ro_y = c.ry + borderPx * 0.5f;
    for (int i = 0; i < seg; ++i) {
        const float t0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float t1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        float i0x = 0.f;
        float i0y = 0.f;
        float i1x = 0.f;
        float i1y = 0.f;
        float o0x = 0.f;
        float o0y = 0.f;
        float o1x = 0.f;
        float o1y = 0.f;
        ellipseLocalPoint(c.cx, c.cy, ri_x, ri_y, c.rotDeg, t0, &i0x, &i0y);
        ellipseLocalPoint(c.cx, c.cy, ri_x, ri_y, c.rotDeg, t1, &i1x, &i1y);
        ellipseLocalPoint(c.cx, c.cy, ro_x, ro_y, c.rotDeg, t0, &o0x, &o0y);
        ellipseLocalPoint(c.cx, c.cy, ro_x, ro_y, c.rotDeg, t1, &o1x, &o1y);
        float wi0x = 0.f;
        float wi0y = 0.f;
        float wi1x = 0.f;
        float wi1y = 0.f;
        float wo0x = 0.f;
        float wo0y = 0.f;
        float wo1x = 0.f;
        float wo1y = 0.f;
        localToWorld(i0x, i0y, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wi0x, &wi0y);
        localToWorld(i1x, i1y, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wi1x, &wi1y);
        localToWorld(o0x, o0y, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wo0x, &wo0y);
        localToWorld(o1x, o1y, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wo1x, &wo1y);
        emitVert(out, wi0x, wi0y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
        emitVert(out, wo0x, wo0y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
        emitVert(out, wo1x, wo1y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
        emitVert(out, wi0x, wi0y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
        emitVert(out, wo1x, wo1y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
        emitVert(out, wi1x, wi1y, fbw, fbh, c.br, c.bg, c.bb, c.ba);
    }
}

inline void appendQueueTransformed(
    std::vector<EllipseCmd> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    int fbw,
    int fbh,
    std::vector<float> & interleaved) noexcept
{
    const float tScale = thicknessScale(scaleX, scaleY);
    for (EllipseCmd const & c : queue) {
        appendEllipseFilled(c, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, fbw, fbh, interleaved);
        if (c.borderThick > 1e-6f) {
            const float bt = c.borderThick * (tScale > 1e-6f ? tScale : 1.f);
            appendEllipseBorderRing(
                c, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, bt, fbw, fbh, interleaved);
        }
    }
}

inline void worldToLocal(
    float wx,
    float wy,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float * lx,
    float * ly) noexcept
{
    const float qx = wx - pivotX - transX;
    const float qy = wy - pivotY - transY;
    const float rad = rotDeg * (3.14159265358979323846f / 180.f);
    const float cr = std::cos(rad);
    const float sr = std::sin(rad);
    const float lsx = qx * cr + qy * sr;
    const float lsy = -qx * sr + qy * cr;
    const float invSx = std::fabs(scaleX) > 1e-12f ? (1.f / scaleX) : 0.f;
    const float invSy = std::fabs(scaleY) > 1e-12f ? (1.f / scaleY) : 0.f;
    const float ex = lsx * invSx;
    const float ey = lsy * invSy;
    *lx = ex + pivotX;
    *ly = ey + pivotY;
}

inline void expandAabb(float & minX, float & minY, float & maxX, float & maxY, float x, float y) noexcept
{
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
}

inline bool aabbsIntersect(float minAx, float minAy, float maxAx, float maxAy, float minBx, float minBy, float maxBx, float maxBy) noexcept
{
    return !(maxAx < minBx || maxBx < minAx || maxAy < minBy || maxBy < minAy);
}

inline void ellipseCmdExpandWorldAabb(
    EllipseCmd const & c,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    float tScale,
    bool & hasAny,
    float & minX,
    float & minY,
    float & maxX,
    float & maxY) noexcept
{
    constexpr int kSamples = 36;
    auto const sampleLoop = [&](float erx, float ery) noexcept {
        if (erx <= 1e-6f || ery <= 1e-6f)
            return;
        for (int i = 0; i < kSamples; ++i) {
            const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(kSamples);
            float lx = 0.f;
            float ly = 0.f;
            ellipseLocalPoint(c.cx, c.cy, erx, ery, c.rotDeg, t, &lx, &ly);
            float wx = 0.f;
            float wy = 0.f;
            localToWorld(lx, ly, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &wx, &wy);
            if (!hasAny) {
                minX = maxX = wx;
                minY = maxY = wy;
                hasAny = true;
            } else {
                expandAabb(minX, minY, maxX, maxY, wx, wy);
            }
        }
    };

    if (c.fa > 1e-6f && c.rx > 0.f && c.ry > 0.f)
        sampleLoop(c.rx, c.ry);
    if (c.borderThick > 1e-6f && c.ba > 1e-6f && c.rx > 0.f && c.ry > 0.f) {
        const float bt = c.borderThick * (tScale > 1e-6f ? tScale : 1.f);
        const float ro_x = c.rx + bt * 0.5f;
        const float ro_y = c.ry + bt * 0.5f;
        sampleLoop(ro_x, ro_y);
    }
}

inline bool implicitEllipse(float u, float v, float rx, float ry) noexcept
{
    if (rx <= 1e-6f || ry <= 1e-6f)
        return false;
    const float nx = u / rx;
    const float ny = v / ry;
    return nx * nx + ny * ny <= 1.f + 1e-5f;
}

inline bool ellipseCmdContainsWorldPoint(
    float px,
    float py,
    EllipseCmd const & c,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    float tScale) noexcept
{
    float lx = 0.f;
    float ly = 0.f;
    worldToLocal(px, py, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, &lx, &ly);
    const float dx = lx - c.cx;
    const float dy = ly - c.cy;
    const float rd = -c.rotDeg * (3.14159265358979323846f / 180.f);
    const float cr = std::cos(rd);
    const float sr = std::sin(rd);
    const float u = dx * cr - dy * sr;
    const float v = dx * sr + dy * cr;

    if (c.fa > 1e-6f && c.rx > 0.f && c.ry > 0.f && implicitEllipse(u, v, c.rx, c.ry))
        return true;

    if (c.borderThick > 1e-6f && c.ba > 1e-6f && c.rx > 0.f && c.ry > 0.f) {
        const float bt = c.borderThick * (tScale > 1e-6f ? tScale : 1.f);
        const float ri_x = std::max(0.25f, c.rx - bt * 0.5f);
        const float ri_y = std::max(0.25f, c.ry - bt * 0.5f);
        const float ro_x = c.rx + bt * 0.5f;
        const float ro_y = c.ry + bt * 0.5f;
        if (implicitEllipse(u, v, ro_x, ro_y) && !implicitEllipse(u, v, ri_x, ri_y))
            return true;
    }
    return false;
}

inline bool queueWorldAabb(
    std::vector<EllipseCmd> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    float & outMinX,
    float & outMinY,
    float & outMaxX,
    float & outMaxY) noexcept
{
    const float tScale = thicknessScale(scaleX, scaleY);
    bool hasAny = false;
    for (EllipseCmd const & c : queue)
        ellipseCmdExpandWorldAabb(
            c, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, tScale, hasAny, outMinX, outMinY, outMaxX, outMaxY);
    return hasAny;
}

inline bool eclipseIntersectsEclipse(
    std::vector<EllipseCmd> const & qa,
    float apx,
    float apy,
    float asx,
    float asy,
    float arot,
    float atx,
    float aty,
    std::vector<EllipseCmd> const & qb,
    float bpx,
    float bpy,
    float bsx,
    float bsy,
    float brot,
    float btx,
    float bty) noexcept
{
    if (qa.empty() || qb.empty())
        return false;
    float aminX = 0.f;
    float aminY = 0.f;
    float amaxX = 0.f;
    float amaxY = 0.f;
    float bminX = 0.f;
    float bminY = 0.f;
    float bmaxX = 0.f;
    float bmaxY = 0.f;
    if (!queueWorldAabb(qa, apx, apy, asx, asy, arot, atx, aty, aminX, aminY, amaxX, amaxY))
        return false;
    if (!queueWorldAabb(qb, bpx, bpy, bsx, bsy, brot, btx, bty, bminX, bminY, bmaxX, bmaxY))
        return false;
    return aabbsIntersect(aminX, aminY, amaxX, amaxY, bminX, bminY, bmaxX, bmaxY);
}

inline bool eclipseContainsPoint(
    std::vector<EllipseCmd> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float entityRotDeg,
    float transX,
    float transY,
    float px,
    float py) noexcept
{
    if (queue.empty())
        return false;
    const float tScale = thicknessScale(scaleX, scaleY);
    for (EllipseCmd const & c : queue) {
        if (ellipseCmdContainsWorldPoint(px, py, c, pivotX, pivotY, scaleX, scaleY, entityRotDeg, transX, transY, tScale))
            return true;
    }
    return false;
}

} // namespace eclipse_detail
} // namespace HonkordGL::Graphics

#if defined(__ANDROID__)

#include <GLES2/gl2.h>

namespace HonkordGL::Graphics {

namespace {

static const char kVsEs2Fill[] = R"(attribute vec2 a_pos;
attribute vec4 a_color;
varying vec4 v_color;
void main() {
  v_color = a_color;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char kFsEs2Fill[] = R"(precision mediump float;
varying vec4 v_color;
void main() {
  gl_FragColor = v_color;
}
)";

static GLuint compileShaderEs2(GLenum type, const char * src)
{
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint linkFillProgramEs2()
{
    const GLuint vs = compileShaderEs2(GL_VERTEX_SHADER, kVsEs2Fill);
    const GLuint fs = compileShaderEs2(GL_FRAGMENT_SHADER, kFsEs2Fill);
    if (!vs || !fs)
        return 0;
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_color");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace

struct Eclipse::Impl {
    std::vector<eclipse_detail::EllipseCmd> queue{};
    bool recording{false};
    float nextFr{1.f};
    float nextFg{1.f};
    float nextFb{1.f};
    float nextFa{1.f};
    float nextBr{1.f};
    float nextBg{1.f};
    float nextBb{1.f};
    float nextBa{0.f};
    float nextBorderThick{0.f};
    int nextSegments{64};
    float pivotX{0.f};
    float pivotY{0.f};
    float entityX{0.f};
    float entityY{0.f};
    float rotDeg{0.f};
    float scaleX{1.f};
    float scaleY{1.f};
    unsigned int fillProgram{0};
    unsigned int fillVbo{0};
};

Eclipse::Eclipse() noexcept
    : impl_(std::make_unique<Impl>())
{
}

Eclipse::~Eclipse()
{
    Destroy();
}

Eclipse::Eclipse(Eclipse&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

Eclipse& Eclipse::operator=(Eclipse&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void Eclipse::Destroy() noexcept
{
    if (!impl_)
        return;
    if (impl_->fillVbo)
        glDeleteBuffers(1, &impl_->fillVbo);
    if (impl_->fillProgram)
        glDeleteProgram(impl_->fillProgram);
    impl_->fillVbo = 0;
    impl_->fillProgram = 0;
    impl_->queue.clear();
    impl_->recording = false;
}

void Eclipse::SetEntityPivot(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = x;
    impl_->pivotY = y;
}

void Eclipse::SetEntityPosition(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->entityX = x;
    impl_->entityY = y;
}

void Eclipse::SetEntityRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->rotDeg = degrees;
}

void Eclipse::SetEntityScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = sx;
    impl_->scaleY = sy;
}

void Eclipse::SetEntityScale(float uniform) noexcept
{
    SetEntityScale(uniform, uniform);
}

void Eclipse::DilateEntity(float uniformScale) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX *= uniformScale;
    impl_->scaleY *= uniformScale;
}

void Eclipse::FlipEntityX() noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = -impl_->scaleX;
}

void Eclipse::FlipEntityY() noexcept
{
    if (!impl_)
        return;
    impl_->scaleY = -impl_->scaleY;
}

void Eclipse::ResetEntityTransform() noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = 0.f;
    impl_->pivotY = 0.f;
    impl_->entityX = 0.f;
    impl_->entityY = 0.f;
    impl_->rotDeg = 0.f;
    impl_->scaleX = 1.f;
    impl_->scaleY = 1.f;
}

float Eclipse::EntityPivotX() const noexcept
{
    return impl_ ? impl_->pivotX : 0.f;
}

float Eclipse::EntityPivotY() const noexcept
{
    return impl_ ? impl_->pivotY : 0.f;
}

float Eclipse::EntityPositionX() const noexcept
{
    return impl_ ? impl_->entityX : 0.f;
}

float Eclipse::EntityPositionY() const noexcept
{
    return impl_ ? impl_->entityY : 0.f;
}

float Eclipse::EntityRotationDegrees() const noexcept
{
    return impl_ ? impl_->rotDeg : 0.f;
}

float Eclipse::EntityScaleX() const noexcept
{
    return impl_ ? impl_->scaleX : 1.f;
}

float Eclipse::EntityScaleY() const noexcept
{
    return impl_ ? impl_->scaleY : 1.f;
}

bool Eclipse::IntersectsEntityBounds(Eclipse const & other) const noexcept
{
    if (!impl_ || !other.impl_)
        return false;
    return eclipse_detail::eclipseIntersectsEclipse(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        other.impl_->queue,
        other.impl_->pivotX,
        other.impl_->pivotY,
        other.impl_->scaleX,
        other.impl_->scaleY,
        other.impl_->rotDeg,
        other.impl_->entityX,
        other.impl_->entityY);
}

bool Eclipse::ContainsClientPoint(float x, float y) const noexcept
{
    if (!impl_)
        return false;
    return eclipse_detail::eclipseContainsPoint(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        x,
        y);
}

void Eclipse::Begin() noexcept
{
    if (!impl_)
        return;
    impl_->queue.clear();
    impl_->recording = true;
    impl_->nextFr = 1.f;
    impl_->nextFg = 1.f;
    impl_->nextFb = 1.f;
    impl_->nextFa = 1.f;
    impl_->nextBr = 1.f;
    impl_->nextBg = 1.f;
    impl_->nextBb = 1.f;
    impl_->nextBa = 0.f;
    impl_->nextBorderThick = 0.f;
    impl_->nextSegments = 64;
}

void Eclipse::SetNextFillColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextFr = r;
    impl_->nextFg = g;
    impl_->nextFb = b;
    impl_->nextFa = a;
}

void Eclipse::SetNextBorderColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextBr = r;
    impl_->nextBg = g;
    impl_->nextBb = b;
    impl_->nextBa = a;
}

void Eclipse::SetNextBorderThickness(float thicknessPixels) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (thicknessPixels >= 0.f)
        impl_->nextBorderThick = thicknessPixels;
}

void Eclipse::SetNextSegmentCount(int segments) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextSegments = std::max(8, std::min(256, segments));
}

void Eclipse::Circle(float centerX, float centerY, float radius) noexcept
{
    Ellipse(centerX, centerY, radius, radius, 0.f);
}

void Eclipse::Ellipse(float centerX, float centerY, float radiusX, float radiusY, float rotationDegrees) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (radiusX <= 0.f || radiusY <= 0.f)
        return;
    eclipse_detail::EllipseCmd c{};
    c.cx = centerX;
    c.cy = centerY;
    c.rx = radiusX;
    c.ry = radiusY;
    c.rotDeg = rotationDegrees;
    c.segments = impl_->nextSegments;
    c.fr = impl_->nextFr;
    c.fg = impl_->nextFg;
    c.fb = impl_->nextFb;
    c.fa = impl_->nextFa;
    c.br = impl_->nextBr;
    c.bg = impl_->nextBg;
    c.bb = impl_->nextBb;
    c.ba = impl_->nextBa;
    c.borderThick = impl_->nextBorderThick;
    impl_->queue.push_back(c);
}

void Eclipse::End(ApplicationContextSettings & ctx) noexcept
{
    if (!impl_)
        return;
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!impl_->recording)
        return;
    impl_->recording = false;
    if (impl_->queue.empty())
        return;

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    std::vector<float> interleaved;
    eclipse_detail::appendQueueTransformed(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        fbw,
        fbh,
        interleaved);
    if (interleaved.empty())
        return;

    if (!impl_->fillProgram)
        impl_->fillProgram = linkFillProgramEs2();
    if (!impl_->fillProgram)
        return;

    if (!impl_->fillVbo)
        glGenBuffers(1, &impl_->fillVbo);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->fillProgram);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->fillVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
        interleaved.data(),
        GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(0));
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(8));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    const GLsizei vcount = static_cast<GLsizei>(interleaved.size() / 6u);
    glDrawArrays(GL_TRIANGLES, 0, vcount);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

} // namespace HonkordGL::Graphics

#else /* !__ANDROID__ */

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#else
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#if !defined(_WIN32)
#include <GL/glx.h>
#endif
#endif

namespace HonkordGL::Graphics {

namespace {

#if !defined(__APPLE__)
using PFN_glGenVertexArrays = void (*)(GLsizei, GLuint *);
using PFN_glDeleteVertexArrays = void (*)(GLsizei, const GLuint *);
using PFN_glBindVertexArray = void (*)(GLuint);
using PFN_glGenBuffers = void (*)(GLsizei, GLuint *);
using PFN_glDeleteBuffers = void (*)(GLsizei, const GLuint *);
using PFN_glBindBuffer = void (*)(GLenum, GLuint);
using PFN_glBufferData = void (*)(GLenum, GLsizeiptr, const void *, GLenum);
using PFN_glVertexAttribPointer = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
using PFN_glEnableVertexAttribArray = void (*)(GLuint);
using PFN_glDisableVertexAttribArray = void (*)(GLuint);
using PFN_glCreateShader = GLuint (*)(GLenum);
using PFN_glShaderSource = void (*)(GLuint, GLsizei, const GLchar * const *, const GLint *);
using PFN_glCompileShader = void (*)(GLuint);
using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glDeleteShader = void (*)(GLuint);
using PFN_glCreateProgram = GLuint (*)(void);
using PFN_glAttachShader = void (*)(GLuint, GLuint);
using PFN_glLinkProgram = void (*)(GLuint);
using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glDeleteProgram = void (*)(GLuint);
using PFN_glUseProgram = void (*)(GLuint);

static PFN_glGenVertexArrays s_glGenVertexArrays{};
static PFN_glDeleteVertexArrays s_glDeleteVertexArrays{};
static PFN_glBindVertexArray s_glBindVertexArray{};
static PFN_glGenBuffers s_glGenBuffers{};
static PFN_glDeleteBuffers s_glDeleteBuffers{};
static PFN_glBindBuffer s_glBindBuffer{};
static PFN_glBufferData s_glBufferData{};
static PFN_glVertexAttribPointer s_glVertexAttribPointer{};
static PFN_glEnableVertexAttribArray s_glEnableVertexAttribArray{};
static PFN_glDisableVertexAttribArray s_glDisableVertexAttribArray{};
static PFN_glCreateShader s_glCreateShader{};
static PFN_glShaderSource s_glShaderSource{};
static PFN_glCompileShader s_glCompileShader{};
static PFN_glGetShaderiv s_glGetShaderiv{};
static PFN_glDeleteShader s_glDeleteShader{};
static PFN_glCreateProgram s_glCreateProgram{};
static PFN_glAttachShader s_glAttachShader{};
static PFN_glLinkProgram s_glLinkProgram{};
static PFN_glGetProgramiv s_glGetProgramiv{};
static PFN_glDeleteProgram s_glDeleteProgram{};
static PFN_glUseProgram s_glUseProgram{};

static void * LoadProc(const char * name) noexcept
{
#if defined(_WIN32)
    void * p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p)
        return p;
    return reinterpret_cast<void *>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));
#else
    return reinterpret_cast<void *>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#endif
}

static bool LoadGL3Procs() noexcept
{
    if (s_glGenVertexArrays)
        return true;
    s_glGenVertexArrays = reinterpret_cast<PFN_glGenVertexArrays>(LoadProc("glGenVertexArrays"));
    s_glDeleteVertexArrays = reinterpret_cast<PFN_glDeleteVertexArrays>(LoadProc("glDeleteVertexArrays"));
    s_glBindVertexArray = reinterpret_cast<PFN_glBindVertexArray>(LoadProc("glBindVertexArray"));
    s_glGenBuffers = reinterpret_cast<PFN_glGenBuffers>(LoadProc("glGenBuffers"));
    s_glDeleteBuffers = reinterpret_cast<PFN_glDeleteBuffers>(LoadProc("glDeleteBuffers"));
    s_glBindBuffer = reinterpret_cast<PFN_glBindBuffer>(LoadProc("glBindBuffer"));
    s_glBufferData = reinterpret_cast<PFN_glBufferData>(LoadProc("glBufferData"));
    s_glVertexAttribPointer = reinterpret_cast<PFN_glVertexAttribPointer>(LoadProc("glVertexAttribPointer"));
    s_glEnableVertexAttribArray =
        reinterpret_cast<PFN_glEnableVertexAttribArray>(LoadProc("glEnableVertexAttribArray"));
    s_glDisableVertexAttribArray =
        reinterpret_cast<PFN_glDisableVertexAttribArray>(LoadProc("glDisableVertexAttribArray"));
    s_glCreateShader = reinterpret_cast<PFN_glCreateShader>(LoadProc("glCreateShader"));
    s_glShaderSource = reinterpret_cast<PFN_glShaderSource>(LoadProc("glShaderSource"));
    s_glCompileShader = reinterpret_cast<PFN_glCompileShader>(LoadProc("glCompileShader"));
    s_glGetShaderiv = reinterpret_cast<PFN_glGetShaderiv>(LoadProc("glGetShaderiv"));
    s_glDeleteShader = reinterpret_cast<PFN_glDeleteShader>(LoadProc("glDeleteShader"));
    s_glCreateProgram = reinterpret_cast<PFN_glCreateProgram>(LoadProc("glCreateProgram"));
    s_glAttachShader = reinterpret_cast<PFN_glAttachShader>(LoadProc("glAttachShader"));
    s_glLinkProgram = reinterpret_cast<PFN_glLinkProgram>(LoadProc("glLinkProgram"));
    s_glGetProgramiv = reinterpret_cast<PFN_glGetProgramiv>(LoadProc("glGetProgramiv"));
    s_glDeleteProgram = reinterpret_cast<PFN_glDeleteProgram>(LoadProc("glDeleteProgram"));
    s_glUseProgram = reinterpret_cast<PFN_glUseProgram>(LoadProc("glUseProgram"));
    return s_glGenVertexArrays && s_glDeleteVertexArrays && s_glBindVertexArray && s_glGenBuffers && s_glDeleteBuffers
        && s_glBindBuffer && s_glBufferData && s_glVertexAttribPointer && s_glEnableVertexAttribArray
        && s_glDisableVertexAttribArray && s_glCreateShader && s_glShaderSource && s_glCompileShader && s_glGetShaderiv
        && s_glDeleteShader && s_glCreateProgram && s_glAttachShader && s_glLinkProgram && s_glGetProgramiv
        && s_glDeleteProgram && s_glUseProgram;
}
#endif /* !__APPLE__ */

static const char kVsFill[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;
out vec4 v_color;
void main() {
  v_color = a_color;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char kFsFill[] = R"(#version 330 core
in vec4 v_color;
out vec4 o_color;
void main() {
  o_color = v_color;
}
)";

#if defined(__APPLE__)
static GLuint compileShader(GLenum type, const char * src)
{
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint linkProgram(GLuint vs, GLuint fs)
{
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
#else
static GLuint compileShader(GLenum type, const char * src)
{
    const GLuint s = s_glCreateShader(type);
    s_glShaderSource(s, 1, &src, nullptr);
    s_glCompileShader(s);
    GLint ok = 0;
    s_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        s_glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint linkProgram(GLuint vs, GLuint fs)
{
    const GLuint p = s_glCreateProgram();
    s_glAttachShader(p, vs);
    s_glAttachShader(p, fs);
    s_glLinkProgram(p);
    GLint ok = 0;
    s_glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        s_glDeleteProgram(p);
        return 0;
    }
    return p;
}
#endif

static GLuint createFillProgramGl3() noexcept
{
#if defined(__APPLE__)
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsFill);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsFill);
    if (!vs || !fs)
        return 0;
    const GLuint p = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
#else
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsFill);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsFill);
    if (!vs || !fs)
        return 0;
    const GLuint p = linkProgram(vs, fs);
    s_glDeleteShader(vs);
    s_glDeleteShader(fs);
    return p;
#endif
}

} // namespace

struct Eclipse::Impl {
    std::vector<eclipse_detail::EllipseCmd> queue{};
    bool recording{false};
    float nextFr{1.f};
    float nextFg{1.f};
    float nextFb{1.f};
    float nextFa{1.f};
    float nextBr{1.f};
    float nextBg{1.f};
    float nextBb{1.f};
    float nextBa{0.f};
    float nextBorderThick{0.f};
    int nextSegments{64};
    float pivotX{0.f};
    float pivotY{0.f};
    float entityX{0.f};
    float entityY{0.f};
    float rotDeg{0.f};
    float scaleX{1.f};
    float scaleY{1.f};
    unsigned int fillProgram{0};
    unsigned int fillVao{0};
    unsigned int fillVbo{0};
};

Eclipse::Eclipse() noexcept
    : impl_(std::make_unique<Impl>())
{
}

Eclipse::~Eclipse()
{
    Destroy();
}

Eclipse::Eclipse(Eclipse&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

Eclipse& Eclipse::operator=(Eclipse&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void Eclipse::Destroy() noexcept
{
    if (!impl_)
        return;
#if defined(__APPLE__)
    if (impl_->fillVbo)
        glDeleteBuffers(1, &impl_->fillVbo);
    if (impl_->fillVao)
        glDeleteVertexArrays(1, &impl_->fillVao);
    if (impl_->fillProgram)
        glDeleteProgram(impl_->fillProgram);
#else
    if (s_glGenVertexArrays) {
        if (impl_->fillVbo)
            s_glDeleteBuffers(1, &impl_->fillVbo);
        if (impl_->fillVao)
            s_glDeleteVertexArrays(1, &impl_->fillVao);
        if (impl_->fillProgram)
            s_glDeleteProgram(impl_->fillProgram);
    }
#endif
    impl_->fillVbo = 0;
    impl_->fillVao = 0;
    impl_->fillProgram = 0;
    impl_->queue.clear();
    impl_->recording = false;
}

void Eclipse::SetEntityPivot(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = x;
    impl_->pivotY = y;
}

void Eclipse::SetEntityPosition(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->entityX = x;
    impl_->entityY = y;
}

void Eclipse::SetEntityRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->rotDeg = degrees;
}

void Eclipse::SetEntityScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = sx;
    impl_->scaleY = sy;
}

void Eclipse::SetEntityScale(float uniform) noexcept
{
    SetEntityScale(uniform, uniform);
}

void Eclipse::DilateEntity(float uniformScale) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX *= uniformScale;
    impl_->scaleY *= uniformScale;
}

void Eclipse::FlipEntityX() noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = -impl_->scaleX;
}

void Eclipse::FlipEntityY() noexcept
{
    if (!impl_)
        return;
    impl_->scaleY = -impl_->scaleY;
}

void Eclipse::ResetEntityTransform() noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = 0.f;
    impl_->pivotY = 0.f;
    impl_->entityX = 0.f;
    impl_->entityY = 0.f;
    impl_->rotDeg = 0.f;
    impl_->scaleX = 1.f;
    impl_->scaleY = 1.f;
}

float Eclipse::EntityPivotX() const noexcept
{
    return impl_ ? impl_->pivotX : 0.f;
}

float Eclipse::EntityPivotY() const noexcept
{
    return impl_ ? impl_->pivotY : 0.f;
}

float Eclipse::EntityPositionX() const noexcept
{
    return impl_ ? impl_->entityX : 0.f;
}

float Eclipse::EntityPositionY() const noexcept
{
    return impl_ ? impl_->entityY : 0.f;
}

float Eclipse::EntityRotationDegrees() const noexcept
{
    return impl_ ? impl_->rotDeg : 0.f;
}

float Eclipse::EntityScaleX() const noexcept
{
    return impl_ ? impl_->scaleX : 1.f;
}

float Eclipse::EntityScaleY() const noexcept
{
    return impl_ ? impl_->scaleY : 1.f;
}

bool Eclipse::IntersectsEntityBounds(Eclipse const & other) const noexcept
{
    if (!impl_ || !other.impl_)
        return false;
    return eclipse_detail::eclipseIntersectsEclipse(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        other.impl_->queue,
        other.impl_->pivotX,
        other.impl_->pivotY,
        other.impl_->scaleX,
        other.impl_->scaleY,
        other.impl_->rotDeg,
        other.impl_->entityX,
        other.impl_->entityY);
}

bool Eclipse::ContainsClientPoint(float x, float y) const noexcept
{
    if (!impl_)
        return false;
    return eclipse_detail::eclipseContainsPoint(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        x,
        y);
}

void Eclipse::Begin() noexcept
{
    if (!impl_)
        return;
    impl_->queue.clear();
    impl_->recording = true;
    impl_->nextFr = 1.f;
    impl_->nextFg = 1.f;
    impl_->nextFb = 1.f;
    impl_->nextFa = 1.f;
    impl_->nextBr = 1.f;
    impl_->nextBg = 1.f;
    impl_->nextBb = 1.f;
    impl_->nextBa = 0.f;
    impl_->nextBorderThick = 0.f;
    impl_->nextSegments = 64;
}

void Eclipse::SetNextFillColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextFr = r;
    impl_->nextFg = g;
    impl_->nextFb = b;
    impl_->nextFa = a;
}

void Eclipse::SetNextBorderColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextBr = r;
    impl_->nextBg = g;
    impl_->nextBb = b;
    impl_->nextBa = a;
}

void Eclipse::SetNextBorderThickness(float thicknessPixels) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (thicknessPixels >= 0.f)
        impl_->nextBorderThick = thicknessPixels;
}

void Eclipse::SetNextSegmentCount(int segments) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextSegments = std::max(8, std::min(256, segments));
}

void Eclipse::Circle(float centerX, float centerY, float radius) noexcept
{
    Ellipse(centerX, centerY, radius, radius, 0.f);
}

void Eclipse::Ellipse(float centerX, float centerY, float radiusX, float radiusY, float rotationDegrees) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (radiusX <= 0.f || radiusY <= 0.f)
        return;
    eclipse_detail::EllipseCmd c{};
    c.cx = centerX;
    c.cy = centerY;
    c.rx = radiusX;
    c.ry = radiusY;
    c.rotDeg = rotationDegrees;
    c.segments = impl_->nextSegments;
    c.fr = impl_->nextFr;
    c.fg = impl_->nextFg;
    c.fb = impl_->nextFb;
    c.fa = impl_->nextFa;
    c.br = impl_->nextBr;
    c.bg = impl_->nextBg;
    c.bb = impl_->nextBb;
    c.ba = impl_->nextBa;
    c.borderThick = impl_->nextBorderThick;
    impl_->queue.push_back(c);
}

void Eclipse::End(ApplicationContextSettings & ctx) noexcept
{
    if (!impl_)
        return;
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!impl_->recording)
        return;
    impl_->recording = false;
    if (impl_->queue.empty())
        return;

#if !defined(__APPLE__)
    if (!LoadGL3Procs())
        return;
#endif

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    std::vector<float> interleaved;
    eclipse_detail::appendQueueTransformed(
        impl_->queue,
        impl_->pivotX,
        impl_->pivotY,
        impl_->scaleX,
        impl_->scaleY,
        impl_->rotDeg,
        impl_->entityX,
        impl_->entityY,
        fbw,
        fbh,
        interleaved);
    if (interleaved.empty())
        return;

    if (!impl_->fillProgram)
        impl_->fillProgram = createFillProgramGl3();
    if (!impl_->fillProgram)
        return;

#if defined(__APPLE__)
    if (!impl_->fillVao) {
        glGenVertexArrays(1, &impl_->fillVao);
        glGenBuffers(1, &impl_->fillVbo);
        glBindVertexArray(impl_->fillVao);
        glBindBuffer(GL_ARRAY_BUFFER, impl_->fillVbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(0));
        glVertexAttribPointer(
            1, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(8));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->fillProgram);
    glBindVertexArray(impl_->fillVao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->fillVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
        interleaved.data(),
        GL_DYNAMIC_DRAW);
    const GLsizei vcount = static_cast<GLsizei>(interleaved.size() / 6u);
    glDrawArrays(GL_TRIANGLES, 0, vcount);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
#else
    if (!s_glGenVertexArrays)
        return;
    if (!impl_->fillVao) {
        s_glGenVertexArrays(1, &impl_->fillVao);
        s_glGenBuffers(1, &impl_->fillVbo);
        s_glBindVertexArray(impl_->fillVao);
        s_glBindBuffer(GL_ARRAY_BUFFER, impl_->fillVbo);
        s_glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(0));
        s_glVertexAttribPointer(
            1, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), reinterpret_cast<void *>(8));
        s_glEnableVertexAttribArray(0);
        s_glEnableVertexAttribArray(1);
        s_glBindVertexArray(0);
        s_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_glUseProgram(impl_->fillProgram);
    s_glBindVertexArray(impl_->fillVao);
    s_glBindBuffer(GL_ARRAY_BUFFER, impl_->fillVbo);
    s_glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
        interleaved.data(),
        GL_DYNAMIC_DRAW);
    const GLsizei vcount = static_cast<GLsizei>(interleaved.size() / 6u);
    glDrawArrays(GL_TRIANGLES, 0, vcount);
    s_glBindVertexArray(0);
    s_glBindBuffer(GL_ARRAY_BUFFER, 0);
    s_glUseProgram(0);
#endif
}

} // namespace HonkordGL::Graphics

#endif /* !__ANDROID__ */
