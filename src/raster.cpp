// Morpheus - Raster image, 2D projective geometry, and pixel blend modes.
// Ported 1:1 from EffectCraft:
//   crates/geom/src/lib.rs
//   crates/color/src/blend.rs
//   crates/raster/src/lib.rs
//   crates/raster/src/warp.rs
// after EffectCraft (crates/geom/src/lib.rs, crates/color/src/blend.rs, crates/raster/src/lib.rs, crates/raster/src/warp.rs)
#include "raster.h"

#include <cmath>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace Raster {

// =====================================================================
// Geometry (after EffectCraft crates/geom/src/lib.rs)
// =====================================================================

// after EffectCraft crates/geom/src/lib.rs square_to_quad
Mat3 Mat3::square_to_quad(const std::array<Vec2, 4>& q) {
    double x0 = q[0].x, y0 = q[0].y;
    double x1 = q[1].x, y1 = q[1].y;
    double x2 = q[2].x, y2 = q[2].y;
    double x3 = q[3].x, y3 = q[3].y;
    double dx1 = x1 - x2;
    double dx2 = x3 - x2;
    double dy1 = y1 - y2;
    double dy2 = y3 - y2;
    double sx = x0 - x1 + x2 - x3;
    double sy = y0 - y1 + y2 - y3;
    if (std::abs(sx) < 1e-12 && std::abs(sy) < 1e-12) {
        return Mat3(x1 - x0, x3 - x0, x0,
                    y1 - y0, y3 - y0, y0,
                    0.0,     0.0,     1.0);
    }
    double den = dx1 * dy2 - dx2 * dy1;
    if (den == 0.0 || !std::isfinite(den)) {
        return Mat3::IDENTITY;
    }
    double g = (sx * dy2 - dx2 * sy) / den;
    double h = (dx1 * sy - sx * dy1) / den;
    return Mat3(x1 - x0 + g * x1, x3 - x0 + h * x3, x0,
                y1 - y0 + g * y1, y3 - y0 + h * y3, y0,
                g,                h,                1.0);
}

// =====================================================================
// Color & Blend Modes (after EffectCraft crates/color/src/blend.rs)
// =====================================================================

static const BlendMode ALL_BLEND_MODES[] = {
    BlendMode::Normal, BlendMode::Dissolve, BlendMode::DancingDissolve,
    BlendMode::Darken, BlendMode::Multiply, BlendMode::ColorBurn, BlendMode::ClassicColorBurn, BlendMode::LinearBurn, BlendMode::DarkerColor,
    BlendMode::Add, BlendMode::Lighten, BlendMode::Screen, BlendMode::ColorDodge, BlendMode::ClassicColorDodge, BlendMode::LinearDodge, BlendMode::LighterColor,
    BlendMode::Overlay, BlendMode::SoftLight, BlendMode::HardLight, BlendMode::LinearLight, BlendMode::VividLight, BlendMode::PinLight, BlendMode::HardMix,
    BlendMode::Difference, BlendMode::ClassicDifference, BlendMode::Exclusion, BlendMode::Subtract, BlendMode::Divide,
    BlendMode::Hue, BlendMode::Saturation, BlendMode::Color, BlendMode::Luminosity,
    BlendMode::StencilAlpha, BlendMode::StencilLuma, BlendMode::SilhouetteAlpha, BlendMode::SilhouetteLuma,
    BlendMode::AlphaAdd, BlendMode::LuminescentPremul
};

const char* blend_mode_label(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return "Normal";
        case BlendMode::Dissolve: return "Dissolve";
        case BlendMode::DancingDissolve: return "Dancing Dissolve";
        case BlendMode::Darken: return "Darken";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::ColorBurn: return "Color Burn";
        case BlendMode::ClassicColorBurn: return "Classic Color Burn";
        case BlendMode::LinearBurn: return "Linear Burn";
        case BlendMode::DarkerColor: return "Darker Color";
        case BlendMode::Add: return "Add";
        case BlendMode::Lighten: return "Lighten";
        case BlendMode::Screen: return "Screen";
        case BlendMode::ColorDodge: return "Color Dodge";
        case BlendMode::ClassicColorDodge: return "Classic Color Dodge";
        case BlendMode::LinearDodge: return "Linear Dodge";
        case BlendMode::LighterColor: return "Lighter Color";
        case BlendMode::Overlay: return "Overlay";
        case BlendMode::SoftLight: return "Soft Light";
        case BlendMode::HardLight: return "Hard Light";
        case BlendMode::LinearLight: return "Linear Light";
        case BlendMode::VividLight: return "Vivid Light";
        case BlendMode::PinLight: return "Pin Light";
        case BlendMode::HardMix: return "Hard Mix";
        case BlendMode::Difference: return "Difference";
        case BlendMode::ClassicDifference: return "Classic Difference";
        case BlendMode::Exclusion: return "Exclusion";
        case BlendMode::Subtract: return "Subtract";
        case BlendMode::Divide: return "Divide";
        case BlendMode::Hue: return "Hue";
        case BlendMode::Saturation: return "Saturation";
        case BlendMode::Color: return "Color";
        case BlendMode::Luminosity: return "Luminosity";
        case BlendMode::StencilAlpha: return "Stencil Alpha";
        case BlendMode::StencilLuma: return "Stencil Luma";
        case BlendMode::SilhouetteAlpha: return "Silhouette Alpha";
        case BlendMode::SilhouetteLuma: return "Silhouette Luma";
        case BlendMode::AlphaAdd: return "Alpha Add";
        case BlendMode::LuminescentPremul: return "Luminescent Premul";
    }
    return "Normal";
}

static std::string normalize_name(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum((unsigned char)c))
            out.push_back((char)std::tolower((unsigned char)c));
    }
    return out;
}

std::optional<BlendMode> blend_mode_from_name(const std::string& name) {
    std::string norm = normalize_name(name);
    for (BlendMode m : ALL_BLEND_MODES) {
        if (normalize_name(blend_mode_label(m)) == norm) return m;
    }
    return std::nullopt;
}

bool blend_mode_ends_group(BlendMode mode) {
    return mode == BlendMode::DancingDissolve
        || mode == BlendMode::DarkerColor
        || mode == BlendMode::LighterColor
        || mode == BlendMode::HardMix
        || mode == BlendMode::Divide
        || mode == BlendMode::Luminosity
        || mode == BlendMode::SilhouetteLuma;
}

bool blend_mode_is_stencil(BlendMode mode) {
    return mode == BlendMode::StencilAlpha
        || mode == BlendMode::StencilLuma
        || mode == BlendMode::SilhouetteAlpha
        || mode == BlendMode::SilhouetteLuma;
}

bool blend_mode_supports_hdr(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal:
        case BlendMode::Dissolve:
        case BlendMode::DancingDissolve:
        case BlendMode::Darken:
        case BlendMode::Multiply:
        case BlendMode::ColorBurn:
        case BlendMode::ClassicColorBurn:
        case BlendMode::LinearBurn:
        case BlendMode::DarkerColor:
        case BlendMode::Add:
        case BlendMode::Lighten:
        case BlendMode::Screen:
        case BlendMode::LinearDodge:
        case BlendMode::LighterColor:
        case BlendMode::Difference:
        case BlendMode::ClassicDifference:
        case BlendMode::Overlay:
        case BlendMode::Exclusion:
        case BlendMode::Luminosity:
        case BlendMode::Subtract:
        case BlendMode::Divide:
        case BlendMode::StencilAlpha:
        case BlendMode::StencilLuma:
        case BlendMode::SilhouetteAlpha:
        case BlendMode::SilhouetteLuma:
        case BlendMode::AlphaAdd:
        case BlendMode::LuminescentPremul:
            return true;
        default:
            return false;
    }
}

std::optional<float> blend_mode_classic_neutral(BlendMode mode) {
    switch (mode) {
        case BlendMode::ClassicColorDodge:
        case BlendMode::ClassicDifference:
            return 0.0f;
        case BlendMode::ClassicColorBurn:
            return 1.0f;
        default:
            return std::nullopt;
    }
}

// after EffectCraft crates/color/src/lib.rs luminance
float luminance(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// Helpers for blend modes
static inline float burn(float cb, float cs) {
    if (cb >= 1.0f) return 1.0f;
    if (cs <= 0.0f) return 0.0f;
    return 1.0f - std::min(1.0f, (1.0f - cb) / cs);
}

static inline float dodge(float cb, float cs) {
    if (cb <= 0.0f) return 0.0f;
    if (cs >= 1.0f) return 1.0f;
    return std::min(1.0f, cb / (1.0f - cs));
}

static inline float hard_light(float cb, float cs) {
    if (cs <= 0.5f) return cb * 2.0f * cs;
    float s = 2.0f * cs - 1.0f;
    return cb + s - cb * s;
}

static inline float soft_light(float cb, float cs) {
    if (cs <= 0.5f) {
        return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    } else {
        float d = (cb <= 0.25f) ? (((16.0f * cb - 12.0f) * cb + 4.0f) * cb) : std::sqrt(std::max(0.0f, cb));
        return cb + (2.0f * cs - 1.0f) * (d - cb);
    }
}

static inline float vivid(float cb, float cs) {
    if (cs <= 0.5f) return burn(cb, 2.0f * cs);
    return dodge(cb, 2.0f * (cs - 0.5f));
}

static inline float separable(BlendMode mode, float cb, float cs) {
    switch (mode) {
        case BlendMode::Darken: return std::min(cb, cs);
        case BlendMode::Multiply: return cb * cs;
        case BlendMode::ColorBurn: return burn(cb, cs);
        case BlendMode::ClassicColorBurn:
            return (cs <= 0.0f) ? 0.0f : (1.0f - (1.0f - cb) / cs);
        case BlendMode::LinearBurn: return std::max(0.0f, cb + cs - 1.0f);
        case BlendMode::Add:
        case BlendMode::LinearDodge: return cb + cs;
        case BlendMode::Lighten: return std::max(cb, cs);
        case BlendMode::Screen: return cb + cs - cb * cs;
        case BlendMode::ColorDodge: return dodge(cb, cs);
        case BlendMode::ClassicColorDodge:
            return (cs >= 1.0f) ? 1.0f : std::min(1.0f, cb / (1.0f - cs));
        case BlendMode::Overlay: return hard_light(cs, cb);
        case BlendMode::SoftLight: return soft_light(cb, cs);
        case BlendMode::HardLight: return hard_light(cb, cs);
        case BlendMode::LinearLight: return std::clamp(cb + 2.0f * cs - 1.0f, 0.0f, 1.0f);
        case BlendMode::VividLight: return vivid(cb, cs);
        case BlendMode::PinLight:
            return (cs <= 0.5f) ? std::min(cb, 2.0f * cs) : std::max(cb, 2.0f * cs - 1.0f);
        case BlendMode::HardMix:
            return (cb + cs >= 1.0f) ? 1.0f : 0.0f;
        case BlendMode::Difference:
        case BlendMode::ClassicDifference: return std::abs(cb - cs);
        case BlendMode::Exclusion: return cb + cs - 2.0f * cb * cs;
        case BlendMode::Subtract: return std::max(0.0f, cb - cs);
        case BlendMode::Divide:
            if (cs <= 0.0f) return (cb > 0.0f) ? 1.0f : 0.0f;
            return cb / cs;
        default: return cs;
    }
}

static inline float lum(const std::array<float, 3>& c) {
    return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2];
}

static inline std::array<float, 3> clip_color(std::array<float, 3> c) {
    float l = lum(c);
    float n = std::min({c[0], c[1], c[2]});
    float x = std::max({c[0], c[1], c[2]});
    std::array<float, 3> o = c;
    if (n < 0.0f) {
        for (float& v : o) v = l + (v - l) * l / std::max(1e-9f, l - n);
    }
    if (x > 1.0f) {
        for (float& v : o) v = l + (v - l) * (1.0f - l) / std::max(1e-9f, x - l);
    }
    return o;
}

std::array<float, 3> set_lum(std::array<float, 3> c, float l) {
    float d = l - lum(c);
    return clip_color({c[0] + d, c[1] + d, c[2] + d});
}

static inline float sat(const std::array<float, 3>& c) {
    return std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]});
}

static inline std::array<float, 3> set_sat(const std::array<float, 3>& c, float s) {
    float mx = std::max({c[0], c[1], c[2]});
    float mn = std::min({c[0], c[1], c[2]});
    if (mx - mn <= 1e-9f) return {0.0f, 0.0f, 0.0f};
    auto f = [&](float v) { return (v - mn) * s / (mx - mn); };
    return {f(c[0]), f(c[1]), f(c[2])};
}

static inline std::array<float, 3> non_separable(BlendMode mode, const std::array<float, 3>& cb, const std::array<float, 3>& cs) {
    switch (mode) {
        case BlendMode::Hue: return set_lum(set_sat(cs, sat(cb)), lum(cb));
        case BlendMode::Saturation: return set_lum(set_sat(cb, sat(cs)), lum(cb));
        case BlendMode::Color: return set_lum(cs, lum(cb));
        case BlendMode::Luminosity: return set_lum(cb, lum(cs));
        case BlendMode::DarkerColor:
            return (luminance(cs[0], cs[1], cs[2]) < luminance(cb[0], cb[1], cb[2])) ? cs : cb;
        case BlendMode::LighterColor:
            return (luminance(cs[0], cs[1], cs[2]) > luminance(cb[0], cb[1], cb[2])) ? cs : cb;
        default:
            return {separable(mode, cb[0], cs[0]), separable(mode, cb[1], cs[1]), separable(mode, cb[2], cs[2])};
    }
}

// after EffectCraft crates/color/src/blend.rs blend_pixel
Px blend_pixel(BlendMode mode, Px dst, Px src, float noise) {
    float sa = src[3];
    float da = dst[3];
    switch (mode) {
        case BlendMode::Normal: {
            float k = 1.0f - sa;
            return {src[0] + dst[0] * k, src[1] + dst[1] * k, src[2] + dst[2] * k, sa + da * k};
        }
        case BlendMode::Dissolve:
        case BlendMode::DancingDissolve: {
            if (sa <= 0.0f || noise >= sa) return dst;
            float inv = 1.0f / sa;
            return {src[0] * inv, src[1] * inv, src[2] * inv, 1.0f};
        }
        case BlendMode::StencilAlpha:
            return {dst[0] * sa, dst[1] * sa, dst[2] * sa, dst[3] * sa};
        case BlendMode::SilhouetteAlpha: {
            float k = 1.0f - sa;
            return {dst[0] * k, dst[1] * k, dst[2] * k, dst[3] * k};
        }
        case BlendMode::StencilLuma:
        case BlendMode::SilhouetteLuma: {
            float l = luminance(src[0], src[1], src[2]);
            float k = (mode == BlendMode::StencilLuma) ? l : (1.0f - l);
            return {dst[0] * k, dst[1] * k, dst[2] * k, dst[3] * k};
        }
        case BlendMode::AlphaAdd: {
            float k = 1.0f - sa;
            return {src[0] + dst[0] * k, src[1] + dst[1] * k, src[2] + dst[2] * k, std::min(1.0f, sa + da)};
        }
        case BlendMode::LuminescentPremul: {
            float k = 1.0f - sa;
            return {src[0] + dst[0] * k, src[1] + dst[1] * k, src[2] + dst[2] * k, sa + da - sa * da};
        }
        default: break;
    }
    if (sa <= 0.0f) return dst;
    if (da <= 0.0f) return src;

    std::array<float, 3> cs{src[0] / sa, src[1] / sa, src[2] / sa};
    std::array<float, 3> cb{dst[0] / da, dst[1] / da, dst[2] / da};
    if (!blend_mode_supports_hdr(mode)) {
        for (int i = 0; i < 3; ++i) {
            cs[i] = std::clamp(cs[i], 0.0f, 1.0f);
            cb[i] = std::clamp(cb[i], 0.0f, 1.0f);
        }
    }
    float ao = sa + da - sa * da;
    Px o{0.0f, 0.0f, 0.0f, ao};
    auto neutral = blend_mode_classic_neutral(mode);
    if (neutral.has_value()) {
        float neut = *neutral;
        std::array<float, 3> cs_faded{
            neut + sa * (cs[0] - neut),
            neut + sa * (cs[1] - neut),
            neut + sa * (cs[2] - neut)
        };
        std::array<float, 3> b = non_separable(mode, cb, cs_faded);
        for (int i = 0; i < 3; ++i) o[i] = (1.0f - da) * src[i] + da * b[i];
    } else {
        std::array<float, 3> b = non_separable(mode, cb, cs);
        for (int i = 0; i < 3; ++i) o[i] = (1.0f - da) * src[i] + (1.0f - sa) * dst[i] + sa * da * b[i];
    }
    return o;
}

// =====================================================================
// Noise, Quantization, Dither (after EffectCraft crates/raster/src/lib.rs)
// =====================================================================

// after EffectCraft crates/raster/src/lib.rs hash_noise
float hash_noise(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t h = (x * 0x8da6b343u) ^ (y * 0xd8163841u) ^ (seed * 0xcb1ab31fu);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float)(h & 0x00ffffffu) / 16777216.0f;
}

static const uint32_t DITHER_SEED = 0x5bd1e995u;

// after EffectCraft crates/raster/src/lib.rs dither_u8
uint8_t dither_u8(float v, uint32_t x, uint32_t y) {
    float s = std::clamp(v, 0.0f, 1.0f) * 255.0f;
    float t = hash_noise(x, y, DITHER_SEED) * (1.0f - 2e-4f) + 1e-4f;
    return (uint8_t)std::floor(s + t);
}

static inline float round_half_up(float v) {
    return (float)(uint32_t)(v + 0.5f);
}

// after EffectCraft crates/raster/src/lib.rs quantize_levels
Px quantize_levels(Px p, float levels, float inv) {
    float na = round_half_up(std::clamp(p[3], 0.0f, 1.0f) * levels);
    float a = na * inv;
    Px n = {0.0f, 0.0f, 0.0f, na};
    for (int c = 0; c < 3; ++c) {
        float hi = a;
        n[c] = round_half_up(std::clamp(p[c], 0.0f, hi) * levels);
    }
    return n;
}

void quantize_px(Px& p, float levels, float inv) {
    Px q = quantize_levels(p, levels, inv);
    p = {q[0] * inv, q[1] * inv, q[2] * inv, q[3] * inv};
}

std::array<uint8_t, 4> quantize_px_u8(Px p) {
    Px q = quantize_levels(p, 255.0f, 1.0f / 255.0f);
    return {(uint8_t)q[0], (uint8_t)q[1], (uint8_t)q[2], (uint8_t)q[3]};
}

Px expand_u8(const std::array<uint8_t, 4>& n) {
    float inv = 1.0f / 255.0f;
    return {(float)n[0] * inv, (float)n[1] * inv, (float)n[2] * inv, (float)n[3] * inv};
}

// =====================================================================
// Image (after EffectCraft crates/raster/src/lib.rs)
// =====================================================================

Image::Image(uint32_t w, uint32_t h)
    : width(w), height(h), data((size_t)w * h, {0.0f, 0.0f, 0.0f, 0.0f}) {}

Image Image::filled(uint32_t w, uint32_t h, Px px) {
    Image img;
    img.width = w;
    img.height = h;
    img.data.assign((size_t)w * h, px);
    return img;
}

// after EffectCraft crates/raster/src/lib.rs from_rgba8
Image Image::from_rgba8(uint32_t width, uint32_t height, const uint8_t* rgba, size_t len) {
    Image img(width, height);
    size_t count = (size_t)width * height;
    if (!rgba || len < count * 4) return img;
    for (size_t i = 0; i < count; ++i) {
        float a = (float)rgba[i * 4 + 3] / 255.0f;
        img.data[i] = {
            ((float)rgba[i * 4 + 0] / 255.0f) * a,
            ((float)rgba[i * 4 + 1] / 255.0f) * a,
            ((float)rgba[i * 4 + 2] / 255.0f) * a,
            a
        };
    }
    return img;
}

Px Image::sample_nearest(double x, double y) const {
    return get((int64_t)std::floor(x), (int64_t)std::floor(y));
}

// after EffectCraft crates/raster/src/lib.rs Image::sample_bilinear
Px Image::sample_bilinear(double x, double y) const {
    double fx = x - 0.5;
    double fy = y - 0.5;
    double x0 = std::floor(fx);
    double y0 = std::floor(fy);
    float tx = (float)(fx - x0);
    float ty = (float)(fy - y0);
    int64_t xi = (int64_t)x0;
    int64_t yi = (int64_t)y0;
    Px a = get(xi, yi);
    Px b = get(xi + 1, yi);
    Px c = get(xi, yi + 1);
    Px d = get(xi + 1, yi + 1);
    Px o;
    for (int i = 0; i < 4; ++i) {
        float top = a[i] + (b[i] - a[i]) * tx;
        float bot = c[i] + (d[i] - c[i]) * tx;
        o[i] = top + (bot - top) * ty;
    }
    return o;
}

Px Image::sample_bilinear_clamped(double x, double y) const {
    double fx = x - 0.5;
    double fy = y - 0.5;
    double x0 = std::floor(fx);
    double y0 = std::floor(fy);
    float tx = (float)(fx - x0);
    float ty = (float)(fy - y0);
    int64_t xi = (int64_t)x0;
    int64_t yi = (int64_t)y0;
    Px a = get_clamped(xi, yi);
    Px b = get_clamped(xi + 1, yi);
    Px c = get_clamped(xi, yi + 1);
    Px d = get_clamped(xi + 1, yi + 1);
    Px o;
    for (int i = 0; i < 4; ++i) {
        float top = a[i] + (b[i] - a[i]) * tx;
        float bot = c[i] + (d[i] - c[i]) * tx;
        o[i] = top + (bot - top) * ty;
    }
    return o;
}

// after EffectCraft crates/raster/src/lib.rs Image::sample_bicubic
Px Image::sample_bicubic(double x, double y) const {
    double fx = x - 0.5;
    double fy = y - 0.5;
    double x0 = std::floor(fx);
    double y0 = std::floor(fy);
    double tx = fx - x0;
    double ty = fy - y0;
    auto w = [](double t) -> std::array<float, 4> {
        double t2 = t * t;
        double t3 = t2 * t;
        return {
            (float)(-0.5 * t3 + t2 - 0.5 * t),
            (float)(1.5 * t3 - 2.5 * t2 + 1.0),
            (float)(-1.5 * t3 + 2.0 * t2 + 0.5 * t),
            (float)(0.5 * t3 - 0.5 * t2)
        };
    };
    auto wx = w(tx);
    auto wy = w(ty);
    Px o{0.0f, 0.0f, 0.0f, 0.0f};
    int64_t xi = (int64_t)x0;
    int64_t yi = (int64_t)y0;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            Px p = get(xi - 1 + i, yi - 1 + j);
            float k = wx[i] * wy[j];
            for (int c = 0; c < 4; ++c) o[c] += p[c] * k;
        }
    }
    o[3] = std::max(0.0f, o[3]);
    for (int c = 0; c < 3; ++c) o[c] = std::max(0.0f, o[c]);
    return o;
}

std::optional<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> Image::alpha_bounds() const {
    uint32_t min_x = UINT32_MAX, min_y = UINT32_MAX, max_x = 0, max_y = 0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            if (get((int64_t)x, (int64_t)y)[3] > 0.0f) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x + 1);
                max_y = std::max(max_y, y + 1);
            }
        }
    }
    if (min_x < max_x && min_y < max_y)
        return std::make_tuple(min_x, min_y, max_x, max_y);
    return std::nullopt;
}

Image Image::padded(uint32_t pad) const {
    uint32_t w_rem = (width < MAX_PADDED_SIDE) ? (MAX_PADDED_SIDE - width) : 0;
    uint32_t h_rem = (height < MAX_PADDED_SIDE) ? (MAX_PADDED_SIDE - height) : 0;
    uint32_t eff = std::min(pad, std::min(w_rem / 2, h_rem / 2));
    uint32_t nw = width + 2 * eff;
    uint32_t nh = height + 2 * eff;
    Image out(nw, nh);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            out.set(x + eff, y + eff, get((int64_t)x, (int64_t)y));
        }
    }
    return out;
}

std::vector<uint8_t> Image::to_rgba8() const {
    std::vector<uint8_t> out((size_t)width * height * 4);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& p = data[i];
        float a = std::clamp(p[3], 0.0f, 1.0f);
        float inv = (a > 0.0f) ? (1.0f / a) : 0.0f;
        out[i * 4 + 0] = (uint8_t)(std::clamp(p[0] * inv, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 1] = (uint8_t)(std::clamp(p[1] * inv, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 2] = (uint8_t)(std::clamp(p[2] * inv, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 3] = (uint8_t)(a * 255.0f + 0.5f);
    }
    return out;
}

std::vector<uint8_t> Image::to_rgba8_dithered() const {
    std::vector<uint8_t> out((size_t)width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t i = (size_t)y * width + x;
            const auto& p = data[i];
            float a = std::clamp(p[3], 0.0f, 1.0f);
            float inv = (a > 0.0f) ? (1.0f / a) : 0.0f;
            out[i * 4 + 0] = dither_u8(p[0] * inv, x, y);
            out[i * 4 + 1] = dither_u8(p[1] * inv, x, y);
            out[i * 4 + 2] = dither_u8(p[2] * inv, x, y);
            out[i * 4 + 3] = (uint8_t)(a * 255.0f + 0.5f);
        }
    }
    return out;
}

std::vector<uint8_t> Image::to_rgba8_over(const std::array<float, 3>& bg) const {
    std::vector<uint8_t> out((size_t)width * height * 4);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& p = data[i];
        float k = 1.0f - std::clamp(p[3], 0.0f, 1.0f);
        out[i * 4 + 0] = (uint8_t)(std::clamp(p[0] + bg[0] * k, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 1] = (uint8_t)(std::clamp(p[1] + bg[1] * k, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 2] = (uint8_t)(std::clamp(p[2] + bg[2] * k, 0.0f, 1.0f) * 255.0f + 0.5f);
        out[i * 4 + 3] = 255;
    }
    return out;
}

std::vector<uint8_t> Image::to_rgba8_over_dithered(const std::array<float, 3>& bg) const {
    std::vector<uint8_t> out((size_t)width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t i = (size_t)y * width + x;
            const auto& p = data[i];
            float k = 1.0f - std::clamp(p[3], 0.0f, 1.0f);
            out[i * 4 + 0] = dither_u8(p[0] + bg[0] * k, x, y);
            out[i * 4 + 1] = dither_u8(p[1] + bg[1] * k, x, y);
            out[i * 4 + 2] = dither_u8(p[2] + bg[2] * k, x, y);
            out[i * 4 + 3] = 255;
        }
    }
    return out;
}

void Image::blend_from(const Image& src, BlendMode mode, float opacity, uint32_t seed) {
    if (src.width != width || src.height != height) return;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t i = (size_t)y * width + x;
            Px s = src.data[i];
            if (opacity < 1.0f) {
                for (float& c : s) c *= opacity;
            }
            float noise = (mode == BlendMode::Dissolve || mode == BlendMode::DancingDissolve)
                          ? hash_noise(x, y, seed) : 0.5f;
            data[i] = blend_pixel(mode, data[i], s, noise);
        }
    }
}

// =====================================================================
// Composite Warping (after EffectCraft crates/raster/src/warp.rs)
// =====================================================================

static inline Px bilerp(Px a, Px b, Px c, Px d, float tx, float ty) {
    Px o;
    for (int i = 0; i < 4; ++i) {
        float top = a[i] + (b[i] - a[i]) * tx;
        float bot = c[i] + (d[i] - c[i]) * tx;
        o[i] = top + (bot - top) * ty;
    }
    return o;
}

static const size_t TILE = 16;

ClearTiles::ClearTiles(const Image& img) {
    size_t w = img.width, h = img.height;
    cols = (w + TILE - 1) / TILE;
    size_t rows = (h + TILE - 1) / TILE;
    clear.assign(rows * cols, true);
    for (size_t ty = 0; ty < rows; ++ty) {
        for (size_t tx = 0; tx < cols; ++tx) {
            bool all_clear = true;
            size_t y_end = std::min(ty * TILE + TILE + 1, h);
            for (size_t y = ty * TILE; y < y_end && all_clear; ++y) {
                size_t x_end = std::min(tx * TILE + TILE + 1, w);
                for (size_t x = tx * TILE; x < x_end; ++x) {
                    if (img.data[y * w + x][3] != 0.0f) {
                        all_clear = false;
                        break;
                    }
                }
            }
            clear[ty * cols + tx] = all_clear;
        }
    }
}

bool ClearTiles::is_clear(size_t x, size_t y) const {
    size_t tx = x / TILE;
    size_t ty = y / TILE;
    size_t idx = ty * cols + tx;
    return (idx < clear.size()) ? clear[idx] : false;
}

static inline std::optional<Px> sample_bilinear_fast(const Image& img, const ClearTiles& clear, double x, double y) {
    double fx = x - 0.5;
    double fy = y - 0.5;
    double x0 = std::floor(fx);
    double y0 = std::floor(fy);
    int64_t xi = (int64_t)x0;
    int64_t yi = (int64_t)y0;
    int64_t w = (int64_t)img.width;
    int64_t h = (int64_t)img.height;
    if (xi >= 0 && yi >= 0 && xi + 1 < w && yi + 1 < h) {
        if (clear.is_clear((size_t)xi, (size_t)yi)) return std::nullopt;
        float tx = (float)(fx - x0);
        float ty = (float)(fy - y0);
        size_t i = (size_t)yi * (size_t)w + (size_t)xi;
        Px a = img.data[i];
        Px b = img.data[i + 1];
        Px c = img.data[i + (size_t)w];
        Px d = img.data[i + (size_t)w + 1];
        return bilerp(a, b, c, d, tx, ty);
    }
    return img.sample_bilinear(x, y);
}

static inline std::pair<size_t, size_t> span(double p0, double s, double min_val, double max_val, size_t n) {
    if (std::abs(s) < 1e-12) {
        if (p0 >= min_val - 1e-9 && p0 <= max_val + 1e-9) return {0, n};
        return {0, 0};
    }
    double a = (min_val - p0) / s;
    double b = (max_val - p0) / s;
    if (a > b) std::swap(a, b);
    double lo = std::max(0.0, std::floor(a) - 1.0);
    double hi = std::min((double)n, std::ceil(b) + 2.0);
    if (hi <= lo) return {0, 0};
    return {(size_t)lo, (size_t)hi};
}

struct RowSpan {
    size_t a = SIZE_MAX;
    size_t b = 0;
    void add(size_t x) {
        a = std::min(a, x);
        b = x + 1;
    }
};

static inline std::array<size_t, 4> union_rect(const std::array<size_t, 4>& r, const std::array<size_t, 4>& q) {
    if (r[0] >= r[2]) return q;
    if (q[0] >= q[2]) return r;
    return {std::min(r[0], q[0]), std::min(r[1], q[1]), std::max(r[2], q[2]), std::max(r[3], q[3])};
}

static Image half_image(const Image& src) {
    uint32_t w = std::max(1u, (src.width + 1) / 2);
    uint32_t h = std::max(1u, (src.height + 1) / 2);
    Image out(w, h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            int64_t sx = (int64_t)x * 2;
            int64_t sy = (int64_t)y * 2;
            Px acc{0.0f, 0.0f, 0.0f, 0.0f};
            const int64_t offsets[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
            for (const auto& off : offsets) {
                Px p = src.get_clamped(sx + off[0], sy + off[1]);
                for (int c = 0; c < 4; ++c) acc[c] += p[c];
            }
            out.set(x, y, {acc[0] * 0.25f, acc[1] * 0.25f, acc[2] * 0.25f, acc[3] * 0.25f});
        }
    }
    return out;
}

template <typename BlendFn>
static std::array<size_t, 4> affine_rows(Image& dst, const Image& img, const Mat3& inv,
                                        Sampling sampling, float op, std::optional<float> quantize,
                                        size_t x0, size_t x1, size_t y0, size_t y1, BlendFn blend) {
    std::optional<std::pair<float, float>> levels;
    if (quantize.has_value()) levels = {*quantize, 1.0f / *quantize};

    size_t w = dst.width;
    const auto& m = inv.m;
    double iw = img.width;
    double ih = img.height;
    Vec2 step = vec2(m[0][0], m[1][0]);
    bool int_tx = (m[0][0] == 1.0 && m[1][1] == 1.0 && m[0][1] == 0.0 && m[1][0] == 0.0
                   && (m[0][2] == std::floor(m[0][2])) && (m[1][2] == std::floor(m[1][2])));
    std::optional<ClearTiles> clear;
    if (sampling == Sampling::Bilinear && !int_tx) {
        clear.emplace(img);
    }

    std::array<size_t, 4> res = {SIZE_MAX, SIZE_MAX, 0, 0};
    for (size_t y = y0; y < y1; ++y) {
        RowSpan changed;
        double fy = (double)y + 0.5;
        Vec2 p0 = vec2(m[0][0] * ((double)x0 + 0.5) + m[0][1] * fy + m[0][2],
                       m[1][0] * ((double)x0 + 0.5) + m[1][1] * fy + m[1][2]);
        size_t n = x1 - x0;
        auto [ax, bx] = span(p0.x, step.x, -1.0, iw + 1.0, n);
        auto [ay, by] = span(p0.y, step.y, -1.0, ih + 1.0, n);
        size_t lo = std::max(ax, ay);
        size_t hi = std::min(bx, by);
        if (lo >= hi) continue;

        Px* row = &dst.data[y * w];
        if (int_tx) {
            int64_t sy = (int64_t)y + (int64_t)m[1][2];
            if (sy >= 0 && sy < (int64_t)img.height) {
                int64_t dx = (int64_t)m[0][2];
                const Px* srow = &img.data[(size_t)sy * img.width];
                int64_t xa_signed = std::max((int64_t)x0 + (int64_t)lo, std::max(-dx, (int64_t)0));
                int64_t xb_signed = std::min((int64_t)(x0 + hi), (int64_t)img.width - dx);
                size_t xa = (size_t)std::max((int64_t)0, xa_signed);
                size_t xb = (size_t)std::max((int64_t)xa, xb_signed);
                for (size_t x = xa; x < xb; ++x) {
                    Px s = srow[(size_t)((int64_t)x + dx)];
                    if (sampling == Sampling::Bicubic) {
                        for (int c = 0; c < 4; ++c) s[c] = std::max(0.0f, s[c]);
                    }
                    if (s[3] <= 0.0f) continue;
                    if (op < 1.0f) {
                        for (int c = 0; c < 4; ++c) s[c] *= op;
                    }
                    row[x] = blend(row[x], s, (uint32_t)x, (uint32_t)y);
                    if (levels.has_value()) {
                        quantize_px(row[x], levels->first, levels->second);
                    }
                    changed.add(x);
                }
            }
        } else {
            for (size_t i = lo; i < hi; ++i) {
                size_t x = x0 + i;
                Vec2 p = vec2(p0.x + step.x * (double)i, p0.y + step.y * (double)i);
                if (p.x < -1.0 || p.y < -1.0 || p.x > iw + 1.0 || p.y > ih + 1.0) continue;
                Px s;
                if (sampling == Sampling::Nearest) {
                    s = img.get((int64_t)std::floor(p.x), (int64_t)std::floor(p.y));
                } else if (sampling == Sampling::Bilinear) {
                    if (clear.has_value()) {
                        auto fast_s = sample_bilinear_fast(img, *clear, p.x, p.y);
                        if (!fast_s.has_value()) continue;
                        s = *fast_s;
                    } else {
                        s = img.sample_bilinear(p.x, p.y);
                    }
                } else {
                    s = img.sample_bicubic(p.x, p.y);
                }
                if (s[3] <= 0.0f) continue;
                if (op < 1.0f) {
                    for (int c = 0; c < 4; ++c) s[c] *= op;
                }
                row[x] = blend(row[x], s, (uint32_t)x, (uint32_t)y);
                if (levels.has_value()) {
                    quantize_px(row[x], levels->first, levels->second);
                }
                changed.add(x);
            }
        }
        if (changed.a < changed.b) {
            std::array<size_t, 4> row_res = {changed.a, y, changed.b, y + 1};
            res = union_rect(res, row_res);
        }
    }
    return res;
}

// after EffectCraft crates/raster/src/warp.rs composite_warp_impl
static std::optional<std::array<uint32_t, 4>> composite_warp_impl(
    Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts,
    bool allow_fast, std::optional<float> accumulate, std::optional<float> quantize) {

    const std::optional<std::array<uint32_t, 4>> NOTHING = std::array<uint32_t, 4>{0, 0, 0, 0};
    if (src.is_empty() || dst.is_empty() || opts.opacity <= 0.0f) return NOTHING;
    auto inv_opt = m.inverse();
    if (!inv_opt.has_value()) return NOTHING;
    Mat3 inv = *inv_opt;

    double scale = m.mean_scale();
    Image level_img;
    const Image* img = &src;
    if (scale < 0.5 && m.is_affine()) {
        double s = scale;
        double f = 1.0;
        level_img = half_image(src);
        f *= 0.5;
        s *= 2.0;
        while (s < 0.5 && level_img.width > 1 && level_img.height > 1) {
            level_img = half_image(level_img);
            f *= 0.5;
            s *= 2.0;
        }
        img = &level_img;
        inv = Mat3::scale(vec2(f, f)) * inv;
    }

    Rect bounds = m.map_rect(Rect::from_size((double)src.width, (double)src.height)).inflate(1.0).round_out();
    Rect full = Rect::from_size((double)dst.width, (double)dst.height);
    Rect area = bounds.intersect(full);
    if (opts.clip.has_value()) {
        area = area.intersect(*opts.clip);
    }
    if (area.is_empty()) return NOTHING;

    size_t x0 = (size_t)std::clamp(area.x0, 0.0, (double)dst.width);
    size_t x1 = (size_t)std::clamp(area.x1, 0.0, (double)dst.width);
    size_t y0 = (size_t)std::clamp(area.y0, 0.0, (double)dst.height);
    size_t y1 = (size_t)std::clamp(area.y1, 0.0, (double)dst.height);
    if (x0 >= x1 || y0 >= y1) return NOTHING;

    size_t w = dst.width;
    bool affine = inv.is_affine();
    BlendMode mode = opts.mode;
    float op = opts.opacity;

    auto changed = [](const std::array<size_t, 4>& r) -> std::optional<std::array<uint32_t, 4>> {
        return std::array<uint32_t, 4>{(uint32_t)r[0], (uint32_t)r[1], (uint32_t)r[2], (uint32_t)r[3]};
    };

    if (accumulate.has_value() && affine) {
        float k = *accumulate;
        auto r = affine_rows(dst, *img, inv, opts.sampling, 1.0f, std::nullopt, x0, x1, y0, y1,
            [k](Px d, Px s, uint32_t, uint32_t) -> Px {
                return {d[0] + s[0] * k, d[1] + s[1] * k, d[2] + s[2] * k, d[3] + s[3] * k};
            });
        return changed(r);
    }

    if (allow_fast && affine && !blend_mode_is_stencil(mode)) {
        if (mode == BlendMode::Normal) {
            auto r = affine_rows(dst, *img, inv, opts.sampling, op, quantize, x0, x1, y0, y1,
                [](Px d, Px s, uint32_t, uint32_t) -> Px {
                    float k = 1.0f - s[3];
                    return {s[0] + d[0] * k, s[1] + d[1] * k, s[2] + d[2] * k, s[3] + d[3] * k};
                });
            return changed(r);
        } else if (mode == BlendMode::Dissolve || mode == BlendMode::DancingDissolve) {
            uint32_t seed = opts.seed;
            auto r = affine_rows(dst, *img, inv, opts.sampling, op, quantize, x0, x1, y0, y1,
                [mode, seed](Px d, Px s, uint32_t x, uint32_t y) -> Px {
                    return blend_pixel(mode, d, s, hash_noise(x, y, seed));
                });
            return changed(r);
        } else {
            auto r = affine_rows(dst, *img, inv, opts.sampling, op, quantize, x0, x1, y0, y1,
                [mode](Px d, Px s, uint32_t, uint32_t) -> Px {
                    return blend_pixel(mode, d, s, 0.5f);
                });
            return changed(r);
        }
    }

    // Reference per-pixel path (non-affine or stencil modes)
    for (size_t y = y0; y < y1; ++y) {
        double fy = (double)y + 0.5;
        Vec2 sp = inv.apply(vec2((double)x0 + 0.5, fy));
        Vec2 step = inv.apply_vec(vec2(1.0, 0.0));
        Px* row = &dst.data[y * w];
        for (size_t x = x0; x < x1; ++x) {
            Vec2 p = affine ? sp : inv.apply(vec2((double)x + 0.5, fy));
            if (affine) sp += step;
            if (p.x < -1.0 || p.y < -1.0 || p.x > (double)img->width + 1.0 || p.y > (double)img->height + 1.0) {
                if (blend_mode_is_stencil(mode)) {
                    row[x] = blend_pixel(mode, row[x], {0.0f, 0.0f, 0.0f, 0.0f}, 0.5f);
                }
                continue;
            }
            Px s;
            if (opts.sampling == Sampling::Nearest) {
                s = img->get((int64_t)std::floor(p.x), (int64_t)std::floor(p.y));
            } else if (opts.sampling == Sampling::Bilinear) {
                s = img->sample_bilinear(p.x, p.y);
            } else {
                s = img->sample_bicubic(p.x, p.y);
            }
            if (s[3] <= 0.0f && !blend_mode_is_stencil(mode)) continue;
            if (op < 1.0f) {
                for (int c = 0; c < 4; ++c) s[c] *= op;
            }
            float noise = (mode == BlendMode::Dissolve || mode == BlendMode::DancingDissolve)
                          ? hash_noise((uint32_t)x, (uint32_t)y, opts.seed) : 0.5f;
            row[x] = blend_pixel(mode, row[x], s, noise);
        }
    }
    return std::nullopt;
}

void composite_warp(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts) {
    composite_warp_impl(dst, src, m, opts, true, std::nullopt, std::nullopt);
}

void composite_warp_reference(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts) {
    composite_warp_impl(dst, src, m, opts, false, std::nullopt, std::nullopt);
}

std::optional<std::array<uint32_t, 4>> composite_warp_changed(Image& dst, const Image& src, const Mat3& m, const WarpOpts& opts, std::optional<float> quantize) {
    return composite_warp_impl(dst, src, m, opts, true, std::nullopt, quantize);
}

void accumulate_warp(Image& dst, const Image& src, const Mat3& m, Sampling sampling, float weight) {
    WarpOpts opts;
    opts.sampling = sampling;
    auto inv = m.inverse();
    if (m.is_affine() && inv.has_value() && inv->is_affine()) {
        composite_warp_impl(dst, src, m, opts, true, weight, std::nullopt);
        return;
    }
    Image tmp(dst.width, dst.height);
    composite_warp(tmp, src, m, opts);
    for (size_t i = 0; i < dst.data.size(); ++i) {
        for (int c = 0; c < 4; ++c) dst.data[i][c] += tmp.data[i][c] * weight;
    }
}

// after EffectCraft crates/raster/src/warp.rs resample
Image resample(const Image& src, uint32_t w, uint32_t h) {
    Image out(w, h);
    if (src.width == 0 || src.height == 0) return out;
    Mat3 m = Mat3::scale(vec2((double)w / (double)src.width, (double)h / (double)src.height));
    WarpOpts opts;
    composite_warp(out, src, m, opts);
    return out;
}

} // namespace Raster
