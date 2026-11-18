// Morpheus - keyframe and property values implementation.
// Ported function-by-function from EffectCraft crates/keyframe and crates/geom.
// after EffectCraft (crates/keyframe/src/value.rs, crates/keyframe/src/lib.rs,
//                    crates/geom/src/lib.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "keyframe.h"

#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace KeyframeModel {

using Project::TICKS_PER_SECOND;

const Ease Ease::EASY = Ease{0.0, 1.0 / 3.0};

static constexpr double PI = 3.14159265358979323846;

static inline double to_radians(double deg) {
    return deg * (PI / 180.0);
}
static inline double to_degrees(double rad) {
    return rad * (180.0 / PI);
}
static inline double secs(Tick t) {
    return (double)t.v / (double)TICKS_PER_SECOND;
}

// =====================================================================
// ShapePath  (after EffectCraft crates/keyframe/src/value.rs)
// =====================================================================

ShapePath ShapePath::polygon(const std::vector<std::array<double, 2>>& pts, bool closed) {
    ShapePath p;
    p.vertices = pts;
    p.in_tangents.assign(pts.size(), {0.0, 0.0});
    p.out_tangents.assign(pts.size(), {0.0, 0.0});
    p.closed = closed;
    return p;
}

ShapePath ShapePath::rect(std::array<double, 2> c, double w, double h) {
    double x0 = c[0] - w * 0.5;
    double y0 = c[1] - h * 0.5;
    double x1 = c[0] + w * 0.5;
    double y1 = c[1] + h * 0.5;
    return ShapePath::polygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, true);
}

ShapePath ShapePath::ellipse(std::array<double, 2> c, double w, double h) {
    const double K = 0.5522847498;
    double rx = w * 0.5;
    double ry = h * 0.5;
    ShapePath p;
    p.vertices = {
        {c[0], c[1] - ry},
        {c[0] + rx, c[1]},
        {c[0], c[1] + ry},
        {c[0] - rx, c[1]}
    };
    p.in_tangents = {
        {-rx * K, 0.0},
        {0.0, -ry * K},
        {rx * K, 0.0},
        {0.0, ry * K}
    };
    p.out_tangents = {
        {rx * K, 0.0},
        {0.0, ry * K},
        {-rx * K, 0.0},
        {0.0, -ry * K}
    };
    p.closed = true;
    return p;
}

bool ShapePath::fit_tangents() {
    size_t n = vertices.size();
    bool changed = (in_tangents.size() != n || out_tangents.size() != n);
    in_tangents.resize(n, {0.0, 0.0});
    out_tangents.resize(n, {0.0, 0.0});
    return changed;
}

void ShapePath::feather_after_insert(size_t i, const double* split) {
    for (auto& f : feather) {
        if (i > 0 && f.segment == i - 1) {
            if (split && *split > 0.0 && *split < 1.0) {
                double t = *split;
                if (f.t < t) {
                    f.t /= t;
                } else {
                    f.segment += 1;
                    f.t = (f.t - t) / (1.0 - t);
                }
            }
        } else if (f.segment >= i) {
            f.segment += 1;
        }
    }
}

void ShapePath::feather_after_remove(size_t i) {
    size_t segs = segment_count();
    auto it = std::remove_if(feather.begin(), feather.end(), [&](FeatherPoint& f) {
        if (i == 0 && f.segment == 0 && !closed) return true;
        if (i == 0 && closed && segs > 0) {
            if (f.segment == segs) {
                f.segment = segs - 1;
                f.t *= 0.5;
            } else if (f.segment == 0) {
                f.segment = segs - 1;
                f.t = 0.5 + f.t * 0.5;
            } else {
                f.segment -= 1;
            }
        } else if (i > 0 && f.segment == i - 1) {
            f.t *= 0.5;
        } else if (f.segment == i && i > 0) {
            f.segment = i - 1;
            f.t = 0.5 + f.t * 0.5;
        } else if (f.segment > i || (i == 0 && f.segment > 0)) {
            f.segment -= 1;
        }
        return f.segment >= (segs > 1 ? segs : 1);
    });
    feather.erase(it, feather.end());
}

ShapePath ShapePath::lerp(const ShapePath& o, double t) const {
    if (len() != o.len()) {
        return t < 1.0 ? *this : o;
    }
    ShapePath res;
    res.closed = closed;
    size_t n = len();
    res.vertices.resize(n);
    res.in_tangents.resize(n);
    res.out_tangents.resize(n);
    for (size_t i = 0; i < n; ++i) {
        res.vertices[i] = {
            vertices[i][0] + (o.vertices[i][0] - vertices[i][0]) * t,
            vertices[i][1] + (o.vertices[i][1] - vertices[i][1]) * t
        };
        res.in_tangents[i] = {
            in_tangents[i][0] + (o.in_tangents[i][0] - in_tangents[i][0]) * t,
            in_tangents[i][1] + (o.in_tangents[i][1] - in_tangents[i][1]) * t
        };
        res.out_tangents[i] = {
            out_tangents[i][0] + (o.out_tangents[i][0] - out_tangents[i][0]) * t,
            out_tangents[i][1] + (o.out_tangents[i][1] - out_tangents[i][1]) * t
        };
    }
    if (feather.size() == o.feather.size()) {
        res.feather.resize(feather.size());
        for (size_t i = 0; i < feather.size(); ++i) {
            const auto& a = feather[i];
            const auto& b = o.feather[i];
            FeatherPoint fp;
            fp.segment = t < 1.0 ? a.segment : b.segment;
            fp.t = (a.segment == b.segment) ? a.t + (b.t - a.t) * t : (t < 1.0 ? a.t : b.t);
            fp.radius = a.radius + (b.radius - a.radius) * t;
            fp.tension = a.tension + (b.tension - a.tension) * t;
            res.feather[i] = fp;
        }
    } else {
        res.feather = t < 1.0 ? feather : o.feather;
    }
    return res;
}

bool ShapePath::operator==(const ShapePath& o) const {
    return closed == o.closed && vertices == o.vertices &&
           in_tangents == o.in_tangents && out_tangents == o.out_tangents &&
           feather == o.feather;
}

// =====================================================================
// Gradient  (after EffectCraft crates/keyframe/src/value.rs)
// =====================================================================

Gradient Gradient::default_gradient() {
    Gradient g;
    g.colors = {{0.0, {1.0f, 1.0f, 1.0f, 1.0f}}, {1.0, {0.0f, 0.0f, 0.0f, 1.0f}}};
    g.opacities = {{0.0, 1.0f}, {1.0, 1.0f}};
    return g;
}

template <typename T, typename LerpFn>
static bool sample_stops(const std::vector<std::pair<double, T>>& stops, double t, LerpFn lerp, T& out) {
    if (stops.empty()) return false;
    size_t count = std::min(stops.size(), MAX_GRADIENT_STOPS);
    if (t <= stops[0].first) {
        out = stops[0].second;
        return true;
    }
    for (size_t i = 0; i + 1 < count; ++i) {
        if (t <= stops[i+1].first) {
            double span = std::max(stops[i+1].first - stops[i].first, 1e-12);
            double f = (t - stops[i].first) / span;
            out = lerp(stops[i].second, stops[i+1].second, f);
            return true;
        }
    }
    out = stops[count - 1].second;
    return true;
}

std::array<float, 4> Gradient::color_at(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    std::array<float, 4> out = {1.0f, 1.0f, 1.0f, 1.0f};
    sample_stops(colors, t, [](const std::array<float, 4>& a, const std::array<float, 4>& b, double f) {
        return std::array<float, 4>{
            a[0] + (float)((b[0] - a[0]) * f),
            a[1] + (float)((b[1] - a[1]) * f),
            a[2] + (float)((b[2] - a[2]) * f),
            a[3] + (float)((b[3] - a[3]) * f)
        };
    }, out);
    return out;
}

float Gradient::opacity_at(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    float out = 1.0f;
    sample_stops(opacities, t, [](float a, float b, double f) {
        return a + (float)((b - a) * f);
    }, out);
    return out;
}

std::array<float, 4> Gradient::sample(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    auto c = color_at(t);
    float a = opacity_at(t);
    return {c[0], c[1], c[2], c[3] * a};
}

bool Gradient::operator==(const Gradient& o) const {
    return colors == o.colors && opacities == o.opacities;
}

// =====================================================================
// Value  (after EffectCraft crates/keyframe/src/value.rs)
// =====================================================================

std::vector<double> Value::components() const {
    switch (kind) {
    case ValueKind::Scalar: return {scalar};
    case ValueKind::Vec2:   return {v2[0], v2[1]};
    case ValueKind::Vec3:   return {v3[0], v3[1], v3[2]};
    case ValueKind::Color:  return {color[0], color[1], color[2], color[3]};
    case ValueKind::Bool:   return {b ? 1.0 : 0.0};
    case ValueKind::Enum:   return {(double)e};
    default:                return {};
    }
}

size_t Value::dims() const {
    switch (kind) {
    case ValueKind::Scalar:
    case ValueKind::Bool:
    case ValueKind::Enum:   return 1;
    case ValueKind::Vec2:   return 2;
    case ValueKind::Vec3:   return 3;
    case ValueKind::Color:  return 4;
    default:                return 0;
    }
}

Value Value::with_components(const std::vector<double>& c) const {
    auto g = [&](size_t idx, double def = 0.0) -> double {
        return idx < c.size() ? c[idx] : def;
    };
    switch (kind) {
    case ValueKind::Scalar: return Value::Scalar(g(0));
    case ValueKind::Vec2:   return Value::Vec2({g(0), g(1)});
    case ValueKind::Vec3:   return Value::Vec3({g(0), g(1), g(2)});
    case ValueKind::Color:  return Value::Color({g(0), g(1), g(2), g(3, 1.0)});
    case ValueKind::Bool:   return Value::Bool(g(0) >= 0.5);
    case ValueKind::Enum:   return Value::Enum((uint32_t)std::max(0.0, std::round(g(0))));
    default:                return *this;
    }
}

bool Value::interpolates() const {
    return kind == ValueKind::Scalar || kind == ValueKind::Vec2 ||
           kind == ValueKind::Vec3 || kind == ValueKind::Color ||
           kind == ValueKind::Path || kind == ValueKind::Gradient;
}

Value Value::lerp(const Value& o, double t) const {
    if (kind == ValueKind::Path && o.kind == ValueKind::Path) {
        return Value::Path(path.lerp(o.path, t));
    }
    if (kind == ValueKind::Gradient && o.kind == ValueKind::Gradient &&
        grad.colors.size() == o.grad.colors.size() &&
        grad.opacities.size() == o.grad.opacities.size()) {
        Gradient res;
        res.colors.resize(grad.colors.size());
        for (size_t i = 0; i < grad.colors.size(); ++i) {
            double p = grad.colors[i].first + (o.grad.colors[i].first - grad.colors[i].first) * t;
            std::array<float, 4> c;
            for (int k = 0; k < 4; ++k) {
                c[k] = grad.colors[i].second[k] + (float)((o.grad.colors[i].second[k] - grad.colors[i].second[k]) * t);
            }
            res.colors[i] = {p, c};
        }
        res.opacities.resize(grad.opacities.size());
        for (size_t i = 0; i < grad.opacities.size(); ++i) {
            double p = grad.opacities[i].first + (o.grad.opacities[i].first - grad.opacities[i].first) * t;
            float a = grad.opacities[i].second + (float)((o.grad.opacities[i].second - grad.opacities[i].second) * t);
            res.opacities[i] = {p, a};
        }
        return Value::Grad(res);
    }
    if (interpolates() && dims() > 0 && dims() == o.dims()) {
        auto a = components();
        auto b = o.components();
        std::vector<double> out(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            out[i] = a[i] + (b[i] - a[i]) * t;
        }
        return with_components(out);
    }
    return t < 1.0 ? *this : o;
}

double Value::as_f64() const {
    auto c = components();
    return c.empty() ? 0.0 : c[0];
}

std::array<double, 2> Value::as_vec2() const {
    auto c = components();
    return {c.size() > 0 ? c[0] : 0.0, c.size() > 1 ? c[1] : 0.0};
}

std::array<double, 3> Value::as_vec3() const {
    auto c = components();
    return {c.size() > 0 ? c[0] : 0.0, c.size() > 1 ? c[1] : 0.0, c.size() > 2 ? c[2] : 0.0};
}

std::array<float, 4> Value::as_color() const {
    auto c = components();
    return {
        (float)(c.size() > 0 ? c[0] : 0.0),
        (float)(c.size() > 1 ? c[1] : 0.0),
        (float)(c.size() > 2 ? c[2] : 0.0),
        (float)(c.size() > 3 ? c[3] : 1.0)
    };
}

bool Value::as_bool() const {
    if (kind == ValueKind::Bool) return b;
    return as_f64() != 0.0;
}

uint32_t Value::as_enum() const {
    if (kind == ValueKind::Enum) return e;
    return (uint32_t)std::max(0.0, as_f64());
}

const char* Value::kind_name() const {
    switch (kind) {
    case ValueKind::Scalar:   return "scalar";
    case ValueKind::Vec2:     return "vec2";
    case ValueKind::Vec3:     return "vec3";
    case ValueKind::Color:    return "color";
    case ValueKind::Bool:     return "bool";
    case ValueKind::Enum:     return "enum";
    case ValueKind::Path:     return "path";
    case ValueKind::Str:      return "string";
    case ValueKind::Layer:    return "layer";
    case ValueKind::Gradient: return "gradient";
    }
    return "unknown";
}

bool Value::operator==(const Value& o) const {
    if (kind != o.kind) return false;
    switch (kind) {
    case ValueKind::Scalar:   return scalar == o.scalar;
    case ValueKind::Vec2:     return v2 == o.v2;
    case ValueKind::Vec3:     return v3 == o.v3;
    case ValueKind::Color:    return color == o.color;
    case ValueKind::Bool:     return b == o.b;
    case ValueKind::Enum:     return e == o.e;
    case ValueKind::Path:     return path == o.path;
    case ValueKind::Str:      return str == o.str;
    case ValueKind::Layer:    return layer == o.layer;
    case ValueKind::Gradient: return grad == o.grad;
    }
    return false;
}

bool Keyframe::operator==(const Keyframe& o) const {
    return time == o.time && value == o.value &&
           in_interp == o.in_interp && out_interp == o.out_interp &&
           in_ease == o.in_ease && out_ease == o.out_ease &&
           continuous == o.continuous && auto_bezier == o.auto_bezier &&
           spatial_in == o.spatial_in && spatial_out == o.spatial_out &&
           spatial_auto == o.spatial_auto && spatial_continuous == o.spatial_continuous &&
           roving == o.roving && label == o.label && muted == o.muted;
}

// =====================================================================
// Bezier solvers & Easing (after EffectCraft crates/keyframe/src/lib.rs)
// =====================================================================

static inline double bez(double p0, double p1, double p2, double p3, double u) {
    double v = 1.0 - u;
    return v * v * v * p0 + 3.0 * v * v * u * p1 + 3.0 * v * u * u * p2 + u * u * u * p3;
}

static inline double bez_d(double p0, double p1, double p2, double p3, double u) {
    double v = 1.0 - u;
    return 3.0 * v * v * (p1 - p0) + 6.0 * v * u * (p2 - p1) + 3.0 * u * u * (p3 - p2);
}

static double solve_u(double x0, double x1, double x2, double x3, double x) {
    if (x <= x0) return 0.0;
    if (x >= x3) return 1.0;
    if (x1 == x3 && x2 == x0) {
        double progress = (x - x0) / (x3 - x0);
        return 0.5 + std::cbrt((progress - 0.5) * 0.25);
    }
    double u = (x - x0) / (x3 - x0);
    for (int it = 0; it < 8; ++it) {
        double f = bez(x0, x1, x2, x3, u) - x;
        double d = bez_d(x0, x1, x2, x3, u);
        if (std::abs(d) < 1e-12) break;
        if (std::abs(f / d) < 1e-12) return u;
        double n = u - f / d;
        if (n < 0.0 || n > 1.0) break;
        u = n;
    }
    double lo = 0.0, hi = 1.0;
    for (int it = 0; it < 60; ++it) {
        double mid = (lo + hi) * 0.5;
        if (bez(x0, x1, x2, x3, mid) < x) lo = mid;
        else hi = mid;
    }
    return (lo + hi) * 0.5;
}

double ease_progress(double out_speed_n, double out_inf, double in_speed_n, double in_inf, double x) {
    double i0 = std::clamp(out_inf, 0.0, 1.0);
    double i1 = std::clamp(in_inf, 0.0, 1.0);
    double u = solve_u(0.0, i0, 1.0 - i1, 1.0, x);
    return bez(0.0, out_speed_n * i0, 1.0 - in_speed_n * i1, 1.0, u);
}

static bool on_chord(std::array<double, 3> a, std::array<double, 3> c, std::array<double, 3> b) {
    std::array<double, 3> d = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    std::array<double, 3> v = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (dd <= 1e-24) return false;
    double t = (v[0] * d[0] + v[1] * d[1] + v[2] * d[2]) / dd;
    std::array<double, 3> off = {v[0] - d[0] * t, v[1] - d[1] * t, v[2] - d[2] * t};
    return (t >= 0.0 && t <= 1.0) && (off[0] * off[0] + off[1] * off[1] + off[2] * off[2] <= dd * 1e-18);
}

// =====================================================================
// SpatialSeg arc-length table (after EffectCraft crates/keyframe/src/lib.rs)
// =====================================================================

static constexpr size_t ARC_N = 96;
static const struct GaussPt { double x; double w; } GAUSS[5] = {
    {0.0, 0.5688888888888889},
    {-0.5384693101056831, 0.4786286704993665},
    { 0.5384693101056831, 0.4786286704993665},
    {-0.9061798459386640, 0.2369268850561891},
    { 0.9061798459386640, 0.2369268850561891}
};

struct SpatialSeg {
    std::array<double, 3> p[4];
    std::vector<double> table;

    SpatialSeg(const std::array<double, 3> pts[4]) {
        for (int i = 0; i < 4; ++i) p[i] = pts[i];
        table.reserve(ARC_N + 1);
        table.push_back(0.0);
        double acc = 0.0;
        for (size_t i = 1; i <= ARC_N; ++i) {
            acc += arc((double)(i - 1) / (double)ARC_N, (double)i / (double)ARC_N);
            table.push_back(acc);
        }
    }

    std::array<double, 3> point(double u) const {
        return {
            bez(p[0][0], p[1][0], p[2][0], p[3][0], u),
            bez(p[0][1], p[1][1], p[2][1], p[3][1], u),
            bez(p[0][2], p[1][2], p[2][2], p[3][2], u)
        };
    }

    double tangent_len(double u) const {
        double dx = bez_d(p[0][0], p[1][0], p[2][0], p[3][0], u);
        double dy = bez_d(p[0][1], p[1][1], p[2][1], p[3][1], u);
        double dz = bez_d(p[0][2], p[1][2], p[2][2], p[3][2], u);
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double arc(double a, double b) const {
        double m = (a + b) * 0.5;
        double h = (b - a) * 0.5;
        double sum = 0.0;
        for (const auto& g : GAUSS) {
            sum += g.w * tangent_len(m + h * g.x);
        }
        return sum * h;
    }

    double length() const {
        return table.empty() ? 0.0 : table.back();
    }

    std::array<double, 3> at_length(double s) const {
        double len = length();
        if (!(len > 0.0 && std::isfinite(len))) return p[0];
        s = std::clamp(s, 0.0, len);
        auto it = std::lower_bound(table.begin(), table.end(), s);
        size_t i = (size_t)std::distance(table.begin(), it);
        i = std::clamp(i, (size_t)1, ARC_N);
        double a = table[i - 1];
        double b = table[i];
        double u0 = (double)(i - 1) / (double)ARC_N;
        double u1 = (double)i / (double)ARC_N;
        double u = u0 + (b > a ? (s - a) / (b - a) : 0.0) * (u1 - u0);
        for (int step = 0; step < 4; ++step) {
            double d = tangent_len(u);
            if (d <= 1e-12) break;
            double e = a + arc(u0, u) - s;
            u = std::clamp(u - e / d, u0, u1);
            if (std::abs(e) <= 1e-12 * std::max(len, 1.0)) break;
        }
        return point(u);
    }
};

static std::pair<std::array<double, 3>, std::array<double, 3>> auto_spatial(const std::vector<Keyframe>& keys, size_t i) {
    if (i >= keys.size() || keys.size() < 2) return {{0,0,0}, {0,0,0}};
    size_t prev_i = (i > 0) ? i - 1 : 0;
    size_t next_i = (i + 1 < keys.size()) ? i + 1 : keys.size() - 1;
    auto a = keys[prev_i].value.as_vec3();
    auto c = keys[next_i].value.as_vec3();
    std::array<double, 3> tout = {(c[0] - a[0]) / 6.0, (c[1] - a[1]) / 6.0, (c[2] - a[2]) / 6.0};
    auto p = keys[i].value.as_vec3();
    std::array<double, 3> tin = (i > 0 && a == p) ? std::array<double, 3>{0,0,0} : std::array<double, 3>{-tout[0], -tout[1], -tout[2]};
    if (i + 1 < keys.size() && c == p) tout = {0,0,0};
    return {tin, tout};
}

std::pair<std::array<double, 3>, std::array<double, 3>> spatial_tangents(const std::vector<Keyframe>& keys, size_t i) {
    if (i >= keys.size()) return {{0,0,0}, {0,0,0}};
    if (keys[i].spatial_auto) return auto_spatial(keys, i);
    return {keys[i].spatial_in, keys[i].spatial_out};
}

std::pair<std::array<double, 3>, std::array<double, 3>> spatial_handles(const std::vector<Keyframe>& keys, size_t i) {
    auto tang = spatial_tangents(keys, i);
    auto tin = tang.first;
    auto tout = tang.second;
    if (i >= keys.size()) return {tin, tout};
    auto p = keys[i].value.as_vec3();
    auto toward = [&](size_t j, bool after) -> std::pair<bool, std::array<double, 3>> {
        if (j >= keys.size()) return {false, {0,0,0}};
        auto q = keys[j].value.as_vec3();
        auto jtang = spatial_tangents(keys, j);
        auto t = after ? jtang.first : jtang.second;
        std::array<double, 3> d = {q[0] + t[0] - p[0], q[1] + t[1] - p[1], q[2] + t[2] - p[2]};
        double dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        double len = std::sqrt((q[0]-p[0])*(q[0]-p[0]) + (q[1]-p[1])*(q[1]-p[1]) + (q[2]-p[2])*(q[2]-p[2])) / 3.0;
        if (dl > 1e-9) return {true, {d[0] * len / dl, d[1] * len / dl, d[2] * len / dl}};
        return {false, {0,0,0}};
    };
    auto is_zero = [](const std::array<double, 3>& t) { return t[0] == 0.0 && t[1] == 0.0 && t[2] == 0.0; };
    if (is_zero(tout)) {
        auto tw = toward(i + 1, true);
        if (tw.first) tout = tw.second;
    }
    if (is_zero(tin) && i > 0) {
        auto tw = toward(i - 1, false);
        if (tw.first) tin = tw.second;
    }
    return {tin, tout};
}

double spatial_segment_length(const std::vector<Keyframe>& keys, size_t i) {
    if (i + 1 >= keys.size()) return 0.0;
    auto p0 = keys[i].value.as_vec3();
    auto p3 = keys[i + 1].value.as_vec3();
    auto tang_i = spatial_tangents(keys, i);
    auto tang_next = spatial_tangents(keys, i + 1);
    auto out_t = tang_i.second;
    auto in_t = tang_next.first;
    std::array<double, 3> pts[4] = {
        p0,
        {p0[0] + out_t[0], p0[1] + out_t[1], p0[2] + out_t[2]},
        {p3[0] + in_t[0],  p3[1] + in_t[1],  p3[2] + in_t[2]},
        p3
    };
    return SpatialSeg(pts).length();
}

static Ease effective_ease(const std::vector<Keyframe>& keys, size_t i, size_t d, bool spatial, bool out) {
    const auto& k = keys[i];
    const auto& list = out ? k.out_ease : k.in_ease;
    if (k.auto_bezier && i > 0 && i + 1 < keys.size()) {
        double dt = secs(keys[i + 1].time) - secs(keys[i - 1].time);
        double slope = 0.0;
        if (spatial) {
            auto a = keys[i - 1].value.as_vec3();
            auto c = keys[i + 1].value.as_vec3();
            slope = std::sqrt((c[0]-a[0])*(c[0]-a[0]) + (c[1]-a[1])*(c[1]-a[1]) + (c[2]-a[2])*(c[2]-a[2])) / std::max(dt, 1e-12);
        } else {
            auto a = keys[i - 1].value.components();
            auto c = keys[i + 1].value.components();
            double va = d < a.size() ? a[d] : 0.0;
            double vc = d < c.size() ? c[d] : 0.0;
            slope = (vc - va) / std::max(dt, 1e-12);
        }
        return Ease{slope, 1.0 / 6.0};
    }
    if (d < list.size()) return list[d];
    if (!list.empty()) return list[0];
    return Ease{};
}

// after EffectCraft (crates/keyframe/src/lib.rs side_ease)
std::optional<Ease> side_ease(const std::vector<Keyframe>& keys, size_t i, size_t d, bool spatial, bool out) {
    if (i >= keys.size()) return std::nullopt;
    const auto& k = keys[i];
    if (out) {
        if (i + 1 >= keys.size()) return std::nullopt;
    } else {
        if (i == 0) return std::nullopt;
    }
    size_t j = out ? (i + 1) : (i - 1);
    const auto& n = keys[j];
    Interp interp = out ? k.out_interp : k.in_interp;
    if (interp == Interp::Hold || (!out && n.out_interp == Interp::Hold) || !k.value.interpolates()) {
        return std::nullopt;
    }
    bool is_spatial = spatial && (k.value.kind == ValueKind::Vec2 || k.value.kind == ValueKind::Vec3);
    if (interp == Interp::Linear) {
        double dur = std::max(std::fabs(secs(k.time) - secs(n.time)), 1e-12);
        double dv = 0.0;
        if (is_spatial) {
            size_t a = std::min(i, j);
            dv = spatial_segment_length(keys, a);
        } else {
            auto a = k.value.components();
            auto b = n.value.components();
            double va = (d < a.size()) ? a[d] : 0.0;
            double vb = (d < b.size()) ? b[d] : 0.0;
            dv = vb - va;
            if (!out) dv = -dv;
        }
        return Ease{dv / dur, LINEAR_SIDE_INFLUENCE};
    }
    return effective_ease(keys, i, is_spatial ? 0 : d, is_spatial, out);
}

bool retime_roving(std::vector<Keyframe>& keys, bool spatial) {
    size_t n = keys.size();
    if (n < 3) {
        for (auto& k : keys) k.roving = false;
        return false;
    }
    keys[0].roving = false;
    keys[n - 1].roving = false;
    bool has_roving = false;
    for (const auto& k : keys) {
        if (k.roving) { has_roving = true; break; }
    }
    if (!has_roving) return false;

    bool changed = false;
    size_t a = 0;
    while (a < n - 1) {
        size_t b = a + 1;
        while (b < n && keys[b].roving) ++b;
        if (b >= n) break;
        if (b > a + 1) {
            std::vector<double> acc = {0.0};
            for (size_t i = a; i < b; ++i) {
                double len = 0.0;
                if (spatial && (keys[i].value.kind == ValueKind::Vec2 || keys[i].value.kind == ValueKind::Vec3)) {
                    len = spatial_segment_length(keys, i);
                } else {
                    auto x = keys[i].value.components();
                    auto y = keys[i + 1].value.components();
                    double sum_sq = 0.0;
                    size_t count = std::min(x.size(), y.size());
                    for (size_t k = 0; k < count; ++k) sum_sq += (y[k] - x[k]) * (y[k] - x[k]);
                    len = std::sqrt(sum_sq);
                }
                acc.push_back(acc.back() + len);
            }
            double total = acc.back();
            int64_t t0 = keys[a].time.v;
            int64_t t1 = keys[b].time.v;
            for (size_t o = 0; o < b - a - 1; ++o) {
                size_t i = a + 1 + o;
                double f = (total > 1e-9) ? acc[o + 1] / total : (double)(o + 1) / (double)(b - a);
                double span = (double)(t1 - t0);
                double retimed_d = (double)t0 + span * f;
                int64_t retimed = (int64_t)std::round(retimed_d);
                int64_t lo = keys[i - 1].time.v + 1;
                int64_t hi = t1 - (int64_t)(b - i);
                if (lo <= hi) retimed = std::clamp(retimed, lo, hi);
                Tick nt(retimed);
                if (nt != keys[i].time) {
                    keys[i].time = nt;
                    changed = true;
                }
                keys[i].in_interp = Interp::Linear;
                keys[i].out_interp = Interp::Linear;
            }
        }
        a = b;
    }
    return changed;
}

void time_reverse(std::vector<Keyframe>& keys) {
    if (keys.empty()) return;
    int64_t first = keys.front().time.v;
    int64_t last = keys.back().time.v;
    for (auto& k : keys) {
        int64_t reversed = first + last - k.time.v;
        k.time = Tick(reversed);
        std::swap(k.in_interp, k.out_interp);
        std::swap(k.in_ease, k.out_ease);
        std::swap(k.spatial_in, k.spatial_out);
    }
    std::reverse(keys.begin(), keys.end());
}

Value segment_value(const std::vector<Keyframe>& keys, size_t i, Tick t, bool spatial) {
    const auto& a = keys[i];
    const auto& b = keys[i + 1];
    if (a.out_interp == Interp::Hold || !a.value.interpolates()) {
        return a.value;
    }
    double t0 = secs(a.time);
    double t1 = secs(b.time);
    double dur = std::max(t1 - t0, 1e-12);
    double x = std::clamp((secs(t) - t0) / dur, 0.0, 1.0);
    bool lin_out = (a.out_interp == Interp::Linear);
    bool lin_in = (b.in_interp != Interp::Bezier);
    bool is_spatial = spatial && (a.value.kind == ValueKind::Vec2 || a.value.kind == ValueKind::Vec3);

    if (is_spatial) {
        auto p0 = a.value.as_vec3();
        auto p3 = b.value.as_vec3();
        auto tang_i = spatial_tangents(keys, i);
        auto tang_next = spatial_tangents(keys, i + 1);
        auto out_t = tang_i.second;
        auto in_t = tang_next.first;
        std::array<double, 3> pts[4] = {
            p0,
            {p0[0] + out_t[0], p0[1] + out_t[1], p0[2] + out_t[2]},
            {p3[0] + in_t[0],  p3[1] + in_t[1],  p3[2] + in_t[2]},
            p3
        };
        SpatialSeg seg(pts);
        double len = seg.length();
        double f = x;
        if (!(lin_out && lin_in || len <= 1e-12)) {
            Ease eo = lin_out ? Ease{len / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i, 0, true, true);
            Ease ei = lin_in ? Ease{len / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i + 1, 0, true, false);
            f = ease_progress(eo.speed * dur / len, eo.influence, ei.speed * dur / len, ei.influence, x);
        }
        std::array<double, 3> p;
        if (on_chord(p0, pts[1], p3) && on_chord(p0, pts[2], p3)) {
            p = {p0[0] + (p3[0] - p0[0]) * f, p0[1] + (p3[1] - p0[1]) * f, p0[2] + (p3[2] - p0[2]) * f};
        } else {
            p = seg.at_length(f * len);
        }
        return a.value.with_components({p[0], p[1], p[2]});
    }

    if (lin_out && lin_in) {
        return a.value.lerp(b.value, x);
    }

    auto ca = a.value.components();
    auto cb = b.value.components();
    if (ca.empty() || ca.size() != cb.size()) {
        Ease eo = lin_out ? Ease{1.0 / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i, 0, false, true);
        Ease ei = lin_in ? Ease{1.0 / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i + 1, 0, false, false);
        double f = ease_progress(eo.speed * dur, eo.influence, ei.speed * dur, ei.influence, x);
        return a.value.lerp(b.value, f);
    }

    std::vector<double> out(ca.size());
    for (size_t d = 0; d < ca.size(); ++d) {
        double dv = cb[d] - ca[d];
        Ease eo = lin_out ? Ease{dv / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i, d, false, true);
        Ease ei = lin_in ? Ease{dv / dur, LINEAR_SIDE_INFLUENCE} : effective_ease(keys, i + 1, d, false, false);
        double i0 = std::clamp(eo.influence, 0.0, 1.0);
        double i1 = std::clamp(ei.influence, 0.0, 1.0);
        double u = solve_u(0.0, i0, 1.0 - i1, 1.0, x);
        double v = bez(ca[d], ca[d] + eo.speed * i0 * dur, cb[d] - ei.speed * i1 * dur, cb[d], u);
        out[d] = v;
    }
    return a.value.with_components(out);
}

bool evaluate(const std::vector<Keyframe>& keys, Tick t, bool spatial, Value& out) {
    bool has_muted = false;
    for (const auto& k : keys) {
        if (k.muted) { has_muted = true; break; }
    }
    if (!has_muted) {
        if (keys.empty()) return false;
        if (keys.size() == 1 || t <= keys.front().time) {
            out = keys.front().value;
            return true;
        }
        if (t >= keys.back().time) {
            out = keys.back().value;
            return true;
        }
        auto it = std::upper_bound(keys.begin(), keys.end(), t, [](Tick val, const Keyframe& k) {
            return val < k.time;
        });
        size_t i = (size_t)std::distance(keys.begin(), it) - 1;
        out = segment_value(keys, i, t, spatial);
        return true;
    }
    std::vector<Keyframe> active;
    active.reserve(keys.size());
    for (const auto& k : keys) {
        if (!k.muted) active.push_back(k);
    }
    if (active.empty()) return false;
    return evaluate(active, t, spatial, out);
}

// =====================================================================
// 3D Orientation (crates/geom/src/lib.rs orientation_slerp & orientation_arc_deg)
// =====================================================================

static std::array<double, 4> orientation_quat(std::array<double, 3> o) {
    auto half = [](double d) -> std::pair<double, double> {
        double r = to_radians(d) * 0.5;
        return {std::sin(r), std::cos(r)};
    };
    auto hx = half(o[0]);
    auto hy = half(o[1]);
    auto hz = half(o[2]);
    double sx = hx.first,  cx = hx.second;
    double sy = hy.first,  cy = hy.second;
    double sz = hz.first,  cz = hz.second;
    double w = cx * cy, x = sx * cy, y = cx * sy, z = sx * sy;
    return {w * cz - z * sz, x * cz + y * sz, y * cz - x * sz, z * cz + w * sz};
}

double orientation_arc_deg(std::array<double, 3> a, std::array<double, 3> b) {
    auto p = orientation_quat(a);
    auto q = orientation_quat(b);
    double d = std::clamp(std::abs(p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3]), 0.0, 1.0);
    double arc = 2.0 * to_degrees(std::acos(d));
    return std::isfinite(arc) ? arc : 0.0;
}

std::array<double, 3> orientation_slerp(std::array<double, 3> a, std::array<double, 3> b, double t) {
    double bx = a[0] + (b[0] - a[0]) * t;
    double by = a[1] + (b[1] - a[1]) * t;
    double bz = a[2] + (b[2] - a[2]) * t;
    if (!std::isfinite(t) || !std::isfinite(bx) || !std::isfinite(by) || !std::isfinite(bz)) {
        return a;
    }
    auto p = orientation_quat(a);
    auto q = orientation_quat(b);
    double d = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];
    if (d < 0.0) {
        for (int i = 0; i < 4; ++i) q[i] = -q[i];
        d = -d;
    }
    d = std::clamp(d, -1.0, 1.0);
    double ka, kb;
    if (d > 1.0 - 1e-12) {
        ka = 1.0 - t;
        kb = t;
    } else {
        double om = std::acos(d);
        double s = std::sin(om);
        ka = std::sin((1.0 - t) * om) / s;
        kb = std::sin(t * om) / s;
    }
    std::array<double, 4> r;
    for (int i = 0; i < 4; ++i) r[i] = ka * p[i] + kb * q[i];
    double n = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
    if (!(n > 1e-12 && std::isfinite(n))) return a;
    for (int i = 0; i < 4; ++i) r[i] /= n;

    // Quat to matrix and orientation_angles
    double rw = r[0], rx = r[1], ry = r[2], rz = r[3];
    double m00 = 1.0 - 2.0 * (ry * ry + rz * rz);
    double m01 = 2.0 * (rx * ry - rz * rw);
    double m02 = 2.0 * (rx * rz + ry * rw);
    double m11 = 1.0 - 2.0 * (rx * rx + rz * rz);
    double m12 = 2.0 * (ry * rz - rx * rw);
    double m21 = 2.0 * (ry * rz + rx * rw);
    double m22 = 1.0 - 2.0 * (rx * rx + ry * ry);

    double sy = std::clamp(m02, -1.0, 1.0);
    double cy = std::hypot(m00, m01);
    double y = std::atan2(sy, cy);
    double x = 0.0, z = 0.0;
    if (cy > 1e-9) {
        x = std::atan2(-m12, m22);
        z = std::atan2(-m01, m00);
    } else {
        x = std::atan2(m21, m11);
        z = 0.0;
    }
    auto wrap = [](double v) -> double {
        double w = std::fmod(v, 360.0);
        if (w < 0.0) w += 360.0;
        if (w >= 360.0 - 1e-9 || std::abs(w) < 1e-9) return 0.0;
        return w;
    };
    return {wrap(to_degrees(x)), wrap(to_degrees(y)), wrap(to_degrees(z))};
}

bool evaluate_orientation(const std::vector<Keyframe>& keys, Tick t, Value& out) {
    if (keys.empty()) return false;
    if (keys.size() == 1 || t <= keys.front().time) {
        out = keys.front().value;
        return true;
    }
    if (t >= keys.back().time) {
        out = keys.back().value;
        return true;
    }
    auto it = std::upper_bound(keys.begin(), keys.end(), t, [](Tick val, const Keyframe& k) {
        return val < k.time;
    });
    size_t i = (size_t)std::distance(keys.begin(), it) - 1;
    const auto& a = keys[i];
    const auto& b = keys[i + 1];
    if (a.value.kind != ValueKind::Vec3 || b.value.kind != ValueKind::Vec3) {
        out = segment_value(keys, i, t, false);
        return true;
    }
    if (a.out_interp == Interp::Hold) {
        out = a.value;
        return true;
    }
    auto va = a.value.as_vec3();
    auto vb = b.value.as_vec3();
    double t0 = secs(a.time);
    double t1 = secs(b.time);
    double dur = std::max(t1 - t0, 1e-12);
    double x = std::clamp((secs(t) - t0) / dur, 0.0, 1.0);
    if (x <= 0.0) {
        out = a.value;
        return true;
    }
    double arc = orientation_arc_deg(va, vb);
    bool lin_out = (a.out_interp == Interp::Linear);
    bool lin_in = (b.in_interp != Interp::Bezier);
    double f = x;
    if (!(lin_out && lin_in || arc <= 1e-9)) {
        auto side = [&](size_t j, bool is_out) -> Ease {
            if (j >= keys.size()) return Ease{arc / dur, LINEAR_SIDE_INFLUENCE};
            const auto& k = keys[j];
            bool linear = is_out ? lin_out : lin_in;
            if (linear) return Ease{arc / dur, LINEAR_SIDE_INFLUENCE};
            if (k.auto_bezier && j > 0 && j + 1 < keys.size()) {
                double span = std::max(secs(keys[j + 1].time) - secs(keys[j - 1].time), 1e-12);
                if (keys[j - 1].value.kind == ValueKind::Vec3 && keys[j + 1].value.kind == ValueKind::Vec3) {
                    return Ease{orientation_arc_deg(keys[j - 1].value.as_vec3(), keys[j + 1].value.as_vec3()) / span, 1.0 / 6.0};
                }
            }
            const auto& list = is_out ? k.out_ease : k.in_ease;
            return list.empty() ? Ease{} : list[0];
        };
        Ease eo = side(i, true);
        Ease ei = side(i + 1, false);
        f = ease_progress(eo.speed * dur / arc, eo.influence, ei.speed * dur / arc, ei.influence, x);
    }
    if (!std::isfinite(f)) f = x;
    out = Value::Vec3(orientation_slerp(va, vb, f));
    return true;
}

std::vector<double> velocity(const std::vector<Keyframe>& keys, Tick t, bool spatial) {
    Tick h(TICKS_PER_SECOND / 1000);
    Value a, b;
    if (!evaluate(keys, t - h, spatial, a) || !evaluate(keys, t + h, spatial, b)) {
        return {};
    }
    double dt = 2.0 * secs(h);
    auto ca = a.components();
    auto cb = b.components();
    size_t count = std::min(ca.size(), cb.size());
    std::vector<double> vel(count);
    for (size_t i = 0; i < count; ++i) {
        vel[i] = (cb[i] - ca[i]) / dt;
    }
    return vel;
}

double speed(const std::vector<Keyframe>& keys, Tick t, bool spatial) {
    auto vel = velocity(keys, t, spatial);
    double sum_sq = 0.0;
    for (double v : vel) sum_sq += v * v;
    return std::sqrt(sum_sq);
}

size_t set_key(std::vector<Keyframe>& keys, Keyframe key) {
    auto it = std::lower_bound(keys.begin(), keys.end(), key.time, [](const Keyframe& k, Tick val) {
        return k.time < val;
    });
    if (it != keys.end() && it->time == key.time) {
        key.in_interp = it->in_interp;
        key.out_interp = it->out_interp;
        key.in_ease = it->in_ease;
        key.out_ease = it->out_ease;
        key.auto_bezier = it->auto_bezier;
        key.continuous = it->continuous;
        key.spatial_in = it->spatial_in;
        key.spatial_out = it->spatial_out;
        key.spatial_auto = it->spatial_auto;
        key.spatial_continuous = it->spatial_continuous;
        key.roving = it->roving;
        key.label = it->label;
        key.muted = it->muted;
        *it = key;
        return (size_t)std::distance(keys.begin(), it);
    }
    size_t idx = (size_t)std::distance(keys.begin(), it);
    keys.insert(it, key);
    return idx;
}

int64_t key_at(const std::vector<Keyframe>& keys, Tick t) {
    auto it = std::lower_bound(keys.begin(), keys.end(), t, [](const Keyframe& k, Tick val) {
        return k.time < val;
    });
    if (it != keys.end() && it->time == t) {
        return (int64_t)std::distance(keys.begin(), it);
    }
    return -1;
}

void easy_ease(Keyframe& k, bool ease_in, bool ease_out) {
    size_t n = k.value.dims() > 0 ? k.value.dims() : 1;
    if (ease_in) {
        k.in_interp = Interp::Bezier;
        k.in_ease.assign(n, Ease::EASY);
    }
    if (ease_out) {
        k.out_interp = Interp::Bezier;
        k.out_ease.assign(n, Ease::EASY);
    }
    k.auto_bezier = false;
}

} // namespace KeyframeModel
