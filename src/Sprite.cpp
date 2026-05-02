/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Sprite implementation (OpenGL 3.3 desktop / OpenGL ES 2 Android)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Sprite.h>

#include <HonkordGL/Eclipse.h>
#include <HonkordGL/Lines.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <HonkordGL/third_party/stb_image.h>

namespace HonkordGL::Graphics {
namespace sprite_detail {

inline void ndcFromPixel(float px, float py, int fbw, int fbh, float * outX, float * outY) noexcept
{
    if (fbw <= 0 || fbh <= 0) {
        *outX = 0.f;
        *outY = 0.f;
        return;
    }
    *outX = (2.f * px / static_cast<float>(fbw)) - 1.f;
    *outY = (2.f * py / static_cast<float>(fbh)) - 1.f;
}

/**
 * Builds interleaved pos+uv for two triangles; rotation (deg CCW) and scale apply around `pivot` in pixel space.
 * Pivot: if `custom_pivot` is false, uses quad center; if true, `(px + pivot_lx, py + pivot_ly)` in client space
 * (y down from top) maps to the rotation center in GL space.
 */
inline void fillSpriteVertices(
    float px,
    float py,
    float sw,
    float sh,
    int fbw,
    int fbh,
    int tw,
    int th,
    int sx,
    int sy,
    int sw_tex,
    int sh_tex,
    float rot_deg,
    float scale_x,
    float scale_y,
    bool custom_pivot,
    float pivot_lx,
    float pivot_ly,
    float v[24]) noexcept
{
    const float gl_y0 = static_cast<float>(fbh) - py - sh;
    const float gl_y1 = static_cast<float>(fbh) - py;
    float cx = px + sw * 0.5f;
    float cy = (gl_y0 + gl_y1) * 0.5f;
    if (custom_pivot) {
        cx = px + pivot_lx;
        cy = static_cast<float>(fbh) - py - pivot_ly;
    }
    const float rad = rot_deg * (3.14159265358979323846f / 180.f);
    const float cr = std::cos(rad);
    const float sr = std::sin(rad);

    const float corners[4][2] = {
        { px, gl_y0 },
        { px + sw, gl_y0 },
        { px + sw, gl_y1 },
        { px, gl_y1 },
    };

    float ndc_x[4];
    float ndc_y[4];
    for (int i = 0; i < 4; ++i) {
        const float dx = (corners[i][0] - cx) * scale_x;
        const float dy = (corners[i][1] - cy) * scale_y;
        const float rx = dx * cr - dy * sr;
        const float ry = dx * sr + dy * cr;
        ndcFromPixel(cx + rx, cy + ry, fbw, fbh, &ndc_x[i], &ndc_y[i]);
    }

    const float u_left = static_cast<float>(sx) / static_cast<float>(tw);
    const float u_right = static_cast<float>(sx + sw_tex) / static_cast<float>(tw);
    const float vy_bottom = 1.f - static_cast<float>(sy + sh_tex) / static_cast<float>(th);
    const float vy_top = 1.f - static_cast<float>(sy) / static_cast<float>(th);

    v[0] = ndc_x[0];
    v[1] = ndc_y[0];
    v[2] = u_left;
    v[3] = vy_bottom;
    v[4] = ndc_x[1];
    v[5] = ndc_y[1];
    v[6] = u_right;
    v[7] = vy_bottom;
    v[8] = ndc_x[2];
    v[9] = ndc_y[2];
    v[10] = u_right;
    v[11] = vy_top;
    v[12] = ndc_x[0];
    v[13] = ndc_y[0];
    v[14] = u_left;
    v[15] = vy_bottom;
    v[16] = ndc_x[2];
    v[17] = ndc_y[2];
    v[18] = u_right;
    v[19] = vy_top;
    v[20] = ndc_x[3];
    v[21] = ndc_y[3];
    v[22] = u_left;
    v[23] = vy_top;
}

/**
 * Client-space quad corners matching `fillSpriteVertices` / `Draw`. Intermediate GL math uses a
 * reference framebuffer height; translation along y cancels so the result is independent of the
 * real `fbh` value.
 */
inline void spriteQuadCornersClient(
    float px,
    float py,
    float sw,
    float sh,
    float rot_deg,
    float scale_x,
    float scale_y,
    bool custom_pivot,
    float pivot_lx,
    float pivot_ly,
    float outX[4],
    float outY[4]) noexcept
{
    constexpr float kRefH = 8192.f;
    const int fbh = static_cast<int>(kRefH);
    const float gl_y0 = static_cast<float>(fbh) - py - sh;
    const float gl_y1 = static_cast<float>(fbh) - py;
    float cx = px + sw * 0.5f;
    float cy = (gl_y0 + gl_y1) * 0.5f;
    if (custom_pivot) {
        cx = px + pivot_lx;
        cy = static_cast<float>(fbh) - py - pivot_ly;
    }
    const float rad = rot_deg * (3.14159265358979323846f / 180.f);
    const float cr = std::cos(rad);
    const float sr = std::sin(rad);

    const float corners[4][2] = {
        { px, gl_y0 },
        { px + sw, gl_y0 },
        { px + sw, gl_y1 },
        { px, gl_y1 },
    };

    for (int i = 0; i < 4; ++i) {
        const float dx = (corners[i][0] - cx) * scale_x;
        const float dy = (corners[i][1] - cy) * scale_y;
        const float rx = dx * cr - dy * sr;
        const float ry = dx * sr + dy * cr;
        const float glx = cx + rx;
        const float gly = cy + ry;
        outX[i] = glx;
        outY[i] = static_cast<float>(fbh) - gly;
    }
}

inline bool resolveSpriteDrawQuad(
    int tw,
    int th,
    float posX,
    float posY,
    float drawW,
    float drawH,
    bool use_rects,
    Recti const & dst_rect,
    Recti const & src_rect,
    bool use_tex_frame,
    Recti const & tex_frame,
    float & out_px,
    float & out_py,
    float & out_sw,
    float & out_sh) noexcept
{
    if (tw <= 0 || th <= 0)
        return false;

    float px = 0.f;
    float py = 0.f;
    float sw = 0.f;
    float sh = 0.f;
    int sx = 0;
    int sy = 0;
    int sw_tex = tw;
    int sh_tex = th;

    if (use_rects) {
        px = static_cast<float>(dst_rect.x);
        py = static_cast<float>(dst_rect.y);
        sw = static_cast<float>(dst_rect.w);
        sh = static_cast<float>(dst_rect.z);
        sx = src_rect.x;
        sy = src_rect.y;
        sw_tex = src_rect.w;
        sh_tex = src_rect.z;
    } else if (use_tex_frame) {
        sx = tex_frame.x;
        sy = tex_frame.y;
        sw_tex = tex_frame.w;
        sh_tex = tex_frame.z;
        sw = drawW > 0.f ? drawW : static_cast<float>(sw_tex);
        sh = drawH > 0.f ? drawH : static_cast<float>(sh_tex);
        px = posX;
        py = posY;
    } else {
        sw = drawW > 0.f ? drawW : static_cast<float>(tw);
        sh = drawH > 0.f ? drawH : static_cast<float>(th);
        px = posX;
        py = posY;
        sx = 0;
        sy = 0;
        sw_tex = tw;
        sh_tex = th;
    }

    if (sw <= 0.f || sh <= 0.f)
        return false;

    if (sx < 0) {
        sw_tex += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh_tex += sy;
        sy = 0;
    }
    if (sx >= tw || sy >= th)
        return false;
    sw_tex = std::min(sw_tex, tw - sx);
    sh_tex = std::min(sh_tex, th - sy);
    if (sw_tex <= 0 || sh_tex <= 0)
        return false;

    out_px = px;
    out_py = py;
    out_sw = sw;
    out_sh = sh;
    return true;
}

inline float cross2d(float ax, float ay, float bx, float by) noexcept
{
    return ax * by - ay * bx;
}

inline bool pointInConvexQuad(float px, float py, float const qx[4], float const qy[4]) noexcept
{
    int sign = 0;
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        const float c = cross2d(qx[j] - qx[i], qy[j] - qy[i], px - qx[i], py - qy[i]);
        if (c > 1e-5f) {
            if (sign < 0)
                return false;
            sign = 1;
        } else if (c < -1e-5f) {
            if (sign > 0)
                return false;
            sign = -1;
        }
    }
    return true;
}

inline bool axisSeparatesQuads(
    float nx,
    float ny,
    float const ax[4],
    float const ay[4],
    float const bx[4],
    float const by[4]) noexcept
{
    if (nx * nx + ny * ny < 1e-24f)
        return false;
    float aMin = 1e30f;
    float aMax = -1e30f;
    for (int i = 0; i < 4; ++i) {
        const float p = ax[i] * nx + ay[i] * ny;
        aMin = std::min(aMin, p);
        aMax = std::max(aMax, p);
    }
    float bMin = 1e30f;
    float bMax = -1e30f;
    for (int i = 0; i < 4; ++i) {
        const float p = bx[i] * nx + by[i] * ny;
        bMin = std::min(bMin, p);
        bMax = std::max(bMax, p);
    }
    return aMax < bMin || bMax < aMin;
}

inline bool satQuadsOverlap(float const ax[4], float const ay[4], float const bx[4], float const by[4]) noexcept
{
    for (int pass = 0; pass < 2; ++pass) {
        float const * xs = pass ? bx : ax;
        float const * ys = pass ? by : ay;
        for (int i = 0; i < 4; ++i) {
            const int j = (i + 1) % 4;
            const float ex = xs[j] - xs[i];
            const float ey = ys[j] - ys[i];
            const float nx = -ey;
            const float ny = ex;
            if (axisSeparatesQuads(nx, ny, ax, ay, bx, by))
                return false;
        }
    }
    return true;
}

inline bool spriteQuadsOverlapResolved(
    float apx,
    float apy,
    float asw,
    float ash,
    float arot,
    float ascaleX,
    float ascaleY,
    bool acustomPivot,
    float apivX,
    float apivY,
    float bpx,
    float bpy,
    float bsw,
    float bsh,
    float brot,
    float bscaleX,
    float bscaleY,
    bool bcustomPivot,
    float bpivX,
    float bpivY) noexcept
{
    float ax[4];
    float ay[4];
    float bx[4];
    float by[4];
    spriteQuadCornersClient(
        apx, apy, asw, ash, arot, ascaleX, ascaleY, acustomPivot, apivX, apivY, ax, ay);
    spriteQuadCornersClient(
        bpx, bpy, bsw, bsh, brot, bscaleX, bscaleY, bcustomPivot, bpivX, bpivY, bx, by);
    return satQuadsOverlap(ax, ay, bx, by);
}

inline void pixelTopLeftToNdc(float px, float pyTop, int fbw, int fbh, float * outX, float * outY) noexcept
{
    const float glY = static_cast<float>(fbh) - pyTop;
    *outX = (2.f * px / static_cast<float>(fbw)) - 1.f;
    *outY = (2.f * glY / static_cast<float>(fbh)) - 1.f;
}

struct FillTriangleCmd {
    float x0{};
    float y0{};
    float x1{};
    float y1{};
    float x2{};
    float y2{};
    float r{1.f};
    float g{1.f};
    float b{1.f};
    float a{1.f};
};

struct CircleCmd {
    float cx{};
    float cy{};
    float radius{};
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

using ShapeCmdVariant = std::variant<FillTriangleCmd, CircleCmd>;

inline void appendFillTriangle(
    std::vector<float> & out, FillTriangleCmd const & t, int fbw, int fbh) noexcept
{
    auto const vert = [&](float px, float py) {
        float ndcX = 0.f;
        float ndcY = 0.f;
        pixelTopLeftToNdc(px, py, fbw, fbh, &ndcX, &ndcY);
        out.push_back(ndcX);
        out.push_back(ndcY);
        out.push_back(t.r);
        out.push_back(t.g);
        out.push_back(t.b);
        out.push_back(t.a);
    };
    vert(t.x0, t.y0);
    vert(t.x1, t.y1);
    vert(t.x2, t.y2);
}

inline void appendCircleFilled(std::vector<float> & out, CircleCmd const & c, int fbw, int fbh) noexcept
{
    if (c.radius <= 0.f || c.fa <= 1e-6f)
        return;
    constexpr int kSeg = 36;
    float cxNdc = 0.f;
    float cyNdc = 0.f;
    pixelTopLeftToNdc(c.cx, c.cy, fbw, fbh, &cxNdc, &cyNdc);
    for (int i = 0; i < kSeg; ++i) {
        const float t0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(kSeg);
        const float t1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(kSeg);
        const float x0 = c.cx + c.radius * std::cos(t0);
        const float y0 = c.cy + c.radius * std::sin(t0);
        const float x1 = c.cx + c.radius * std::cos(t1);
        const float y1 = c.cy + c.radius * std::sin(t1);
        float nx0 = 0.f;
        float ny0 = 0.f;
        float nx1 = 0.f;
        float ny1 = 0.f;
        pixelTopLeftToNdc(x0, y0, fbw, fbh, &nx0, &ny0);
        pixelTopLeftToNdc(x1, y1, fbw, fbh, &nx1, &ny1);
        out.push_back(cxNdc);
        out.push_back(cyNdc);
        out.push_back(c.fr);
        out.push_back(c.fg);
        out.push_back(c.fb);
        out.push_back(c.fa);
        out.push_back(nx0);
        out.push_back(ny0);
        out.push_back(c.fr);
        out.push_back(c.fg);
        out.push_back(c.fb);
        out.push_back(c.fa);
        out.push_back(nx1);
        out.push_back(ny1);
        out.push_back(c.fr);
        out.push_back(c.fg);
        out.push_back(c.fb);
        out.push_back(c.fa);
    }
}

inline void appendCircleBorderRing(std::vector<float> & out, CircleCmd const & c, int fbw, int fbh) noexcept
{
    if (c.borderThick <= 0.f || c.ba <= 1e-6f)
        return;
    const float ri = std::max(0.25f, c.radius - c.borderThick * 0.5f);
    const float ro = c.radius + c.borderThick * 0.5f;
    constexpr int kSeg = 40;
    for (int i = 0; i < kSeg; ++i) {
        const float t0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(kSeg);
        const float t1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(kSeg);
        const float ix0 = c.cx + ri * std::cos(t0);
        const float iy0 = c.cy + ri * std::sin(t0);
        const float ix1 = c.cx + ri * std::cos(t1);
        const float iy1 = c.cy + ri * std::sin(t1);
        const float ox0 = c.cx + ro * std::cos(t0);
        const float oy0 = c.cy + ro * std::sin(t0);
        const float ox1 = c.cx + ro * std::cos(t1);
        const float oy1 = c.cy + ro * std::sin(t1);
        auto const emit = [&](float px, float py) {
            float nx = 0.f;
            float ny = 0.f;
            pixelTopLeftToNdc(px, py, fbw, fbh, &nx, &ny);
            out.push_back(nx);
            out.push_back(ny);
            out.push_back(c.br);
            out.push_back(c.bg);
            out.push_back(c.bb);
            out.push_back(c.ba);
        };
        emit(ix0, iy0);
        emit(ox0, oy0);
        emit(ox1, oy1);
        emit(ix0, iy0);
        emit(ox1, oy1);
        emit(ix1, iy1);
    }
}

inline void appendShapeQueueToInterleaved(
    std::vector<ShapeCmdVariant> const & queue, int fbw, int fbh, std::vector<float> & interleaved) noexcept
{
    for (ShapeCmdVariant const & cmd : queue) {
        std::visit(
            [&interleaved, fbw, fbh](auto const & c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, FillTriangleCmd>)
                    appendFillTriangle(interleaved, c, fbw, fbh);
                else if constexpr (std::is_same_v<T, CircleCmd>) {
                    if (c.fa > 1e-6f)
                        appendCircleFilled(interleaved, c, fbw, fbh);
                    if (c.borderThick > 0.f && c.ba > 1e-6f)
                        appendCircleBorderRing(interleaved, c, fbw, fbh);
                }
            },
            cmd);
    }
}

} // namespace sprite_detail
} // namespace HonkordGL::Graphics

#if defined(__ANDROID__)

#include <GLES2/gl2.h>

namespace HonkordGL::Graphics {

namespace {

static const char kVsEs2[] = R"(attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char kFsEs2[] = R"(precision mediump float;
varying vec2 v_uv;
uniform sampler2D u_tex;
void main() {
  gl_FragColor = texture2D(u_tex, vec2(v_uv.x, 1.0 - v_uv.y));
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
static GLuint linkProgramEs2(GLuint vs, GLuint fs)
{
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

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

struct Sprite::Impl {
    unsigned int texture{0};
    unsigned int vbo{0};
    unsigned int program{0};
    int texW{0};
    int texH{0};
    float x{0.f};
    float y{0.f};
    float drawW{0.f};
    float drawH{0.f};
    float rotation_degrees{0.f};
    float scale_x{1.f};
    float scale_y{1.f};
    bool use_rects{false};
    Recti dst_rect{};
    Recti src_rect{};

    bool use_tex_frame{false};
    Recti tex_frame{};

    bool use_custom_image_pivot{false};
    float image_pivot_x{0.f};
    float image_pivot_y{0.f};

    std::vector<sprite_detail::ShapeCmdVariant> shapeQueue{};
    bool shapeRecording{false};
    unsigned int lineProgram{0};
    unsigned int lineVbo{0};
};

Sprite::Sprite() noexcept
    : impl_(std::make_unique<Impl>())
{
}
Sprite::~Sprite()
{
    Destroy();
}
Sprite::Sprite(Sprite&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

Sprite& Sprite::operator=(Sprite&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}
void Sprite::Destroy() noexcept
{
    if (!impl_)
        return;
    if (impl_->lineVbo)
        glDeleteBuffers(1, &impl_->lineVbo);
    if (impl_->lineProgram)
        glDeleteProgram(impl_->lineProgram);
    if (impl_->vbo)
        glDeleteBuffers(1, &impl_->vbo);
    if (impl_->texture)
        glDeleteTextures(1, &impl_->texture);
    if (impl_->program)
        glDeleteProgram(impl_->program);
    impl_->texture = 0;
    impl_->vbo = 0;
    impl_->program = 0;
    impl_->lineVbo = 0;
    impl_->lineProgram = 0;
    impl_->texW = 0;
    impl_->texH = 0;
    impl_->use_tex_frame = false;
    impl_->use_custom_image_pivot = false;
    impl_->shapeQueue.clear();
    impl_->shapeRecording = false;
}
bool Sprite::CreateFromRGBA(int width, int height, const std::uint8_t * rgba, int strideBytes) noexcept
{
    if (!rgba || width <= 0 || height <= 0)
        return false;
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    Destroy();

    const int rowStride = strideBytes > 0 ? strideBytes : width * 4;

    const GLuint vs = compileShaderEs2(GL_VERTEX_SHADER, kVsEs2);
    const GLuint fs = compileShaderEs2(GL_FRAGMENT_SHADER, kFsEs2);
    if (!vs || !fs)
        return false;
    const GLuint prog = linkProgramEs2(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog)
        return false;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<std::uint8_t> upload;
    if (rowStride == width * 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        upload.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (int row = 0; row < height; ++row)
            std::memcpy(
                upload.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4u,
                rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(rowStride),
                static_cast<std::size_t>(width) * 4u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    impl_->texture = tex;
    impl_->vbo = vbo;
    impl_->program = prog;
    impl_->texW = width;
    impl_->texH = height;
    impl_->drawW = static_cast<float>(width);
    impl_->drawH = static_cast<float>(height);
    impl_->rotation_degrees = 0.f;
    impl_->scale_x = 1.f;
    impl_->scale_y = 1.f;
    impl_->use_rects = false;
    impl_->use_tex_frame = false;
    impl_->use_custom_image_pivot = false;
    return true;
}
bool Sprite::IsValid() const noexcept
{
    return impl_ && impl_->program != 0 && impl_->texture != 0;
}
void Sprite::SetPosition(float x, float y) noexcept
{
    if (impl_) {
        impl_->use_rects = false;
        impl_->x = x;
        impl_->y = y;
    }
}
void Sprite::SetSize(float width, float height) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    if (width > 0.f)
        impl_->drawW = width;
    if (height > 0.f)
        impl_->drawH = height;
}
void Sprite::SetRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    impl_->rotation_degrees = degrees;
}
void Sprite::SetScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    impl_->scale_x = sx;
    impl_->scale_y = sy;
}
void Sprite::SetScale(float uniform) noexcept
{
    SetScale(uniform, uniform);
}
void Sprite::Draw(ApplicationContextSettings& ctx) noexcept
{
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!IsValid())
        return;

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    const int tw = impl_->texW;
    const int th = impl_->texH;
    float px;
    float py;
    float sw;
    float sh;
    int sx;
    int sy;
    int sw_tex;
    int sh_tex;
    if (impl_->use_rects) {
        px = static_cast<float>(impl_->dst_rect.x);
        py = static_cast<float>(impl_->dst_rect.y);
        sw = static_cast<float>(impl_->dst_rect.w);
        sh = static_cast<float>(impl_->dst_rect.z);
        sx = impl_->src_rect.x;
        sy = impl_->src_rect.y;
        sw_tex = impl_->src_rect.w;
        sh_tex = impl_->src_rect.z;
    } else if (impl_->use_tex_frame) {
        sx = impl_->tex_frame.x;
        sy = impl_->tex_frame.y;
        sw_tex = impl_->tex_frame.w;
        sh_tex = impl_->tex_frame.z;
        sw = impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(sw_tex);
        sh = impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(sh_tex);
        px = impl_->x;
        py = impl_->y;
    } else {
        sw = impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(tw);
        sh = impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(th);
        px = impl_->x;
        py = impl_->y;
        sx = 0;
        sy = 0;
        sw_tex = tw;
        sh_tex = th;
    }

    if (sw <= 0.f || sh <= 0.f)
        return;

    if (sx < 0) {
        sw_tex += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh_tex += sy;
        sy = 0;
    }
    if (sx >= tw || sy >= th)
        return;
    sw_tex = std::min(sw_tex, tw - sx);
    sh_tex = std::min(sh_tex, th - sy);
    if (sw_tex <= 0 || sh_tex <= 0)
        return;

    float v[24];
    sprite_detail::fillSpriteVertices(
        px,
        py,
        sw,
        sh,
        fbw,
        fbh,
        tw,
        th,
        sx,
        sy,
        sw_tex,
        sh_tex,
        impl_->rotation_degrees,
        impl_->scale_x,
        impl_->scale_y,
        impl_->use_custom_image_pivot,
        impl_->image_pivot_x,
        impl_->image_pivot_y,
        v);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->texture);
    const GLint loc = glGetUniformLocation(impl_->program, "u_tex");
    if (loc >= 0)
        glUniform1i(loc, 0);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(4 * sizeof(float)), reinterpret_cast<void *>(0));
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(4 * sizeof(float)), reinterpret_cast<void *>(8));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void Sprite::BeginShape() noexcept
{
    if (!impl_)
        return;
    impl_->shapeQueue.clear();
    impl_->shapeRecording = true;
}

void Sprite::TriangleFill(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float r,
    float g,
    float b,
    float a) noexcept
{
    if (!impl_ || !impl_->shapeRecording)
        return;
    sprite_detail::FillTriangleCmd t{};
    t.x0 = x0;
    t.y0 = y0;
    t.x1 = x1;
    t.y1 = y1;
    t.x2 = x2;
    t.y2 = y2;
    t.r = r;
    t.g = g;
    t.b = b;
    t.a = a;
    impl_->shapeQueue.push_back(t);
}

void Sprite::Circle(
    float centerX,
    float centerY,
    float radius,
    float fillR,
    float fillG,
    float fillB,
    float fillA,
    float borderR,
    float borderG,
    float borderB,
    float borderA,
    float borderThickness) noexcept
{
    if (!impl_ || !impl_->shapeRecording)
        return;
    sprite_detail::CircleCmd c{};
    c.cx = centerX;
    c.cy = centerY;
    c.radius = radius;
    c.fr = fillR;
    c.fg = fillG;
    c.fb = fillB;
    c.fa = fillA;
    c.br = borderR;
    c.bg = borderG;
    c.bb = borderB;
    c.ba = borderA;
    c.borderThick = borderThickness;
    impl_->shapeQueue.push_back(c);
}

void Sprite::EndShape(ApplicationContextSettings & ctx) noexcept
{
    if (!impl_)
        return;
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!impl_->shapeRecording)
        return;
    impl_->shapeRecording = false;
    if (impl_->shapeQueue.empty())
        return;
    if (!IsValid())
        return;

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    std::vector<float> interleaved;
    sprite_detail::appendShapeQueueToInterleaved(impl_->shapeQueue, fbw, fbh, interleaved);
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

int Sprite::TextureWidth() const noexcept
{
    return impl_ ? impl_->texW : 0;
}
int Sprite::TextureHeight() const noexcept
{
    return impl_ ? impl_->texH : 0;
}
float Sprite::PositionX() const noexcept
{
    return impl_ ? impl_->x : 0.f;
}
float Sprite::PositionY() const noexcept
{
    return impl_ ? impl_->y : 0.f;
}
float Sprite::DrawWidth() const noexcept
{
    if (!impl_)
        return 0.f;
    return impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(impl_->texW);
}
float Sprite::DrawHeight() const noexcept
{
    if (!impl_)
        return 0.f;
    return impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(impl_->texH);
}
float Sprite::RotationDegrees() const noexcept
{
    return impl_ ? impl_->rotation_degrees : 0.f;
}
float Sprite::ScaleX() const noexcept
{
    return impl_ ? impl_->scale_x : 1.f;
}
float Sprite::ScaleY() const noexcept
{
    return impl_ ? impl_->scale_y : 1.f;
}

void Sprite::ApplyRects(Recti const& dst, Recti const& src) noexcept
{
    if (!impl_)
        return;
    if (dst.w <= 0 || dst.z <= 0 || src.w <= 0 || src.z <= 0)
        return;
    impl_->use_rects = true;
    impl_->use_tex_frame = false;
    impl_->dst_rect = dst;
    impl_->src_rect = src;
}

void Sprite::SetImageFrame(Recti const& frame) noexcept
{
    if (!impl_ || impl_->texW <= 0 || impl_->texH <= 0)
        return;
    int fx = std::max(0, frame.x);
    int fy = std::max(0, frame.y);
    int fw = frame.w;
    int fh = frame.z;
    if (fw <= 0 || fh <= 0)
        return;
    fw = std::min(fw, impl_->texW - fx);
    fh = std::min(fh, impl_->texH - fy);
    if (fw <= 0 || fh <= 0)
        return;
    impl_->tex_frame.x = fx;
    impl_->tex_frame.y = fy;
    impl_->tex_frame.w = fw;
    impl_->tex_frame.z = fh;
    impl_->use_tex_frame = true;
    impl_->use_rects = false;
}

void Sprite::ClearImageFrame() noexcept
{
    if (!impl_)
        return;
    impl_->use_tex_frame = false;
}

bool Sprite::HasImageFrame() const noexcept
{
    return impl_ && impl_->use_tex_frame;
}

Recti Sprite::ImageFrame() const noexcept
{
    if (!impl_)
        return Recti{};
    if (impl_->use_tex_frame)
        return impl_->tex_frame;
    return Recti{0, 0, impl_->texW, impl_->texH};
}

void Sprite::SetImagePivot(float localXPx, float localYPx) noexcept
{
    if (!impl_)
        return;
    impl_->use_custom_image_pivot = true;
    impl_->image_pivot_x = localXPx;
    impl_->image_pivot_y = localYPx;
}

void Sprite::ResetImagePivot() noexcept
{
    if (!impl_)
        return;
    impl_->use_custom_image_pivot = false;
}

bool Sprite::HasCustomImagePivot() const noexcept
{
    return impl_ && impl_->use_custom_image_pivot;
}

float Sprite::ImagePivotX() const noexcept
{
    return impl_ ? impl_->image_pivot_x : 0.f;
}

float Sprite::ImagePivotY() const noexcept
{
    return impl_ ? impl_->image_pivot_y : 0.f;
}

void Sprite::ApplyImageEntityFrom(Lines const& lines) noexcept
{
    SetPosition(lines.EntityPositionX(), lines.EntityPositionY());
    SetRotationDegrees(lines.EntityRotationDegrees());
    SetScale(lines.EntityScaleX(), lines.EntityScaleY());
    SetImagePivot(lines.EntityPivotX(), lines.EntityPivotY());
}

void Sprite::ApplyImageEntityFrom(Eclipse const& eclipse) noexcept
{
    SetPosition(eclipse.EntityPositionX(), eclipse.EntityPositionY());
    SetRotationDegrees(eclipse.EntityRotationDegrees());
    SetScale(eclipse.EntityScaleX(), eclipse.EntityScaleY());
    SetImagePivot(eclipse.EntityPivotX(), eclipse.EntityPivotY());
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
using PFN_glBufferSubData = void (*)(GLenum, GLintptr, GLsizeiptr, const void *);
using PFN_glVertexAttribPointer = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
using PFN_glEnableVertexAttribArray = void (*)(GLuint);
using PFN_glDisableVertexAttribArray = void (*)(GLuint);
using PFN_glCreateShader = GLuint (*)(GLenum);
using PFN_glShaderSource = void (*)(GLuint, GLsizei, const GLchar * const *, const GLint *);
using PFN_glCompileShader = void (*)(GLuint);
using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteShader = void (*)(GLuint);
using PFN_glCreateProgram = GLuint (*)(void);
using PFN_glAttachShader = void (*)(GLuint, GLuint);
using PFN_glLinkProgram = void (*)(GLuint);
using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteProgram = void (*)(GLuint);
using PFN_glUseProgram = void (*)(GLuint);
using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar *);
using PFN_glUniform1i = void (*)(GLint, GLint);
using PFN_glActiveTexture = void (*)(GLenum);

static PFN_glGenVertexArrays s_glGenVertexArrays{};
static PFN_glDeleteVertexArrays s_glDeleteVertexArrays{};
static PFN_glBindVertexArray s_glBindVertexArray{};
static PFN_glGenBuffers s_glGenBuffers{};
static PFN_glDeleteBuffers s_glDeleteBuffers{};
static PFN_glBindBuffer s_glBindBuffer{};
static PFN_glBufferData s_glBufferData{};
static PFN_glBufferSubData s_glBufferSubData{};
static PFN_glVertexAttribPointer s_glVertexAttribPointer{};
static PFN_glEnableVertexAttribArray s_glEnableVertexAttribArray{};
static PFN_glDisableVertexAttribArray s_glDisableVertexAttribArray{};
static PFN_glCreateShader s_glCreateShader{};
static PFN_glShaderSource s_glShaderSource{};
static PFN_glCompileShader s_glCompileShader{};
static PFN_glGetShaderiv s_glGetShaderiv{};
static PFN_glGetShaderInfoLog s_glGetShaderInfoLog{};
static PFN_glDeleteShader s_glDeleteShader{};
static PFN_glCreateProgram s_glCreateProgram{};
static PFN_glAttachShader s_glAttachShader{};
static PFN_glLinkProgram s_glLinkProgram{};
static PFN_glGetProgramiv s_glGetProgramiv{};
static PFN_glGetProgramInfoLog s_glGetProgramInfoLog{};
static PFN_glDeleteProgram s_glDeleteProgram{};
static PFN_glUseProgram s_glUseProgram{};
static PFN_glGetUniformLocation s_glGetUniformLocation{};
static PFN_glUniform1i s_glUniform1i{};
static PFN_glActiveTexture s_glActiveTexture{};

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
    s_glBufferSubData = reinterpret_cast<PFN_glBufferSubData>(LoadProc("glBufferSubData"));
    s_glVertexAttribPointer = reinterpret_cast<PFN_glVertexAttribPointer>(LoadProc("glVertexAttribPointer"));
    s_glEnableVertexAttribArray =
        reinterpret_cast<PFN_glEnableVertexAttribArray>(LoadProc("glEnableVertexAttribArray"));
    s_glDisableVertexAttribArray =
        reinterpret_cast<PFN_glDisableVertexAttribArray>(LoadProc("glDisableVertexAttribArray"));
    s_glCreateShader = reinterpret_cast<PFN_glCreateShader>(LoadProc("glCreateShader"));
    s_glShaderSource = reinterpret_cast<PFN_glShaderSource>(LoadProc("glShaderSource"));
    s_glCompileShader = reinterpret_cast<PFN_glCompileShader>(LoadProc("glCompileShader"));
    s_glGetShaderiv = reinterpret_cast<PFN_glGetShaderiv>(LoadProc("glGetShaderiv"));
    s_glGetShaderInfoLog = reinterpret_cast<PFN_glGetShaderInfoLog>(LoadProc("glGetShaderInfoLog"));
    s_glDeleteShader = reinterpret_cast<PFN_glDeleteShader>(LoadProc("glDeleteShader"));
    s_glCreateProgram = reinterpret_cast<PFN_glCreateProgram>(LoadProc("glCreateProgram"));
    s_glAttachShader = reinterpret_cast<PFN_glAttachShader>(LoadProc("glAttachShader"));
    s_glLinkProgram = reinterpret_cast<PFN_glLinkProgram>(LoadProc("glLinkProgram"));
    s_glGetProgramiv = reinterpret_cast<PFN_glGetProgramiv>(LoadProc("glGetProgramiv"));
    s_glGetProgramInfoLog = reinterpret_cast<PFN_glGetProgramInfoLog>(LoadProc("glGetProgramInfoLog"));
    s_glDeleteProgram = reinterpret_cast<PFN_glDeleteProgram>(LoadProc("glDeleteProgram"));
    s_glUseProgram = reinterpret_cast<PFN_glUseProgram>(LoadProc("glUseProgram"));
    s_glGetUniformLocation = reinterpret_cast<PFN_glGetUniformLocation>(LoadProc("glGetUniformLocation"));
    s_glUniform1i = reinterpret_cast<PFN_glUniform1i>(LoadProc("glUniform1i"));
    s_glActiveTexture = reinterpret_cast<PFN_glActiveTexture>(LoadProc("glActiveTexture"));
    return s_glGenVertexArrays && s_glDeleteVertexArrays && s_glBindVertexArray && s_glGenBuffers
        && s_glDeleteBuffers && s_glBindBuffer && s_glBufferData && s_glBufferSubData && s_glVertexAttribPointer
        && s_glEnableVertexAttribArray && s_glDisableVertexAttribArray && s_glCreateShader && s_glShaderSource
        && s_glCompileShader && s_glGetShaderiv && s_glGetShaderInfoLog && s_glDeleteShader && s_glCreateProgram
        && s_glAttachShader && s_glLinkProgram && s_glGetProgramiv && s_glGetProgramInfoLog && s_glDeleteProgram
        && s_glUseProgram && s_glGetUniformLocation && s_glUniform1i && s_glActiveTexture;
}
#endif /* !__APPLE__ */

static const char kVs[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char kFs[] = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  o_color = texture(u_tex, vec2(v_uv.x, 1.0 - v_uv.y));
}
)";

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

struct Sprite::Impl {
    unsigned int texture{0};
    unsigned int vao{0};
    unsigned int vbo{0};
    unsigned int program{0};
    int texW{0};
    int texH{0};
    float x{0.f};
    float y{0.f};
    float drawW{0.f};
    float drawH{0.f};
    float rotation_degrees{0.f};
    float scale_x{1.f};
    float scale_y{1.f};
    bool use_rects{false};
    Recti dst_rect{};
    Recti src_rect{};

    bool use_tex_frame{false};
    Recti tex_frame{};

    bool use_custom_image_pivot{false};
    float image_pivot_x{0.f};
    float image_pivot_y{0.f};

    std::vector<sprite_detail::ShapeCmdVariant> shapeQueue{};
    bool shapeRecording{false};
    unsigned int lineProgram{0};
    unsigned int lineVao{0};
    unsigned int lineVbo{0};
};

Sprite::Sprite() noexcept
    : impl_(std::make_unique<Impl>())
{
}
Sprite::~Sprite()
{
    Destroy();
}
Sprite::Sprite(Sprite&& other) noexcept
    : impl_(std::move(other.impl_))
{
}
Sprite& Sprite::operator=(Sprite&& other) noexcept
{
    if (this != &other) {
        Destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}
void Sprite::Destroy() noexcept
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
    if (impl_->vbo)
        glDeleteBuffers(1, &impl_->vbo);
    if (impl_->vao)
        glDeleteVertexArrays(1, &impl_->vao);
    if (impl_->texture)
        glDeleteTextures(1, &impl_->texture);
    if (impl_->program)
        glDeleteProgram(impl_->program);
#else
    if (s_glGenVertexArrays) {
        if (impl_->lineVbo)
            s_glDeleteBuffers(1, &impl_->lineVbo);
        if (impl_->lineVao)
            s_glDeleteVertexArrays(1, &impl_->lineVao);
        if (impl_->lineProgram)
            s_glDeleteProgram(impl_->lineProgram);
        if (impl_->vbo)
            s_glDeleteBuffers(1, &impl_->vbo);
        if (impl_->vao)
            s_glDeleteVertexArrays(1, &impl_->vao);
        if (impl_->texture)
            glDeleteTextures(1, &impl_->texture);
        if (impl_->program)
            s_glDeleteProgram(impl_->program);
    }
#endif
    impl_->texture = 0;
    impl_->vao = 0;
    impl_->vbo = 0;
    impl_->program = 0;
    impl_->lineVao = 0;
    impl_->lineVbo = 0;
    impl_->lineProgram = 0;
    impl_->texW = 0;
    impl_->texH = 0;
    impl_->use_tex_frame = false;
    impl_->use_custom_image_pivot = false;
    impl_->shapeQueue.clear();
    impl_->shapeRecording = false;
}
bool Sprite::CreateFromRGBA(int width, int height, const std::uint8_t * rgba, int strideBytes) noexcept
{
    if (!rgba || width <= 0 || height <= 0)
        return false;
    if (!impl_)
        impl_ = std::make_unique<Impl>();
#if !defined(__APPLE__)
    if (!LoadGL3Procs())
        return false;
#endif
    Destroy();

    const int rowStride = strideBytes > 0 ? strideBytes : width * 4;

    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVs);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFs);
    if (!vs || !fs)
        return false;
    const GLuint prog = linkProgram(vs, fs);
#if defined(__APPLE__)
    glDeleteShader(vs);
    glDeleteShader(fs);
#else
    s_glDeleteShader(vs);
    s_glDeleteShader(fs);
#endif
    if (!prog)
        return false;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<std::uint8_t> upload;
    if (rowStride == width * 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        upload.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (int row = 0; row < height; ++row)
            std::memcpy(
                upload.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4u,
                rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(rowStride),
                static_cast<std::size_t>(width) * 4u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint vao = 0;
    GLuint vbo = 0;
#if defined(__APPLE__)
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(8));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
#else
    s_glGenVertexArrays(1, &vao);
    s_glGenBuffers(1, &vbo);
    s_glBindVertexArray(vao);
    s_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    s_glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    s_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(0));
    s_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(8));
    s_glEnableVertexAttribArray(0);
    s_glEnableVertexAttribArray(1);
    s_glBindVertexArray(0);
    s_glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif

    impl_->texture = tex;
    impl_->vao = vao;
    impl_->vbo = vbo;
    impl_->program = prog;
    impl_->texW = width;
    impl_->texH = height;
    impl_->drawW = static_cast<float>(width);
    impl_->drawH = static_cast<float>(height);
    impl_->rotation_degrees = 0.f;
    impl_->scale_x = 1.f;
    impl_->scale_y = 1.f;
    impl_->use_rects = false;
    impl_->use_tex_frame = false;
    impl_->use_custom_image_pivot = false;
    return true;
}
bool Sprite::IsValid() const noexcept
{
    return impl_ && impl_->program != 0 && impl_->texture != 0;
}
void Sprite::SetPosition(float x, float y) noexcept
{
    if (impl_) {
        impl_->use_rects = false;
        impl_->x = x;
        impl_->y = y;
    }
}
void Sprite::SetSize(float width, float height) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    if (width > 0.f)
        impl_->drawW = width;
    if (height > 0.f)
        impl_->drawH = height;
}
void Sprite::SetRotationDegrees(float degrees) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    impl_->rotation_degrees = degrees;
}
void Sprite::SetScale(float sx, float sy) noexcept
{
    if (!impl_)
        return;
    impl_->use_rects = false;
    impl_->scale_x = sx;
    impl_->scale_y = sy;
}
void Sprite::SetScale(float uniform) noexcept
{
    SetScale(uniform, uniform);
}
void Sprite::Draw(ApplicationContextSettings& ctx) noexcept
{
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!IsValid())
        return;

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    const int tw = impl_->texW;
    const int th = impl_->texH;
    float px;
    float py;
    float sw;
    float sh;
    int sx;
    int sy;
    int sw_tex;
    int sh_tex;
    if (impl_->use_rects) {
        px = static_cast<float>(impl_->dst_rect.x);
        py = static_cast<float>(impl_->dst_rect.y);
        sw = static_cast<float>(impl_->dst_rect.w);
        sh = static_cast<float>(impl_->dst_rect.z);
        sx = impl_->src_rect.x;
        sy = impl_->src_rect.y;
        sw_tex = impl_->src_rect.w;
        sh_tex = impl_->src_rect.z;
    } else if (impl_->use_tex_frame) {
        sx = impl_->tex_frame.x;
        sy = impl_->tex_frame.y;
        sw_tex = impl_->tex_frame.w;
        sh_tex = impl_->tex_frame.z;
        sw = impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(sw_tex);
        sh = impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(sh_tex);
        px = impl_->x;
        py = impl_->y;
    } else {
        sw = impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(tw);
        sh = impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(th);
        px = impl_->x;
        py = impl_->y;
        sx = 0;
        sy = 0;
        sw_tex = tw;
        sh_tex = th;
    }

    if (sw <= 0.f || sh <= 0.f)
        return;

    if (sx < 0) {
        sw_tex += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh_tex += sy;
        sy = 0;
    }
    if (sx >= tw || sy >= th)
        return;
    sw_tex = std::min(sw_tex, tw - sx);
    sh_tex = std::min(sh_tex, th - sy);
    if (sw_tex <= 0 || sh_tex <= 0)
        return;

    float v[24];
    sprite_detail::fillSpriteVertices(
        px,
        py,
        sw,
        sh,
        fbw,
        fbh,
        tw,
        th,
        sx,
        sy,
        sw_tex,
        sh_tex,
        impl_->rotation_degrees,
        impl_->scale_x,
        impl_->scale_y,
        impl_->use_custom_image_pivot,
        impl_->image_pivot_x,
        impl_->image_pivot_y,
        v);

#if defined(__APPLE__)
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->texture);
    const GLint loc = glGetUniformLocation(impl_->program, "u_tex");
    if (loc >= 0)
        glUniform1i(loc, 0);
    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
#else
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_glUseProgram(impl_->program);
    s_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->texture);
    const GLint loc = s_glGetUniformLocation(impl_->program, "u_tex");
    if (loc >= 0)
        s_glUniform1i(loc, 0);
    s_glBindVertexArray(impl_->vao);
    s_glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    s_glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    s_glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    s_glUseProgram(0);
#endif
}

void Sprite::BeginShape() noexcept
{
    if (!impl_)
        return;
    impl_->shapeQueue.clear();
    impl_->shapeRecording = true;
}

void Sprite::TriangleFill(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float r,
    float g,
    float b,
    float a) noexcept
{
    if (!impl_ || !impl_->shapeRecording)
        return;
    sprite_detail::FillTriangleCmd t{};
    t.x0 = x0;
    t.y0 = y0;
    t.x1 = x1;
    t.y1 = y1;
    t.x2 = x2;
    t.y2 = y2;
    t.r = r;
    t.g = g;
    t.b = b;
    t.a = a;
    impl_->shapeQueue.push_back(t);
}

void Sprite::Circle(
    float centerX,
    float centerY,
    float radius,
    float fillR,
    float fillG,
    float fillB,
    float fillA,
    float borderR,
    float borderG,
    float borderB,
    float borderA,
    float borderThickness) noexcept
{
    if (!impl_ || !impl_->shapeRecording)
        return;
    sprite_detail::CircleCmd c{};
    c.cx = centerX;
    c.cy = centerY;
    c.radius = radius;
    c.fr = fillR;
    c.fg = fillG;
    c.fb = fillB;
    c.fa = fillA;
    c.br = borderR;
    c.bg = borderG;
    c.bb = borderB;
    c.ba = borderA;
    c.borderThick = borderThickness;
    impl_->shapeQueue.push_back(c);
}

void Sprite::EndShape(ApplicationContextSettings & ctx) noexcept
{
    if (!impl_)
        return;
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return;
    if (!impl_->shapeRecording)
        return;
    impl_->shapeRecording = false;
    if (impl_->shapeQueue.empty())
        return;
    if (!IsValid())
        return;

    int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    if (fbw <= 0 || fbh <= 0)
        return;

    std::vector<float> interleaved;
    sprite_detail::appendShapeQueueToInterleaved(impl_->shapeQueue, fbw, fbh, interleaved);
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

int Sprite::TextureWidth() const noexcept
{
    return impl_ ? impl_->texW : 0;
}
int Sprite::TextureHeight() const noexcept
{
    return impl_ ? impl_->texH : 0;
}
float Sprite::PositionX() const noexcept
{
    return impl_ ? impl_->x : 0.f;
}
float Sprite::PositionY() const noexcept
{
    return impl_ ? impl_->y : 0.f;
}
float Sprite::DrawWidth() const noexcept
{
    if (!impl_)
        return 0.f;
    return impl_->drawW > 0.f ? impl_->drawW : static_cast<float>(impl_->texW);
}
float Sprite::DrawHeight() const noexcept
{
    if (!impl_)
        return 0.f;
    return impl_->drawH > 0.f ? impl_->drawH : static_cast<float>(impl_->texH);
}
float Sprite::RotationDegrees() const noexcept
{
    return impl_ ? impl_->rotation_degrees : 0.f;
}
float Sprite::ScaleX() const noexcept
{
    return impl_ ? impl_->scale_x : 1.f;
}
float Sprite::ScaleY() const noexcept
{
    return impl_ ? impl_->scale_y : 1.f;
}

} // namespace HonkordGL::Graphics

#endif /* __ANDROID__ */

namespace HonkordGL::Graphics {

bool Sprite::IntersectsEntityBounds(Sprite const & other) const noexcept
{
    if (!IsValid() || !other.IsValid())
        return false;
    float apx = 0.f;
    float apy = 0.f;
    float asw = 0.f;
    float ash = 0.f;
    float bpx = 0.f;
    float bpy = 0.f;
    float bsw = 0.f;
    float bsh = 0.f;
    if (!sprite_detail::resolveSpriteDrawQuad(
            impl_->texW,
            impl_->texH,
            impl_->x,
            impl_->y,
            impl_->drawW,
            impl_->drawH,
            impl_->use_rects,
            impl_->dst_rect,
            impl_->src_rect,
            impl_->use_tex_frame,
            impl_->tex_frame,
            apx,
            apy,
            asw,
            ash))
        return false;
    if (!sprite_detail::resolveSpriteDrawQuad(
            other.impl_->texW,
            other.impl_->texH,
            other.impl_->x,
            other.impl_->y,
            other.impl_->drawW,
            other.impl_->drawH,
            other.impl_->use_rects,
            other.impl_->dst_rect,
            other.impl_->src_rect,
            other.impl_->use_tex_frame,
            other.impl_->tex_frame,
            bpx,
            bpy,
            bsw,
            bsh))
        return false;
    return sprite_detail::spriteQuadsOverlapResolved(
        apx,
        apy,
        asw,
        ash,
        impl_->rotation_degrees,
        impl_->scale_x,
        impl_->scale_y,
        impl_->use_custom_image_pivot,
        impl_->image_pivot_x,
        impl_->image_pivot_y,
        bpx,
        bpy,
        bsw,
        bsh,
        other.impl_->rotation_degrees,
        other.impl_->scale_x,
        other.impl_->scale_y,
        other.impl_->use_custom_image_pivot,
        other.impl_->image_pivot_x,
        other.impl_->image_pivot_y);
}

bool Sprite::ContainsClientPoint(float x, float y) const noexcept
{
    if (!IsValid())
        return false;
    float px = 0.f;
    float py = 0.f;
    float sw = 0.f;
    float sh = 0.f;
    if (!sprite_detail::resolveSpriteDrawQuad(
            impl_->texW,
            impl_->texH,
            impl_->x,
            impl_->y,
            impl_->drawW,
            impl_->drawH,
            impl_->use_rects,
            impl_->dst_rect,
            impl_->src_rect,
            impl_->use_tex_frame,
            impl_->tex_frame,
            px,
            py,
            sw,
            sh))
        return false;
    float qx[4];
    float qy[4];
    sprite_detail::spriteQuadCornersClient(
        px,
        py,
        sw,
        sh,
        impl_->rotation_degrees,
        impl_->scale_x,
        impl_->scale_y,
        impl_->use_custom_image_pivot,
        impl_->image_pivot_x,
        impl_->image_pivot_y,
        qx,
        qy);
    return sprite_detail::pointInConvexQuad(x, y, qx, qy);
}

bool Sprite::CreateFromRGBA(
    ApplicationContextSettings& ctx,
    int width,
    int height,
    const std::uint8_t * rgba,
    int strideBytes) noexcept
{
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return false;
    return CreateFromRGBA(width, height, rgba, strideBytes);
}

void Sprite::ApplyRects(Recti const& dst, Recti const& src) noexcept
{
    if (!impl_)
        return;
    if (dst.w <= 0 || dst.z <= 0 || src.w <= 0 || src.z <= 0)
        return;
    impl_->use_rects = true;
    impl_->use_tex_frame = false;
    impl_->dst_rect = dst;
    impl_->src_rect = src;
}

void Sprite::SetImageFrame(Recti const& frame) noexcept
{
    if (!impl_ || impl_->texW <= 0 || impl_->texH <= 0)
        return;
    int fx = std::max(0, frame.x);
    int fy = std::max(0, frame.y);
    int fw = frame.w;
    int fh = frame.z;
    if (fw <= 0 || fh <= 0)
        return;
    fw = std::min(fw, impl_->texW - fx);
    fh = std::min(fh, impl_->texH - fy);
    if (fw <= 0 || fh <= 0)
        return;
    impl_->tex_frame.x = fx;
    impl_->tex_frame.y = fy;
    impl_->tex_frame.w = fw;
    impl_->tex_frame.z = fh;
    impl_->use_tex_frame = true;
    impl_->use_rects = false;
}

void Sprite::ClearImageFrame() noexcept
{
    if (!impl_)
        return;
    impl_->use_tex_frame = false;
}

bool Sprite::HasImageFrame() const noexcept
{
    return impl_ && impl_->use_tex_frame;
}

Recti Sprite::ImageFrame() const noexcept
{
    if (!impl_)
        return Recti{};
    if (impl_->use_tex_frame)
        return impl_->tex_frame;
    return Recti{0, 0, impl_->texW, impl_->texH};
}

void Sprite::SetImagePivot(float localXPx, float localYPx) noexcept
{
    if (!impl_)
        return;
    impl_->use_custom_image_pivot = true;
    impl_->image_pivot_x = localXPx;
    impl_->image_pivot_y = localYPx;
}

void Sprite::ResetImagePivot() noexcept
{
    if (!impl_)
        return;
    impl_->use_custom_image_pivot = false;
}

bool Sprite::HasCustomImagePivot() const noexcept
{
    return impl_ && impl_->use_custom_image_pivot;
}

float Sprite::ImagePivotX() const noexcept
{
    return impl_ ? impl_->image_pivot_x : 0.f;
}

float Sprite::ImagePivotY() const noexcept
{
    return impl_ ? impl_->image_pivot_y : 0.f;
}

void Sprite::ApplyImageEntityFrom(Lines const& lines) noexcept
{
    SetPosition(lines.EntityPositionX(), lines.EntityPositionY());
    SetRotationDegrees(lines.EntityRotationDegrees());
    SetScale(lines.EntityScaleX(), lines.EntityScaleY());
    SetImagePivot(lines.EntityPivotX(), lines.EntityPivotY());
}

void Sprite::ApplyImageEntityFrom(Eclipse const& eclipse) noexcept
{
    SetPosition(eclipse.EntityPositionX(), eclipse.EntityPositionY());
    SetRotationDegrees(eclipse.EntityRotationDegrees());
    SetScale(eclipse.EntityScaleX(), eclipse.EntityScaleY());
    SetImagePivot(eclipse.EntityPivotX(), eclipse.EntityPivotY());
}

bool Sprite::CreateFromPixelBuffer(int width, int height, unsigned char const * pixels, int strideBytes) noexcept
{
    return CreateFromRGBA(width, height, reinterpret_cast<std::uint8_t const *>(pixels), strideBytes);
}
bool Sprite::CreateFromPixelBuffer(
    ApplicationContextSettings& ctx,
    int width,
    int height,
    unsigned char const * pixels,
    int strideBytes) noexcept
{
    return CreateFromRGBA(ctx, width, height, reinterpret_cast<std::uint8_t const *>(pixels), strideBytes);
}
bool Sprite::CreateFromImageFile(char const * path) noexcept
{
    if (!path)
        return false;
    int w = 0;
    int h = 0;
    stbi_uc * data = stbi_load(path, &w, &h, nullptr, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data)
            stbi_image_free(data);
        return false;
    }
    const bool ok = CreateFromRGBA(w, h, reinterpret_cast<std::uint8_t const *>(data), 0);
    stbi_image_free(data);
    return ok;
}
bool Sprite::CreateFromImageFile(ApplicationContextSettings& ctx, char const * path) noexcept
{
    if (static_cast<NativePlatform>(ctx.native_platform) == NativePlatform::DRM)
        return false;
    return CreateFromImageFile(path);
}

} // namespace HonkordGL::Graphics