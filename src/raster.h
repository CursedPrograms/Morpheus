// Morpheus - Raster image, 2D projective geometry, and pixel blend modes.
// Ported 1:1 from EffectCraft:
//   crates/geom/src/lib.rs (Vec2, Rect, Mat3)
//   crates/color/src/blend.rs (BlendMode, blend_pixel, luminance)
//   crates/raster/src/lib.rs (Image, Px, quantize, dither, hash_noise)
//   crates/raster/src/warp.rs (composite_warp, composite_warp_reference, composite_warp_changed, accumulate_warp)
// after EffectCraft (crates/geom/src/lib.rs, crates/color/src/blend.rs, crates/raster/src/lib.rs, crates/raster/src/warp.rs)
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cmath>
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <optional>
#include <tuple>
#include <algorithm>

namespace Raster {

// =====================================================================
// Geometry (after EffectCraft crates/geom/src/lib.rs)
// =====================================================================

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2() = default;
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    static const Vec2 ZERO;
    static const Vec2 ONE;

    double dot(const Vec2& o) const { return x * o.x + y * o.y; }
    double cross(const Vec2& o) const { return x * o.y - y * o.x; }
    double length() const { return std::sqrt(dot(*this)); }
    Vec2 normalize() const {
        double l = length();
        return l > 0.0 ? (*this) / l : *this;
    }
    Vec2 lerp(const Vec2& o, double t) const {
        return *this + (o - *this) * t;
    }
    Vec2 rotate_deg(double deg) const {
        double rad = deg * (3.14159265358979323846 / 180.0);
        double s = std::sin(rad);
        double c = std::cos(rad);
        return {x * c - y * s, x * s + y * c};
    }
    Vec2 mul_elem(const Vec2& o) const { return {x * o.x, y * o.y}; }

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double k) const { return {x * k, y * k}; }
    Vec2 operator/(double k) const { return {x / k, y / k}; }
    Vec2 operator-() const { return {-x, -y}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(double k) { x *= k; y *= k; return *this; }
    Vec2& operator/=(double k) { x /= k; y /= k; return *this; }

    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }
};

inline const Vec2 Vec2::ZERO{0.0, 0.0};
inline const Vec2 Vec2::ONE{1.0, 1.0};

inline Vec2 vec2(double x, double y) { return {x, y}; }

// Axis-aligned rectangle (min inclusive, max exclusive for pixel rects).
struct Rect {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;

    constexpr Rect() = default;
    constexpr Rect(double x0_, double y0_, double x1_, double y1_) : x0(x0_), y0(y0_), x1(x1_), y1(y1_) {}

    static const Rect EMPTY;

    static Rect from_size(double w, double h) { return {0.0, 0.0, w, h}; }

    double width() const { return (x1 - x0) > 0.0 ? (x1 - x0) : 0.0; }
    double height() const { return (y1 - y0) > 0.0 ? (y1 - y0) : 0.0; }
    bool is_empty() const { return !(x1 > x0 && y1 > y0); }
    Vec2 center() const { return {(x0 + x1) * 0.5, (y0 + y1) * 0.5}; }

    Rect union_with(const Rect& o) const {
        return {std::min(x0, o.x0), std::min(y0, o.y0), std::max(x1, o.x1), std::max(y1, o.y1)};
    }
    Rect intersect(const Rect& o) const {
        return {std::max(x0, o.x0), std::max(y0, o.y0), std::min(x1, o.x1), std::min(y1, o.y1)};
    }
    void include(const Vec2& p) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    Rect inflate(double d) const {
        return {x0 - d, y0 - d, x1 + d, y1 + d};
    }
    bool contains(const Vec2& p) const {
        return p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1;
    }
    std::array<Vec2, 4> corners() const {
        return {vec2(x0, y0), vec2(x1, y0), vec2(x1, y1), vec2(x0, y1)};
    }
    Rect round_out() const {
        return {std::floor(x0), std::floor(y0), std::ceil(x1), std::ceil(y1)};
    }
};

inline const Rect Rect::EMPTY{
    std::numeric_limits<double>::infinity(),
    std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity()
};

// Row-major 3x3 matrix acting on column vectors (x, y, 1); a 2D projective transform.
struct Mat3 {
    double m[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };

    constexpr Mat3() = default;
    constexpr Mat3(const double arr[3][3]) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m[i][j] = arr[i][j];
    }
    constexpr Mat3(double m00, double m01, double m02,
                   double m10, double m11, double m12,
                   double m20, double m21, double m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    static const Mat3 IDENTITY;

    static Mat3 translate(const Vec2& t) {
        return {1.0, 0.0, t.x,
                0.0, 1.0, t.y,
                0.0, 0.0, 1.0};
    }
    static Mat3 scale(const Vec2& s) {
        return {s.x, 0.0, 0.0,
                0.0, s.y, 0.0,
                0.0, 0.0, 1.0};
    }
    static Mat3 rotate_deg(double deg) {
        double rad = deg * (3.14159265358979323846 / 180.0);
        double s = std::sin(rad);
        double c = std::cos(rad);
        return {c, -s, 0.0,
                s,  c, 0.0,
                0.0, 0.0, 1.0};
    }
    static Mat3 skew_deg(double skew, double axis_deg) {
        double k = std::tan(skew * (3.14159265358979323846 / 180.0));
        Mat3 shear{1.0, k, 0.0,
                   0.0, 1.0, 0.0,
                   0.0, 0.0, 1.0};
        return rotate_deg(axis_deg) * shear * rotate_deg(-axis_deg);
    }
    static Mat3 layer_2d(const Vec2& anchor, const Vec2& position, const Vec2& scale_pct, double rotation_deg) {
        return translate(position) * rotate_deg(rotation_deg) * scale(scale_pct / 100.0) * translate(-anchor);
    }

    Vec2 apply(const Vec2& p) const {
        double x = m[0][0] * p.x + m[0][1] * p.y + m[0][2];
        double y = m[1][0] * p.x + m[1][1] * p.y + m[1][2];
        double w = m[2][0] * p.x + m[2][1] * p.y + m[2][2];
        if (w != 1.0 && w != 0.0) return {x / w, y / w};
        return {x, y};
    }
    Vec2 apply_vec(const Vec2& v) const {
        return {m[0][0] * v.x + m[0][1] * v.y,
                m[1][0] * v.x + m[1][1] * v.y};
    }
    bool is_affine() const {
        return m[2][0] == 0.0 && m[2][1] == 0.0 && std::abs(m[2][2] - 1.0) < 1e-12;
    }
    double determinant() const {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
    std::optional<Mat3> inverse() const {
        double d = determinant();
        if (std::abs(d) < 1e-300 || !std::isfinite(d)) return std::nullopt;
        return Mat3(
            (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / d, (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / d, (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / d,
            (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / d, (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / d, (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / d,
            (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / d, (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / d, (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / d
        );
    }
    Rect map_rect(const Rect& r) const {
        Rect out = Rect::EMPTY;
        for (const auto& c : r.corners()) out.include(apply(c));
        return out;
    }
    double mean_scale() const {
        return std::sqrt(std::abs(determinant()));
    }
    static Mat3 square_to_quad(const std::array<Vec2, 4>& q);

    Mat3 operator*(const Mat3& o) const {
        Mat3 r;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                double sum = 0.0;
                for (int k = 0; k < 3; ++k) sum += m[i][k] * o.m[k][j];
                r.m[i][j] = sum;
            }
        }
        return r;
    }
};

inline const Mat3 Mat3::IDENTITY{
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
};

// =====================================================================
// Color & Blend Modes (after EffectCraft crates/color/src/blend.rs)
// =====================================================================

enum class BlendMode {
    Normal,
    Dissolve,
    DancingDissolve,
    Darken,
    Multiply,
    ColorBurn,
    ClassicColorBurn,
    LinearBurn,
    DarkerColor,
    Add,
    Lighten,
    Screen,
    ColorDodge,
    ClassicColorDodge,
    LinearDodge,
    LighterColor,
    Overlay,
    SoftLight,
    HardLight,
    LinearLight,
    VividLight,
    PinLight,
    HardMix,
    Difference,
    ClassicDifference,
    Exclusion,
    Subtract,
    Divide,
    Hue,
    Saturation,
    Color,
    Luminosity,
    StencilAlpha,
    StencilLuma,
    SilhouetteAlpha,
    SilhouetteLuma,
    AlphaAdd,
    LuminescentPremul
};

const char* blend_mode_label(BlendMode mode);
std::optional<BlendMode> blend_mode_from_name(const std::string& name);
bool blend_mode_ends_group(BlendMode mode);
bool blend_mode_is_stencil(BlendMode mode);
bool blend_mode_supports_hdr(BlendMode mode);
std::optional<float> blend_mode_classic_neutral(BlendMode mode);

float luminance(float r, float g, float b);
std::array<float, 3> set_lum(std::array<float, 3> c, float l);

using Px = std::array<float, 4>; // Premultiplied RGBA [0.0..1.0] (or HDR)

Px blend_pixel(BlendMode mode, Px dst, Px src, float noise = 0.5f);

// =====================================================================
// Noise, Quantization, Dither (after EffectCraft crates/raster/src/lib.rs)
// =====================================================================

constexpr uint32_t MAX_PADDED_SIDE = 30000;

float hash_noise(uint32_t x, uint32_t y, uint32_t seed);
uint8_t dither_u8(float v, uint32_t x, uint32_t y);

void quantize_px(Px& p, float levels, float inv);
Px quantize_levels(Px p, float levels, float inv);
std::array<uint8_t, 4> quantize_px_u8(Px p);
Px expand_u8(const std::array<uint8_t, 4>& n);

// =====================================================================
// Sampling & Warp Options
// =====================================================================

enum class Sampling {
    Nearest,
    Bilinear,
    Bicubic
};

struct WarpOpts {
    Sampling sampling = Sampling::Bilinear;
    float opacity = 1.0f;
    BlendMode mode = BlendMode::Normal;
    uint32_t seed = 0;
    std::optional<Rect> clip = std::nullopt;
};

// =====================================================================
// Image (after EffectCraft crates/raster/src/lib.rs)
// =====================================================================

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Px> data;

    Image() = default;
    Image(uint32_t w, uint32_t h);
    static Image filled(uint32_t w, uint32_t h, Px px);
    static Image from_rgba8(uint32_t width, uint32_t height, const uint8_t* rgba, size_t len);
    static Image from_rgba8(uint32_t width, uint32_t height, const std::vector<uint8_t>& rgba) {
        return from_rgba8(width, height, rgba.data(), rgba.size());
    }

    bool is_empty() const { return width == 0 || height == 0; }
    size_t idx(uint32_t x, uint32_t y) const { return (size_t)y * width + x; }

    Px get(int64_t x, int64_t y) const {
        if (x < 0 || y < 0 || x >= (int64_t)width || y >= (int64_t)height)
            return {0.0f, 0.0f, 0.0f, 0.0f};
        return data[(size_t)y * width + (size_t)x];
    }
    Px get_clamped(int64_t x, int64_t y) const {
        if (is_empty()) return {0.0f, 0.0f, 0.0f, 0.0f};
        int64_t cx = std::clamp(x, (int64_t)0, (int64_t)width - 1);
        int64_t cy = std::clamp(y, (int64_t)0, (int64_t)height - 1);
        return data[(size_t)cy * width + (size_t)cx];
    }
    void set(uint32_t x, uint32_t y, Px px) {
        if (x < width && y < height) data[(size_t)y * width + x] = px;
    }

    Px sample_nearest(double x, double y) const;
    Px sample_bilinear(double x, double y) const;
    Px sample_bilinear_clamped(double x, double y) const;
    Px sample_bicubic(double x, double y) const;

    std::optional<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> alpha_bounds() const;
    Image padded(uint32_t pad) const;

    std::vector<uint8_t> to_rgba8() const;
    std::vector<uint8_t> to_rgba8_dithered() const;
    std::vector<uint8_t> to_rgba8_over(const std::array<float, 3>& bg) const;
    std::vector<uint8_t> to_rgba8_over_dithered(const std::array<float, 3>& bg) const;

    void blend_from(const Image& src, BlendMode mode, float opacity, uint32_t seed);
};

// =====================================================================
// Composite Warping (after EffectCraft crates/raster/src/warp.rs)
// =====================================================================

struct ClearTiles {
    size_t cols = 0;
    std::vector<bool> clear;

    ClearTiles(const Image& img);
    bool is_clear(size_t x, size_t y) const;
};

void composite_warp(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts = {});
void composite_warp_reference(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts = {});
std::optional<std::array<uint32_t, 4>> composite_warp_changed(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts = {}, std::optional<float> quantize = std::nullopt);
void accumulate_warp(Image& dst, const Image& src, const Mat3& m, Sampling sampling, float weight);
Image resample(const Image& src, uint32_t w, uint32_t h);

} // namespace Raster
