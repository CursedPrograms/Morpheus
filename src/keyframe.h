// Morpheus - keyframe and property values.
// Ported function-by-function from EffectCraft crates/keyframe and crates/geom.
//
// Covers:
//   - Value (Scalar, Vec2, Vec3, Color, Bool, Enum, Path, Str, Layer, Gradient)
//   - ShapePath with Bezier vertices, tangents, and feather points
//   - Gradient with colour stops and opacity stops
//   - Keyframe with temporal easing, spatial tangents, auto-Bezier, roving
//   - evaluate, evaluate_orientation, segment_value, spatial arc-length table
//   - orientation_slerp and orientation_arc_deg from crates/geom
//
// after EffectCraft (crates/keyframe/src/value.rs, crates/keyframe/src/lib.rs,
//                    crates/geom/src/lib.rs)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "time_.h"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <memory>
#include <optional>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace KeyframeModel {

using Project::Tick;

// ---- FeatherPoint ----------------------------------------------------
// after EffectCraft crates/keyframe/src/value.rs FeatherPoint
struct FeatherPoint {
    size_t segment = 0;
    double t       = 0.0;
    double radius  = 0.0;
    double tension = 0.0;

    bool operator==(const FeatherPoint& o) const {
        return segment == o.segment && t == o.t && radius == o.radius && tension == o.tension;
    }
};

// ---- ShapePath -------------------------------------------------------
// after EffectCraft crates/keyframe/src/value.rs ShapePath
struct ShapePath {
    std::vector<std::array<double, 2>> vertices;
    std::vector<std::array<double, 2>> in_tangents;
    std::vector<std::array<double, 2>> out_tangents;
    bool closed = false;
    std::vector<FeatherPoint> feather;

    static ShapePath polygon(const std::vector<std::array<double, 2>>& pts, bool closed);
    static ShapePath rect(std::array<double, 2> c, double w, double h);
    static ShapePath ellipse(std::array<double, 2> c, double w, double h);

    size_t len() const { return vertices.size(); }
    bool is_empty() const { return vertices.empty(); }
    size_t segment_count() const {
        if (closed) return vertices.size();
        return vertices.empty() ? 0 : vertices.size() - 1;
    }
    bool fit_tangents();
    void feather_after_insert(size_t i, const double* split);
    void feather_after_remove(size_t i);
    ShapePath lerp(const ShapePath& o, double t) const;

    bool operator==(const ShapePath& o) const;
    bool operator!=(const ShapePath& o) const { return !(*this == o); }
};

// ---- Gradient --------------------------------------------------------
// after EffectCraft crates/keyframe/src/value.rs Gradient
static constexpr size_t MAX_GRADIENT_STOPS = 256;

struct Gradient {
    // (position 0..1, [r, g, b, a])
    std::vector<std::pair<double, std::array<float, 4>>> colors;
    // (position 0..1, alpha)
    std::vector<std::pair<double, float>> opacities;

    static Gradient default_gradient();
    std::array<float, 4> sample(double t) const;
    std::array<float, 4> color_at(double t) const;
    float opacity_at(double t) const;

    bool operator==(const Gradient& o) const;
    bool operator!=(const Gradient& o) const { return !(*this == o); }
};

// ---- Value -----------------------------------------------------------
// after EffectCraft crates/keyframe/src/value.rs Value
enum class ValueKind {
    Scalar,
    Vec2,
    Vec3,
    Color,
    Bool,
    Enum,
    Path,
    Str,
    Layer,
    Gradient
};

struct Value {
    ValueKind kind = ValueKind::Scalar;
    double scalar = 0.0;
    std::array<double, 2> v2 = {0.0, 0.0};
    std::array<double, 3> v3 = {0.0, 0.0, 0.0};
    std::array<double, 4> color = {0.0, 0.0, 0.0, 1.0};
    bool b = false;
    uint32_t e = 0;
    ShapePath path;
    std::string str;
    uint64_t layer = 0; // 0 = none
    Gradient grad;

    Value() : kind(ValueKind::Scalar), scalar(0.0) {}
    static Value Scalar(double v) {
        Value val; val.kind = ValueKind::Scalar; val.scalar = v; return val;
    }
    static Value Vec2(std::array<double, 2> v) {
        Value val; val.kind = ValueKind::Vec2; val.v2 = v; return val;
    }
    static Value Vec2(double x, double y) {
        return Vec2({x, y});
    }
    static Value Vec3(std::array<double, 3> v) {
        Value val; val.kind = ValueKind::Vec3; val.v3 = v; return val;
    }
    static Value Vec3(double x, double y, double z) {
        return Vec3({x, y, z});
    }
    static Value Color(std::array<double, 4> v) {
        Value val; val.kind = ValueKind::Color; val.color = v; return val;
    }
    static Value Color(double r, double g, double b, double a = 1.0) {
        return Color({r, g, b, a});
    }
    static Value Bool(bool v) {
        Value val; val.kind = ValueKind::Bool; val.b = v; return val;
    }
    static Value Enum(uint32_t v) {
        Value val; val.kind = ValueKind::Enum; val.e = v; return val;
    }
    static Value Path(const ShapePath& p) {
        Value val; val.kind = ValueKind::Path; val.path = p; return val;
    }
    static Value Str(const std::string& s) {
        Value val; val.kind = ValueKind::Str; val.str = s; return val;
    }
    static Value Layer(uint64_t l) {
        Value val; val.kind = ValueKind::Layer; val.layer = l; return val;
    }
    static Value Grad(const Gradient& g) {
        Value val; val.kind = ValueKind::Gradient; val.grad = g; return val;
    }

    std::vector<double> components() const;
    size_t dims() const;
    Value with_components(const std::vector<double>& c) const;
    bool interpolates() const;
    Value lerp(const Value& o, double t) const;

    double as_f64() const;
    std::array<double, 2> as_vec2() const;
    std::array<double, 3> as_vec3() const;
    std::array<float, 4> as_color() const;
    bool as_bool() const;
    uint32_t as_enum() const;
    const ShapePath* as_path() const { return kind == ValueKind::Path ? &path : nullptr; }
    uint64_t as_layer() const { return kind == ValueKind::Layer ? layer : 0; }
    const char* kind_name() const;

    bool operator==(const Value& o) const;
    bool operator!=(const Value& o) const { return !(*this == o); }
};

// ---- Interp / Ease / Keyframe ----------------------------------------
// after EffectCraft crates/keyframe/src/lib.rs Interp, Ease, Keyframe

enum class Interp {
    Linear,
    Bezier,
    Hold
};

static inline const char* interp_label(Interp i) {
    switch (i) {
    case Interp::Linear: return "Linear";
    case Interp::Bezier: return "Bezier";
    case Interp::Hold:   return "Hold";
    }
    return "Linear";
}

struct Ease {
    double speed     = 0.0;
    double influence = 1.0 / 6.0;

    static const Ease EASY;
    bool operator==(const Ease& o) const {
        return speed == o.speed && influence == o.influence;
    }
    bool operator!=(const Ease& o) const {
        return !(*this == o);
    }
};

static constexpr double LINEAR_SIDE_INFLUENCE = 1.0 / 6.0;

struct Keyframe {
    Tick time;
    Value value;
    Interp in_interp  = Interp::Linear;
    Interp out_interp = Interp::Linear;
    std::vector<Ease> in_ease;
    std::vector<Ease> out_ease;
    bool continuous   = false;
    bool auto_bezier  = false;
    std::array<double, 3> spatial_in  = {0.0, 0.0, 0.0};
    std::array<double, 3> spatial_out = {0.0, 0.0, 0.0};
    bool spatial_auto       = true;
    bool spatial_continuous = false;
    bool roving             = false;
    uint8_t label           = 0;
    bool muted              = false;

    Keyframe() = default;
    Keyframe(Tick t, Value v) : time(t), value(std::move(v)) {}

    Keyframe hold() const {
        Keyframe k = *this;
        k.in_interp = Interp::Hold;
        k.out_interp = Interp::Hold;
        return k;
    }

    Keyframe eased() const {
        Keyframe k = *this;
        k.in_interp = Interp::Bezier;
        k.out_interp = Interp::Bezier;
        size_t n = k.value.dims() > 0 ? k.value.dims() : 1;
        k.in_ease.assign(n, Ease::EASY);
        k.out_ease.assign(n, Ease::EASY);
        return k;
    }

    bool operator==(const Keyframe& o) const;
    bool operator!=(const Keyframe& o) const { return !(*this == o); }
};

// ---- Keyframe evaluation and manipulation functions -------------------
// after EffectCraft crates/keyframe/src/lib.rs

double ease_progress(double out_speed_n, double out_inf, double in_speed_n, double in_inf, double x);

std::pair<std::array<double, 3>, std::array<double, 3>> spatial_tangents(const std::vector<Keyframe>& keys, size_t i);
std::pair<std::array<double, 3>, std::array<double, 3>> spatial_handles(const std::vector<Keyframe>& keys, size_t i);
double spatial_segment_length(const std::vector<Keyframe>& keys, size_t i);
std::optional<Ease> side_ease(const std::vector<Keyframe>& keys, size_t i, size_t d, bool spatial, bool out);

bool retime_roving(std::vector<Keyframe>& keys, bool spatial);
void time_reverse(std::vector<Keyframe>& keys);

bool evaluate(const std::vector<Keyframe>& keys, Tick t, bool spatial, Value& out);
bool evaluate_orientation(const std::vector<Keyframe>& keys, Tick t, Value& out);

inline std::optional<Value> evaluate(const std::vector<Keyframe>& keys, Tick t, bool spatial) {
    Value out;
    if (evaluate(keys, t, spatial, out)) return out;
    return std::nullopt;
}

inline std::optional<Value> evaluate_orientation(const std::vector<Keyframe>& keys, Tick t) {
    Value out;
    if (evaluate_orientation(keys, t, out)) return out;
    return std::nullopt;
}
Value segment_value(const std::vector<Keyframe>& keys, size_t i, Tick t, bool spatial);

std::vector<double> velocity(const std::vector<Keyframe>& keys, Tick t, bool spatial);
double speed(const std::vector<Keyframe>& keys, Tick t, bool spatial);

size_t set_key(std::vector<Keyframe>& keys, Keyframe key);
int64_t key_at(const std::vector<Keyframe>& keys, Tick t); // -1 if not found
void easy_ease(Keyframe& k, bool ease_in, bool ease_out);

// Geom 3D orientation helpers (after EffectCraft crates/geom/src/lib.rs)
std::array<double, 3> orientation_slerp(std::array<double, 3> a, std::array<double, 3> b, double t);
double orientation_arc_deg(std::array<double, 3> a, std::array<double, 3> b);

} // namespace KeyframeModel
