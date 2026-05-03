/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Lines implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Lines.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace HonkordGL::Graphics {
namespace lines_detail {

struct StrokeSegment {
    float x0{};
    float y0{};
    float x1{};
    float y1{};
    float r{1.f};
    float g{1.f};
    float b{1.f};
    float a{1.f};
    float thickPx{1.f};
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
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float rx = lsx * c - lsy * s;
    const float ry = lsx * s + lsy * c;
    *wx = pivotX + transX + rx;
    *wy = pivotY + transY + ry;
}
inline float thicknessScale(float scaleX, float scaleY) noexcept
{
    return 0.5f * (std::fabs(scaleX) + std::fabs(scaleY));
}
inline void appendStrokeQuad(std::vector<float> & interleaved, StrokeSegment const & s, int fbw, int fbh) noexcept
{
    const float dx = s.x1 - s.x0;
    const float dy = s.y1 - s.y0;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-12f)
        return;
    const float len = std::sqrt(len2);
    const float nx = (-dy / len) * (s.thickPx * 0.5f);
    const float ny = (dx / len) * (s.thickPx * 0.5f);

    const float x0p = s.x0 + nx;
    const float y0p = s.y0 + ny;
    const float x0m = s.x0 - nx;
    const float y0m = s.y0 - ny;
    const float x1p = s.x1 + nx;
    const float y1p = s.y1 + ny;
    const float x1m = s.x1 - nx;
    const float y1m = s.y1 - ny;

    auto const emit = [&](float px, float py) {
        float ndcX = 0.f;
        float ndcY = 0.f;
        pixelTopLeftToNdc(px, py, fbw, fbh, &ndcX, &ndcY);
        interleaved.push_back(ndcX);
        interleaved.push_back(ndcY);
        interleaved.push_back(s.r);
        interleaved.push_back(s.g);
        interleaved.push_back(s.b);
        interleaved.push_back(s.a);
    };
    emit(x0p, y0p);
    emit(x0m, y0m);
    emit(x1m, y1m);
    emit(x0p, y0p);
    emit(x1m, y1m);
    emit(x1p, y1p);
}
inline void appendQueueTransformed(
    std::vector<StrokeSegment> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    int fbw,
    int fbh,
    std::vector<float> & interleaved) noexcept
{
    const float tScale = thicknessScale(scaleX, scaleY);
    for (StrokeSegment s : queue) {
        localToWorld(s.x0, s.y0, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x0, &s.y0);
        localToWorld(s.x1, s.y1, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x1, &s.y1);
        if (tScale > 1e-6f)
            s.thickPx *= tScale;
        appendStrokeQuad(interleaved, s, fbw, fbh);
    }
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

inline void strokeWorldCorners(
    StrokeSegment s,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float * ox0,
    float * oy0,
    float * ox1,
    float * oy1,
    float * ox2,
    float * oy2,
    float * ox3,
    float * oy3) noexcept
{
    localToWorld(s.x0, s.y0, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x0, &s.y0);
    localToWorld(s.x1, s.y1, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x1, &s.y1);
    const float tScale = thicknessScale(scaleX, scaleY);
    if (tScale > 1e-6f)
        s.thickPx *= tScale;
    const float dx = s.x1 - s.x0;
    const float dy = s.y1 - s.y0;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-12f) {
        *ox0 = *ox1 = *ox2 = *ox3 = s.x0;
        *oy0 = *oy1 = *oy2 = *oy3 = s.y0;
        return;
    }
    const float len = std::sqrt(len2);
    const float nx = (-dy / len) * (s.thickPx * 0.5f);
    const float ny = (dx / len) * (s.thickPx * 0.5f);
    *ox0 = s.x0 + nx;
    *oy0 = s.y0 + ny;
    *ox1 = s.x0 - nx;
    *oy1 = s.y0 - ny;
    *ox2 = s.x1 - nx;
    *oy2 = s.y1 - ny;
    *ox3 = s.x1 + nx;
    *oy3 = s.y1 + ny;
}

inline void strokeExpandWorldAabb(
    StrokeSegment const & seg,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    bool & hasAny,
    float & minX,
    float & minY,
    float & maxX,
    float & maxY) noexcept
{
    StrokeSegment s = seg;
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    float x3 = 0.f;
    float y3 = 0.f;
    strokeWorldCorners(s, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &x0, &y0, &x1, &y1, &x2, &y2, &x3, &y3);
    const float xs[4] = { x0, x1, x2, x3 };
    const float ys[4] = { y0, y1, y2, y3 };
    for (int i = 0; i < 4; ++i) {
        if (!hasAny) {
            minX = maxX = xs[i];
            minY = maxY = ys[i];
            hasAny = true;
        } else {
            expandAabb(minX, minY, maxX, maxY, xs[i], ys[i]);
        }
    }
}

inline bool queueWorldAabb(
    std::vector<StrokeSegment> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float & outMinX,
    float & outMinY,
    float & outMaxX,
    float & outMaxY) noexcept
{
    bool hasAny = false;
    for (StrokeSegment const & seg : queue)
        strokeExpandWorldAabb(seg, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, hasAny, outMinX, outMinY, outMaxX, outMaxY);
    return hasAny;
}

inline bool strokeContainsWorldPoint(
    StrokeSegment s,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float px,
    float py) noexcept
{
    localToWorld(s.x0, s.y0, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x0, &s.y0);
    localToWorld(s.x1, s.y1, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, &s.x1, &s.y1);
    const float tScale = thicknessScale(scaleX, scaleY);
    if (tScale > 1e-6f)
        s.thickPx *= tScale;
    const float dx = s.x1 - s.x0;
    const float dy = s.y1 - s.y0;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-12f)
        return false;
    const float len = std::sqrt(len2);
    const float half = s.thickPx * 0.5f;
    const float t = std::clamp(((px - s.x0) * dx + (py - s.y0) * dy) / len2, 0.f, 1.f);
    const float cx = s.x0 + t * dx;
    const float cy = s.y0 + t * dy;
    const float ex = px - cx;
    const float ey = py - cy;
    return ex * ex + ey * ey <= half * half + 1e-5f;
}

inline bool linesIntersectsLines(
    std::vector<StrokeSegment> const & qa,
    float apx,
    float apy,
    float asx,
    float asy,
    float arot,
    float atx,
    float aty,
    std::vector<StrokeSegment> const & qb,
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

inline bool linesContainsPoint(
    std::vector<StrokeSegment> const & queue,
    float pivotX,
    float pivotY,
    float scaleX,
    float scaleY,
    float rotDeg,
    float transX,
    float transY,
    float px,
    float py) noexcept
{
    if (queue.empty())
        return false;
    for (StrokeSegment const & seg : queue) {
        if (strokeContainsWorldPoint(seg, pivotX, pivotY, scaleX, scaleY, rotDeg, transX, transY, px, py))
            return true;
    }
    return false;
}

} // namespace lines_detail
} // namespace HonkordGL::Graphics

#if defined(__ANDROID__)

#include <GLES2/gl2.h>

namespace HonkordGL::Graphics 
{
namespace 
{
static const char kVsEs2Line[] = R"(attribute vec2 a_pos;
attribute vec4 a_color;
varying vec4 v_color;
void main() {
  v_color = a_color;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";
static const char kFsEs2Line[] = R"(precision mediump float;
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
static GLuint linkLineProgramEs2()
{
    const GLuint vs = compileShaderEs2(GL_VERTEX_SHADER, kVsEs2Line);
    const GLuint fs = compileShaderEs2(GL_FRAGMENT_SHADER, kFsEs2Line);
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

struct Lines::Impl {
    std::vector<lines_detail::StrokeSegment> queue{};
    float pathX{0.f};
    float pathY{0.f};
    bool hasPathPoint{false};
    bool recording{false};
    float nextR{1.f};
    float nextG{1.f};
    float nextB{1.f};
    float nextA{1.f};
    float nextThick{1.f};
    float pivotX{0.f};
    float pivotY{0.f};
    float entityX{0.f};
    float entityY{0.f};
    float rotDeg{0.f};
    float scaleX{1.f};
    float scaleY{1.f};
    unsigned int lineProgram{0};
    unsigned int lineVbo{0};
};

Lines::Lines() noexcept
    : impl_(std::make_unique<Impl>())
{
}
Lines::~Lines()
{
    Destroy();
}
Lines::Lines(Lines&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

Lines& Lines::operator=(Lines&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void Lines::Destroy() noexcept
{
    if (!impl_)
        return;
    if (impl_->lineVbo)
        glDeleteBuffers(1, &impl_->lineVbo);
    if (impl_->lineProgram)
        glDeleteProgram(impl_->lineProgram);
    impl_->lineVbo = 0;
    impl_->lineProgram = 0;
    impl_->queue.clear();
    impl_->recording = false;
    impl_->hasPathPoint = false;
}
void Lines::SetEntityPivot(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = x;
    impl_->pivotY = y;
}
void Lines::SetEntityPosition(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->entityX = x;
    impl_->entityY = y;
}

void Lines::SetEntityRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->rotDeg = degrees;
}
void Lines::SetEntityScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = sx;
    impl_->scaleY = sy;
}

void Lines::SetEntityScale(float uniform) noexcept
{
    SetEntityScale(uniform, uniform);
}

void Lines::DilateEntity(float uniformScale) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX *= uniformScale;
    impl_->scaleY *= uniformScale;
}
void Lines::FlipEntityX() noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = -impl_->scaleX;
}
void Lines::FlipEntityY() noexcept
{
    if (!impl_)
        return;
    impl_->scaleY = -impl_->scaleY;
}
void Lines::ResetEntityTransform() noexcept
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
float Lines::EntityPivotX() const noexcept
{
    return impl_ ? impl_->pivotX : 0.f;
}
float Lines::EntityPivotY() const noexcept
{
    return impl_ ? impl_->pivotY : 0.f;
}
float Lines::EntityPositionX() const noexcept
{
    return impl_ ? impl_->entityX : 0.f;
}
float Lines::EntityPositionY() const noexcept
{
    return impl_ ? impl_->entityY : 0.f;
}
float Lines::EntityRotationDegrees() const noexcept
{
    return impl_ ? impl_->rotDeg : 0.f;
}
float Lines::EntityScaleX() const noexcept
{
    return impl_ ? impl_->scaleX : 1.f;
}
float Lines::EntityScaleY() const noexcept
{
    return impl_ ? impl_->scaleY : 1.f;
}

bool Lines::IntersectsEntityBounds(Lines const & other) const noexcept
{
    if (!impl_ || !other.impl_)
        return false;
    return lines_detail::linesIntersectsLines(
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

bool Lines::ContainsClientPoint(float x, float y) const noexcept
{
    if (!impl_)
        return false;
    return lines_detail::linesContainsPoint(
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

void Lines::Begin() noexcept
{
    if (!impl_)
        return;
    impl_->queue.clear();
    impl_->hasPathPoint = false;
    impl_->recording = true;
    impl_->nextR = 1.f;
    impl_->nextG = 1.f;
    impl_->nextB = 1.f;
    impl_->nextA = 1.f;
    impl_->nextThick = 1.f;
}
void Lines::SetNextLineColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextR = r;
    impl_->nextG = g;
    impl_->nextB = b;
    impl_->nextA = a;
}
void Lines::SetNextLineThickness(float thicknessPixels) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (thicknessPixels > 0.f)
        impl_->nextThick = thicknessPixels;
}
void Lines::MoveTo(float x, float y) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->pathX = x;
    impl_->pathY = y;
    impl_->hasPathPoint = true;
}
void Lines::LineTo(float x, float y) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    lines_detail::StrokeSegment s{};
    s.x0 = impl_->pathX;
    s.y0 = impl_->pathY;
    s.x1 = x;
    s.y1 = y;
    s.r = impl_->nextR;
    s.g = impl_->nextG;
    s.b = impl_->nextB;
    s.a = impl_->nextA;
    s.thickPx = impl_->nextThick;
    impl_->queue.push_back(s);
    impl_->pathX = x;
    impl_->pathY = y;
}
void Lines::LineRelative(float dx, float dy) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    LineTo(impl_->pathX + dx, impl_->pathY + dy);
}
void Lines::LinePolar(float length, float angleDegrees) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    if (length <= 0.f)
        return;
    const float rad = angleDegrees * (3.14159265358979323846f / 180.f);
    const float x = impl_->pathX + length * std::cos(rad);
    const float y = impl_->pathY + length * std::sin(rad);
    LineTo(x, y);
}
void Lines::LineSegment(float x0, float y0, float x1, float y1) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    lines_detail::StrokeSegment s{};
    s.x0 = x0;
    s.y0 = y0;
    s.x1 = x1;
    s.y1 = y1;
    s.r = impl_->nextR;
    s.g = impl_->nextG;
    s.b = impl_->nextB;
    s.a = impl_->nextA;
    s.thickPx = impl_->nextThick;
    impl_->queue.push_back(s);
    impl_->pathX = x1;
    impl_->pathY = y1;
    impl_->hasPathPoint = true;
}
void Lines::End(ApplicationContextSettings & ctx) noexcept
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
    lines_detail::appendQueueTransformed(
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

    if (!impl_->lineProgram)
        impl_->lineProgram = linkLineProgramEs2();
    if (!impl_->lineProgram)
        return;

    if (!impl_->lineVbo)
        glGenBuffers(1, &impl_->lineVbo);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->lineProgram);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
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

static const char kVsLine[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;
out vec4 v_color;
void main() {
  v_color = a_color;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";
static const char kFsLine[] = R"(#version 330 core
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

static GLuint createLineProgramGl3() noexcept
{
#if defined(__APPLE__)
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsLine);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsLine);
    if (!vs || !fs)
        return 0;
    const GLuint p = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
#else
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsLine);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsLine);
    if (!vs || !fs)
        return 0;
    const GLuint p = linkProgram(vs, fs);
    s_glDeleteShader(vs);
    s_glDeleteShader(fs);
    return p;
#endif
}

} // namespace

struct Lines::Impl {
    std::vector<lines_detail::StrokeSegment> queue{};
    float pathX{0.f};
    float pathY{0.f};
    bool hasPathPoint{false};
    bool recording{false};
    float nextR{1.f};
    float nextG{1.f};
    float nextB{1.f};
    float nextA{1.f};
    float nextThick{1.f};
    float pivotX{0.f};
    float pivotY{0.f};
    float entityX{0.f};
    float entityY{0.f};
    float rotDeg{0.f};
    float scaleX{1.f};
    float scaleY{1.f};
    unsigned int lineProgram{0};
    unsigned int lineVao{0};
    unsigned int lineVbo{0};
};

Lines::Lines() noexcept
    : impl_(std::make_unique<Impl>())
{
}
Lines::~Lines()
{
    Destroy();
}
Lines::Lines(Lines&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

Lines& Lines::operator=(Lines&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void Lines::Destroy() noexcept
{
    if (!impl_)
        return;
#if defined(__APPLE__)
    if (impl_->lineVbo)
        glDeleteBuffers(1, &impl_->lineVbo);
    if (impl_->lineVao)
        glDeleteVertexArrays(1, &impl_->lineVao);
    if (impl_->lineProgram)
        glDeleteProgram(impl_->lineProgram);
#else
    if (s_glGenVertexArrays) {
        if (impl_->lineVbo)
            s_glDeleteBuffers(1, &impl_->lineVbo);
        if (impl_->lineVao)
            s_glDeleteVertexArrays(1, &impl_->lineVao);
        if (impl_->lineProgram)
            s_glDeleteProgram(impl_->lineProgram);
    }
#endif
    impl_->lineVbo = 0;
    impl_->lineVao = 0;
    impl_->lineProgram = 0;
    impl_->queue.clear();
    impl_->recording = false;
    impl_->hasPathPoint = false;
}
void Lines::SetEntityPivot(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->pivotX = x;
    impl_->pivotY = y;
}
void Lines::SetEntityPosition(float x, float y) noexcept
{
    if (!impl_)
        return;
    impl_->entityX = x;
    impl_->entityY = y;
}
void Lines::SetEntityRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->rotDeg = degrees;
}
void Lines::SetEntityScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = sx;
    impl_->scaleY = sy;
}
void Lines::SetEntityScale(float uniform) noexcept
{
    SetEntityScale(uniform, uniform);
}
void Lines::DilateEntity(float uniformScale) noexcept
{
    if (!impl_)
        return;
    impl_->scaleX *= uniformScale;
    impl_->scaleY *= uniformScale;
}
void Lines::FlipEntityX() noexcept
{
    if (!impl_)
        return;
    impl_->scaleX = -impl_->scaleX;
}
void Lines::FlipEntityY() noexcept
{
    if (!impl_)
        return;
    impl_->scaleY = -impl_->scaleY;
}
void Lines::ResetEntityTransform() noexcept
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
float Lines::EntityPivotX() const noexcept
{
    return impl_ ? impl_->pivotX : 0.f;
}
float Lines::EntityPivotY() const noexcept
{
    return impl_ ? impl_->pivotY : 0.f;
}
float Lines::EntityPositionX() const noexcept
{
    return impl_ ? impl_->entityX : 0.f;
}
float Lines::EntityPositionY() const noexcept
{
    return impl_ ? impl_->entityY : 0.f;
}
float Lines::EntityRotationDegrees() const noexcept
{
    return impl_ ? impl_->rotDeg : 0.f;
}
float Lines::EntityScaleX() const noexcept
{
    return impl_ ? impl_->scaleX : 1.f;
}
float Lines::EntityScaleY() const noexcept
{
    return impl_ ? impl_->scaleY : 1.f;
}

bool Lines::IntersectsEntityBounds(Lines const & other) const noexcept
{
    if (!impl_ || !other.impl_)
        return false;
    return lines_detail::linesIntersectsLines(
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

bool Lines::ContainsClientPoint(float x, float y) const noexcept
{
    if (!impl_)
        return false;
    return lines_detail::linesContainsPoint(
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

void Lines::Begin() noexcept
{
    if (!impl_)
        return;
    impl_->queue.clear();
    impl_->hasPathPoint = false;
    impl_->recording = true;
    impl_->nextR = 1.f;
    impl_->nextG = 1.f;
    impl_->nextB = 1.f;
    impl_->nextA = 1.f;
    impl_->nextThick = 1.f;
}
void Lines::SetNextLineColor(float r, float g, float b, float a) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->nextR = r;
    impl_->nextG = g;
    impl_->nextB = b;
    impl_->nextA = a;
}
void Lines::SetNextLineThickness(float thicknessPixels) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    if (thicknessPixels > 0.f)
        impl_->nextThick = thicknessPixels;
}
void Lines::MoveTo(float x, float y) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    impl_->pathX = x;
    impl_->pathY = y;
    impl_->hasPathPoint = true;
}
void Lines::LineTo(float x, float y) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    lines_detail::StrokeSegment s{};
    s.x0 = impl_->pathX;
    s.y0 = impl_->pathY;
    s.x1 = x;
    s.y1 = y;
    s.r = impl_->nextR;
    s.g = impl_->nextG;
    s.b = impl_->nextB;
    s.a = impl_->nextA;
    s.thickPx = impl_->nextThick;
    impl_->queue.push_back(s);
    impl_->pathX = x;
    impl_->pathY = y;
}
void Lines::LineRelative(float dx, float dy) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    LineTo(impl_->pathX + dx, impl_->pathY + dy);
}
void Lines::LinePolar(float length, float angleDegrees) noexcept
{
    if (!impl_ || !impl_->recording || !impl_->hasPathPoint)
        return;
    if (length <= 0.f)
        return;
    const float rad = angleDegrees * (3.14159265358979323846f / 180.f);
    const float x = impl_->pathX + length * std::cos(rad);
    const float y = impl_->pathY + length * std::sin(rad);
    LineTo(x, y);
}
void Lines::LineSegment(float x0, float y0, float x1, float y1) noexcept
{
    if (!impl_ || !impl_->recording)
        return;
    lines_detail::StrokeSegment s{};
    s.x0 = x0;
    s.y0 = y0;
    s.x1 = x1;
    s.y1 = y1;
    s.r = impl_->nextR;
    s.g = impl_->nextG;
    s.b = impl_->nextB;
    s.a = impl_->nextA;
    s.thickPx = impl_->nextThick;
    impl_->queue.push_back(s);
    impl_->pathX = x1;
    impl_->pathY = y1;
    impl_->hasPathPoint = true;
}
void Lines::End(ApplicationContextSettings & ctx) noexcept
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
    lines_detail::appendQueueTransformed(
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

    if (!impl_->lineProgram)
        impl_->lineProgram = createLineProgramGl3();
    if (!impl_->lineProgram)
        return;

#if defined(__APPLE__)
    if (!impl_->lineVao) {
        glGenVertexArrays(1, &impl_->lineVao);
        glGenBuffers(1, &impl_->lineVbo);
        glBindVertexArray(impl_->lineVao);
        glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
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
    glUseProgram(impl_->lineProgram);
    glBindVertexArray(impl_->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
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
    if (!impl_->lineVao) {
        s_glGenVertexArrays(1, &impl_->lineVao);
        s_glGenBuffers(1, &impl_->lineVbo);
        s_glBindVertexArray(impl_->lineVao);
        s_glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
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
    s_glUseProgram(impl_->lineProgram);
    s_glBindVertexArray(impl_->lineVao);
    s_glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
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