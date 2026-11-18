// Morpheus - exact media time implementation.
// Ported function-by-function from crates/time/src/lib.rs in EffectCraft.
//
// All time is an integer number of Ticks at TICKS_PER_SECOND = 254016000000/s.
// That rate divides evenly into every broadcast frame duration and common audio
// sample rate, so edits and frame maths never drift.
// Timecode, feet+frames, frames, samples are display-only conversions of Ticks.
//
// after EffectCraft (crates/time/src/lib.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "time_.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <intrin.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Project {

// =====================================================================
// 128-bit integer support (exact i128 / u128 for MSVC and C++17)
// after EffectCraft (uses Rust native i128)
// =====================================================================

struct I128 {
    int64_t  hi;
    uint64_t lo;

    static constexpr I128 zero()    { return { 0, 0 }; }
    static constexpr I128 from64(int64_t v) { return { v < 0 ? -1LL : 0LL, (uint64_t)v }; }
    static constexpr I128 max_val() { return { INT64_MAX, UINT64_MAX }; }
    static constexpr I128 min_val() { return { INT64_MIN, 0ULL }; }

    bool negative() const { return hi < 0; }
    bool is_zero()  const { return hi == 0 && lo == 0; }

    I128 neg() const {
        if (hi == INT64_MIN && lo == 0) return max_val();
        uint64_t nlo = ~lo + 1;
        int64_t  nhi = ~hi + (nlo == 0 ? 1 : 0);
        return { nhi, nlo };
    }

    bool operator==(const I128& o) const { return hi == o.hi && lo == o.lo; }
    bool operator!=(const I128& o) const { return hi != o.hi || lo != o.lo; }
    bool operator< (const I128& o) const {
        if (hi != o.hi) return hi < o.hi;
        return lo < o.lo;
    }
    bool operator> (const I128& o) const { return o < *this; }
    bool operator<=(const I128& o) const { return !(o < *this); }
    bool operator>=(const I128& o) const { return !(*this < o); }

    int64_t clamp64() const {
        if (hi > 0 || (hi == 0 && lo > (uint64_t)INT64_MAX)) return INT64_MAX;
        if (hi < -1 || (hi == -1 && lo < (uint64_t)INT64_MIN)) return INT64_MIN;
        return (int64_t)lo;
    }
};

static inline I128 add128(I128 a, I128 b) {
    uint64_t lo = a.lo + b.lo;
    int64_t carry = (lo < a.lo) ? 1LL : 0LL;
    int64_t hi = a.hi + b.hi + carry;
    return { hi, lo };
}

static inline I128 sub128(I128 a, I128 b) {
    return add128(a, b.neg());
}

static inline I128 sat_add128(I128 a, I128 b) {
    bool a_pos = a.hi > 0 || (a.hi == 0 && a.lo > 0);
    bool b_pos = b.hi > 0 || (b.hi == 0 && b.lo > 0);
    bool a_neg = a.hi < 0;
    bool b_neg = b.hi < 0;
    I128 sum = add128(a, b);
    if (a_pos && b_pos && sum.hi < 0) return I128::max_val();
    if (a_neg && b_neg && sum.hi >= 0) return I128::min_val();
    return sum;
}

static inline I128 mul128(int64_t a, int64_t b) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    int64_t hi;
    uint64_t lo = (uint64_t)_mul128(a, b, &hi);
    return { hi, lo };
#else
    bool neg = (a < 0) ^ (b < 0);
    uint64_t ua = (a == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(a < 0 ? -a : a);
    uint64_t ub = (b == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(b < 0 ? -b : b);
    uint64_t a_hi = ua >> 32, a_lo = ua & 0xFFFFFFFFULL;
    uint64_t b_hi = ub >> 32, b_lo = ub & 0xFFFFFFFFULL;
    uint64_t lo = a_lo * b_lo;
    uint64_t m1 = a_hi * b_lo;
    uint64_t m2 = a_lo * b_hi;
    uint64_t hi = a_hi * b_hi;
    uint64_t m = m1 + m2;
    if (m < m1) hi += (1ULL << 32);
    hi += (m >> 32);
    lo += (m << 32);
    if (lo < (m << 32)) hi += 1;
    I128 res = { (int64_t)hi, lo };
    return neg ? res.neg() : res;
#endif
}

static inline I128 mul128_64(I128 a, int64_t b) {
    if (b == 0 || (a.hi == 0 && a.lo == 0)) return I128::zero();
    if (b == 1) return a;
    if (b == -1) return a.neg();

    bool neg = (a.hi < 0) ^ (b < 0);
    I128 abs_a = a.negative() ? a.neg() : a;
    uint64_t ub = (b == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(b < 0 ? -b : b);

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    uint64_t h0, h1;
    uint64_t l0 = _umul128(abs_a.lo, ub, &h0);
    uint64_t l1 = _umul128((uint64_t)abs_a.hi, ub, &h1);
#else
    auto mul64to128 = [](uint64_t u, uint64_t v, uint64_t& hi_out) -> uint64_t {
        uint64_t u1 = u >> 32, u0 = u & 0xFFFFFFFFULL;
        uint64_t v1 = v >> 32, v0 = v & 0xFFFFFFFFULL;
        uint64_t w0 = u0 * v0;
        uint64_t t = u1 * v0 + (w0 >> 32);
        uint64_t w1 = t & 0xFFFFFFFFULL;
        uint64_t w2 = t >> 32;
        w1 += u0 * v1;
        hi_out = u1 * v1 + w2 + (w1 >> 32);
        return (w1 << 32) + (w0 & 0xFFFFFFFFULL);
    };
    uint64_t h0 = 0, h1 = 0;
    uint64_t l0 = mul64to128(abs_a.lo, ub, h0);
    uint64_t l1 = mul64to128((uint64_t)abs_a.hi, ub, h1);
#endif
    if (h1 != 0) return neg ? I128::min_val() : I128::max_val();
    uint64_t mid = l1 + h0;
    if (mid < l1) return neg ? I128::min_val() : I128::max_val();
    if (mid > (uint64_t)INT64_MAX) {
        if (neg && mid == 0x8000000000000000ULL && l0 == 0) return I128::min_val();
        return neg ? I128::min_val() : I128::max_val();
    }
    I128 res = { (int64_t)mid, l0 };
    return neg ? res.neg() : res;
}

static inline void udivrem128(uint64_t a_hi, uint64_t a_lo, uint64_t b_hi, uint64_t b_lo,
                              uint64_t& q_hi, uint64_t& q_lo, uint64_t& r_hi, uint64_t& r_lo) {
    if (b_hi == 0 && b_lo == 0) {
        q_hi = q_lo = r_hi = r_lo = 0;
        return;
    }
    q_hi = q_lo = r_hi = r_lo = 0;
    for (int i = 127; i >= 0; --i) {
        r_hi = (r_hi << 1) | (r_lo >> 63);
        r_lo = (r_lo << 1);
        uint64_t bit = (i >= 64) ? ((a_hi >> (i - 64)) & 1) : ((a_lo >> i) & 1);
        r_lo |= bit;
        if (r_hi > b_hi || (r_hi == b_hi && r_lo >= b_lo)) {
            uint64_t borrow = (r_lo < b_lo) ? 1 : 0;
            r_lo -= b_lo;
            r_hi = r_hi - b_hi - borrow;
            if (i >= 64) q_hi |= (1ULL << (i - 64));
            else         q_lo |= (1ULL << i);
        }
    }
}

// after EffectCraft div_euclid and rem_euclid (exact 128-bit)
static inline void div_euclid128(I128 a, I128 b, I128& q, I128& r) {
    if (b.is_zero()) {
        q = I128::zero();
        r = I128::zero();
        return;
    }
    if (a == I128::min_val() && b == I128::from64(-1)) {
        q = I128::max_val();
        r = I128::zero();
        return;
    }

    bool a_neg = a.negative();
    bool b_neg = b.negative();
    I128 ua = a_neg ? a.neg() : a;
    I128 ub = b_neg ? b.neg() : b;

    uint64_t q_hi, q_lo, r_hi, r_lo;
    udivrem128((uint64_t)ua.hi, ua.lo, (uint64_t)ub.hi, ub.lo, q_hi, q_lo, r_hi, r_lo);

    I128 tq = { (int64_t)q_hi, q_lo };
    I128 tr = { (int64_t)r_hi, r_lo };

    if (a_neg != b_neg) tq = tq.neg();
    if (a_neg) tr = tr.neg();

    if (tr.negative()) {
        if (b > I128::zero()) {
            tq = sub128(tq, I128::from64(1));
            tr = add128(tr, b);
        } else {
            tq = add128(tq, I128::from64(1));
            tr = sub128(tr, b);
        }
    }
    q = tq;
    r = tr;
}

// after EffectCraft nonzero
static inline int64_t nonzero(int64_t v) { return v == 0 ? 1 : v; }

// after EffectCraft clamp128
static inline int64_t clamp128(I128 v) { return v.clamp64(); }

// after EffectCraft gcd
static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b) { uint64_t t = b; b = a % b; a = t; }
    return a;
}

static inline int64_t sat_add(int64_t a, int64_t b) {
    if (b > 0 && a > INT64_MAX - b) return INT64_MAX;
    if (b < 0 && a < INT64_MIN - b) return INT64_MIN;
    return a + b;
}

static inline int64_t sat_sub(int64_t a, int64_t b) {
    if (b < 0 && a > INT64_MAX + b) return INT64_MAX;
    if (b > 0 && a < INT64_MIN + b) return INT64_MIN;
    return a - b;
}

// Drop-frame params: (frames_to_drop_per_minute, timecode_base).
// after EffectCraft crates/time/src/lib.rs df_params
static void df_params(FrameRate rate, int64_t& drop, int64_t& base) {
    base = rate.timecode_base();
    drop = base / 15; // 30->2, 60->4, 120->8
}

// =====================================================================
// Tick constants
// =====================================================================
const Tick Tick::ZERO  { 0 };
const Tick Tick::MAX_T { INT64_MAX / 4 };
const Tick Tick::MIN_T { INT64_MIN / 4 };

// =====================================================================
// Tick implementation  (after EffectCraft crates/time/src/lib.rs Tick)
// =====================================================================

// after EffectCraft Tick::from_seconds_f64
Tick Tick::from_seconds_f64(double s) {
    if (std::isnan(s)) return Tick::ZERO;
    double t = std::round(s * (double)TICKS_PER_SECOND);
    int64_t i;
    if (t >= (double)INT64_MAX)       i = INT64_MAX;
    else if (t <= (double)INT64_MIN)  i = INT64_MIN;
    else                              i = (int64_t)t;
    if (i > Tick::MAX_T.v) i = Tick::MAX_T.v;
    if (i < Tick::MIN_T.v) i = Tick::MIN_T.v;
    return Tick(i);
}

// after EffectCraft Tick::seconds
double Tick::seconds() const {
    return (double)v / (double)TICKS_PER_SECOND;
}

// after EffectCraft Tick::from_units
Tick Tick::from_units(int64_t units, int64_t per_second) {
    I128 num = mul128(units, TICKS_PER_SECOND);
    I128 q, r;
    div_euclid128(num, I128::from64(nonzero(per_second)), q, r);
    return Tick(clamp128(q));
}

// after EffectCraft Tick::to_units_floor
int64_t Tick::to_units_floor(int64_t per_second) const {
    I128 num = mul128(v, per_second);
    I128 q, r;
    div_euclid128(num, I128::from64(TICKS_PER_SECOND), q, r);
    return clamp128(q);
}

// after EffectCraft Tick::from_rational
Tick Tick::from_rational(int64_t pts, int64_t num, int64_t den) {
    I128 p1 = mul128(pts, num);
    I128 p2 = mul128_64(p1, TICKS_PER_SECOND);
    I128 q, r;
    div_euclid128(p2, I128::from64(nonzero(den)), q, r);
    return Tick(clamp128(q));
}

// after EffectCraft Tick::to_rational_floor
int64_t Tick::to_rational_floor(int64_t num, int64_t den) const {
    I128 numer = mul128(v, den);
    I128 denom = mul128(nonzero(num), TICKS_PER_SECOND);
    I128 q, r;
    div_euclid128(numer, denom, q, r);
    return clamp128(q);
}

// after EffectCraft Tick::abs
Tick Tick::abs() const {
    if (v == INT64_MIN) return Tick(INT64_MAX);
    return Tick(v < 0 ? -v : v);
}

Tick Tick::min_t(Tick o) const { return Tick(v < o.v ? v : o.v); }
Tick Tick::max_t(Tick o) const { return Tick(v > o.v ? v : o.v); }
Tick Tick::clamp_t(Tick lo, Tick hi) const {
    return Tick(v < lo.v ? lo.v : (v > hi.v ? hi.v : v));
}

// after EffectCraft Tick::mul_ratio
Tick Tick::mul_ratio(int64_t num, int64_t den) const {
    I128 p = mul128(v, num);
    I128 q, r;
    div_euclid128(p, I128::from64(nonzero(den)), q, r);
    return Tick(clamp128(q));
}

// Saturating add/sub/neg (after EffectCraft Tick ops)
Tick Tick::operator+(Tick o) const {
    return Tick(sat_add(v, o.v));
}
Tick Tick::operator-(Tick o) const {
    return Tick(sat_sub(v, o.v));
}
Tick Tick::operator-() const {
    if (v == INT64_MIN) return Tick(INT64_MAX);
    return Tick(-v);
}
Tick& Tick::operator+=(Tick o) { *this = *this + o; return *this; }
Tick& Tick::operator-=(Tick o) { *this = *this - o; return *this; }

// =====================================================================
// TimeRange  (after EffectCraft crates/time/src/lib.rs TimeRange)
// =====================================================================
TimeRange TimeRange::from_bounds(Tick start, Tick end) {
    return TimeRange(start, end - start);
}
Tick TimeRange::end() const { return start + duration; }
bool TimeRange::contains(Tick t) const { return t >= start && t < end(); }
bool TimeRange::overlaps(const TimeRange& o) const { return start < o.end() && o.start < end(); }
bool TimeRange::intersect(const TimeRange& o, TimeRange& out) const {
    Tick s = start.max_t(o.start);
    Tick e = end().min_t(o.end());
    if (e > s) {
        out = TimeRange::from_bounds(s, e);
        return true;
    }
    return false;
}
bool TimeRange::is_empty() const { return duration.v <= 0; }

// =====================================================================
// FrameRate constants  (after EffectCraft crates/time/src/lib.rs)
// =====================================================================
const FrameRate FrameRate::FPS_23_976 { 24000, 1001 };
const FrameRate FrameRate::FPS_24     {    24,    1 };
const FrameRate FrameRate::FPS_25     {    25,    1 };
const FrameRate FrameRate::FPS_29_97  { 30000, 1001 };
const FrameRate FrameRate::FPS_30     {    30,    1 };
const FrameRate FrameRate::FPS_48     {    48,    1 };
const FrameRate FrameRate::FPS_50     {    50,    1 };
const FrameRate FrameRate::FPS_59_94  { 60000, 1001 };
const FrameRate FrameRate::FPS_60     {    60,    1 };
const FrameRate FrameRate::FPS_119_88 { 120000, 1001 };
const FrameRate FrameRate::FPS_120    {   120,    1 };

// after EffectCraft FrameRate::new
FrameRate FrameRate::make(int64_t n, int64_t d) {
    uint64_t an = (n == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(n < 0 ? -n : n);
    uint64_t ad = (d == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(d < 0 ? -d : d);
    uint64_t g  = gcd(an, ad);
    if (g < 1) g = 1;
    int64_t qn = (n < 0) ? -(int64_t)(an / g) : (int64_t)(an / g);
    int64_t qd = (d < 0) ? -(int64_t)(ad / g) : (int64_t)(ad / g);
    return FrameRate { qn, qd };
}

// after EffectCraft FrameRate::from_f64
FrameRate FrameRate::from_f64(double fps) {
    if (std::isnan(fps)) return FrameRate{0, 1};
    static const FrameRate COMMON[] = {
        {24000,1001},{24,1},{25,1},{30000,1001},{30,1},
        {48,1},{50,1},{60000,1001},{60,1},{120000,1001},{120,1}
    };
    for (const auto& r : COMMON) {
        if (std::fabs(r.as_f64() - fps) < 0.005) return r;
    }
    double n = std::round(fps * 1000.0);
    int64_t ni;
    if (n >= (double)INT64_MAX)       ni = INT64_MAX;
    else if (n <= (double)INT64_MIN)  ni = INT64_MIN;
    else                              ni = (int64_t)n;
    if (fps > 0.0 && ni < 1) ni = 1;
    return make(ni, 1000);
}

double FrameRate::as_f64() const {
    return (double)num / (double)den;
}

// after EffectCraft FrameRate::frame_duration
Tick FrameRate::frame_duration() const {
    I128 numer = mul128(TICKS_PER_SECOND, den);
    I128 q, r;
    div_euclid128(numer, I128::from64(nonzero(num)), q, r);
    return Tick(clamp128(q));
}

// after EffectCraft FrameRate::frame_at (floor division)
int64_t FrameRate::frame_at(Tick t) const {
    I128 numer = mul128(t.v, num);
    I128 denom = mul128(TICKS_PER_SECOND, nonzero(den));
    I128 q, r;
    div_euclid128(numer, denom, q, r);
    return clamp128(q);
}

// after EffectCraft FrameRate::tick_of (ceiling at fractional tick boundary)
Tick FrameRate::tick_of(int64_t f) const {
    I128 n1 = mul128(f, TICKS_PER_SECOND);
    I128 n  = mul128_64(n1, den);
    int64_t d = nonzero(num);
    I128 q, r;
    div_euclid128(n, I128::from64(d), q, r);
    if (d > 0 && !r.is_zero()) {
        q = sat_add128(q, I128::from64(1));
    }
    return Tick(clamp128(q));
}

// after EffectCraft FrameRate::snap
Tick FrameRate::snap(Tick t) const {
    return tick_of(frame_at(t));
}

// after EffectCraft FrameRate::snap_nearest
Tick FrameRate::snap_nearest(Tick t) const {
    Tick a = snap(t);
    Tick b = tick_of(sat_add(frame_at(t), 1));
    return ((t - a) <= (b - t)) ? a : b;
}

// after EffectCraft FrameRate::timecode_base
int64_t FrameRate::timecode_base() const {
    if (num <= 0 || den <= 0) return 1;
    I128 v = add128(add128(I128::from64(num), I128::from64(den)), I128::from64(-1));
    I128 q, r;
    div_euclid128(v, I128::from64(den), q, r);
    int64_t res = clamp128(q);
    return (res < 1) ? 1 : res;
}

// after EffectCraft FrameRate::is_ntsc
bool FrameRate::is_ntsc() const { return den == 1001; }

// after EffectCraft FrameRate::supports_drop_frame
bool FrameRate::supports_drop_frame() const {
    return is_ntsc() && (timecode_base() % 30 == 0);
}

// after EffectCraft FrameRate::label
std::string FrameRate::label() const {
    if (den == 1) {
        char buf[32]; snprintf(buf, sizeof buf, "%lld", (long long)num);
        return buf;
    }
    double v = as_f64();
    char buf[64]; snprintf(buf, sizeof buf, "%.3f", v);
    char* p = buf + strlen(buf) - 1;
    while (p > buf && *p == '0') *p-- = '\0';
    if (p > buf && *p == '.') *p = '\0';
    return buf;
}

// =====================================================================
// Timecode & Display formatting
// after EffectCraft crates/time/src/lib.rs
// =====================================================================

// after EffectCraft frames_to_fields
void frames_to_fields(int64_t frame, FrameRate rate, bool drop_frame,
                      bool& neg, int64_t& h, int64_t& m, int64_t& s, int64_t& f) {
    neg = frame < 0;
    int64_t n = (frame == INT64_MIN) ? INT64_MAX : (frame < 0 ? -frame : frame);
    int64_t drop, base;
    df_params(rate, drop, base);
    if (drop_frame && rate.supports_drop_frame()) {
        int64_t per_min = sat_sub(base * 60, drop);
        int64_t per_10  = sat_add(per_min * 10, drop);
        if (per_10 > 0) {
            int64_t d  = n / per_10;
            int64_t mm = n % per_10;
            n = sat_add(n, (drop * 9) * d);
            if (mm > drop && per_min > 0) n = sat_add(n, drop * ((mm - drop) / per_min));
        }
    }
    if (base < 1) base = 1;
    f  = n % base;
    s  = (n / base) % 60;
    m  = (n / (base * 60)) % 60;
    h  = n / (base * 3600);
}

// after EffectCraft fields_to_frames
int64_t fields_to_frames(int64_t h, int64_t m, int64_t s, int64_t f,
                          FrameRate rate, bool drop_frame) {
    int64_t drop, base;
    df_params(rate, drop, base);
    if (base < 1) base = 1;
    int64_t secs  = sat_add(sat_add(h * 3600, m * 60), s);
    int64_t total = sat_add(secs * base, f);
    if (drop_frame && rate.supports_drop_frame()) {
        int64_t total_min = sat_add(h * 60, m);
        total = sat_sub(total, drop * (total_min - total_min / 10));
    }
    return total;
}

// after EffectCraft format_timecode_frames
std::string format_timecode_frames(int64_t frame, FrameRate rate, bool drop_frame) {
    bool df = drop_frame && rate.supports_drop_frame();
    bool neg; int64_t h, m, s, f;
    frames_to_fields(frame, rate, df, neg, h, m, s, f);
    char sep = df ? ';' : ':';
    int64_t base = rate.timecode_base();
    int fw = (base >= 100) ? 3 : 2;
    char buf[64];
    if (fw == 3)
        snprintf(buf, sizeof buf, "%s%02lld%c%02lld%c%02lld%c%03lld",
                 neg ? "-" : "", (long long)h, sep, (long long)m, sep, (long long)s, sep, (long long)f);
    else
        snprintf(buf, sizeof buf, "%s%02lld%c%02lld%c%02lld%c%02lld",
                 neg ? "-" : "", (long long)h, sep, (long long)m, sep, (long long)s, sep, (long long)f);
    return buf;
}

// after EffectCraft format_timecode_ae (hours not zero-padded)
std::string format_timecode_ae(int64_t frame, FrameRate rate, bool drop_frame) {
    bool df = drop_frame && rate.supports_drop_frame();
    bool neg; int64_t h, m, s, f;
    frames_to_fields(frame, rate, df, neg, h, m, s, f);
    char sep = df ? ';' : ':';
    int64_t base = rate.timecode_base();
    int fw = (base >= 100) ? 3 : 2;
    char buf[64];
    if (fw == 3)
        snprintf(buf, sizeof buf, "%s%lld%c%02lld%c%02lld%c%03lld",
                 neg ? "-" : "", (long long)h, sep, (long long)m, sep, (long long)s, sep, (long long)f);
    else
        snprintf(buf, sizeof buf, "%s%lld%c%02lld%c%02lld%c%02lld",
                 neg ? "-" : "", (long long)h, sep, (long long)m, sep, (long long)s, sep, (long long)f);
    return buf;
}

// after EffectCraft format_time
std::string format_time(Tick t, FrameRate rate, bool drop_frame,
                         TimeDisplay display, int64_t sample_rate) {
    int64_t frame = rate.frame_at(t);
    char buf[128];
    switch (display) {
    case TimeDisplay::Timecode:
        return format_timecode_frames(frame, rate, drop_frame);
    case TimeDisplay::Frames:
        snprintf(buf, sizeof buf, "%lld", (long long)frame);
        return buf;
    case TimeDisplay::Feet35:
    case TimeDisplay::Feet16: {
        int64_t per_ft = (display == TimeDisplay::Feet35) ? 16 : 40;
        bool neg = frame < 0;
        uint64_t a = (frame == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(neg ? -frame : frame);
        snprintf(buf, sizeof buf, "%s%lld+%02lld",
                 neg ? "-" : "", (long long)(a / (uint64_t)per_ft), (long long)(a % (uint64_t)per_ft));
        return buf;
    }
    case TimeDisplay::AudioSamples: {
        int64_t sr = (sample_rate <= 0) ? 48000 : sample_rate;
        bool neg = t.v < 0;
        I128 abs_v = I128::from64(t.v < 0 ? (t.v == INT64_MIN ? INT64_MAX : -t.v) : t.v);
        I128 q, r;
        div_euclid128(abs_v, I128::from64(TICKS_PER_SECOND), q, r);
        int64_t secs = clamp128(q);
        int64_t rem = Tick(clamp128(r)).to_units_floor(sr);
        snprintf(buf, sizeof buf, "%s%02lld:%02lld:%02lld:%05lld",
                 neg ? "-" : "", (long long)(secs / 3600), (long long)((secs / 60) % 60),
                 (long long)(secs % 60), (long long)rem);
        return buf;
    }
    case TimeDisplay::Seconds: {
        double s = t.seconds();
        if (std::round(s * 1000.0) == 0.0) s = 0.0;
        snprintf(buf, sizeof buf, "%.3f", s);
        return buf;
    }
    }
    return format_timecode_frames(frame, rate, drop_frame);
}

// after EffectCraft format_feet_frames
std::string format_feet_frames(int64_t frame, int64_t per_foot) {
    int64_t pf = (per_foot < 1) ? 1 : per_foot;
    bool neg = frame < 0;
    uint64_t a = (frame == INT64_MIN) ? ((uint64_t)INT64_MAX + 1) : (uint64_t)(neg ? -frame : frame);
    int w = (pf > 100) ? 3 : 2;
    char buf[64];
    if (w == 3)
        snprintf(buf, sizeof buf, "%s%04lld+%03lld",
                 neg ? "-" : "", (long long)(a / (uint64_t)pf), (long long)(a % (uint64_t)pf));
    else
        snprintf(buf, sizeof buf, "%s%04lld+%02lld",
                 neg ? "-" : "", (long long)(a / (uint64_t)pf), (long long)(a % (uint64_t)pf));
    return buf;
}

static bool parseI64(const char* s, int64_t& out) {
    if (!s || !*s) { out = 0; return true; }
    char* end;
    long long v = strtoll(s, &end, 10);
    if (*end) return false;
    out = (int64_t)v;
    return true;
}

// after EffectCraft parse_timecode
bool parse_timecode(const char* input, FrameRate rate, bool drop_frame,
                    int64_t current, int64_t& out_frame) {
    if (!input) return false;
    std::string s = input;
    size_t st = s.find_first_not_of(" \t\r\n");
    if (st == std::string::npos) return false;
    size_t en = s.find_last_not_of(" \t\r\n");
    s = s.substr(st, en - st + 1);
    if (s.empty()) return false;

    int rel = 0;
    std::string body = s;
    if (s[0] == '+') { rel = 1; body = s.substr(1); }
    else if (s[0] == '-') { rel = -1; body = s.substr(1); }

    std::vector<std::string> parts;
    std::string cur;
    for (char c : body) {
        if (c == ':' || c == ';' || c == '.' || c == ',') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);

    std::vector<int64_t> nums;
    if (parts.size() == 1) {
        const std::string& digits = parts[0];
        if (digits.empty()) return false;
        for (char c : digits) {
            if (c < '0' || c > '9') return false;
        }
        if (rel != 0) {
            int64_t val = 0;
            if (!parseI64(digits.c_str(), val)) return false;
            nums.push_back(val);
        } else {
            // Right-aligned pairs
            int len = (int)digits.size();
            while (len > 0) {
                int pstart = (len >= 2) ? len - 2 : 0;
                std::string pair_str = digits.substr(pstart, len - pstart);
                int64_t val = 0;
                parseI64(pair_str.c_str(), val);
                nums.push_back(val);
                len = pstart;
            }
            std::reverse(nums.begin(), nums.end());
        }
    } else {
        for (const auto& p : parts) {
            if (p.empty()) {
                nums.push_back(0);
            } else {
                int64_t val = 0;
                if (!parseI64(p.c_str(), val)) return false;
                nums.push_back(val);
            }
        }
    }

    if (nums.size() > 4) return false;
    int64_t f4[4] = { 0, 0, 0, 0 };
    int off = 4 - (int)nums.size();
    for (size_t i = 0; i < nums.size(); ++i) {
        f4[off + i] = nums[i];
    }
    int64_t h = f4[0], m = f4[1], sec = f4[2], fr = f4[3];
    int64_t base = rate.timecode_base();
    if (base < 1) base = 1;

    int64_t frames = 0;
    if (nums.size() == 1 && rel != 0) {
        frames = fr;
    } else if (drop_frame && rate.supports_drop_frame() && m < 60 && sec < 60 && fr < base) {
        if (h > INT64_MAX / std::max<int64_t>(1, base * 3600)) return false;
        frames = fields_to_frames(h, m, sec, fr, rate, true);
    } else {
        I128 v = mul128(h, 3600);
        v = add128(v, mul128(m, 60));
        v = add128(v, I128::from64(sec));
        v = mul128_64(v, base);
        v = add128(v, I128::from64(fr));
        if (v.hi != 0 && v.hi != -1) return false;
        if (v.hi == 0 && v.lo > (uint64_t)INT64_MAX) return false;
        if (v.hi == -1 && v.lo < (uint64_t)INT64_MIN) return false;
        frames = (int64_t)v.lo;
    }

    if (rel == -1) {
        out_frame = sat_sub(current, frames);
    } else if (rel == 1) {
        out_frame = sat_add(current, frames);
    } else {
        out_frame = frames;
    }
    return true;
}

// after EffectCraft parse_feet_frames
bool parse_feet_frames(const char* input, int64_t per_foot,
                       int64_t current, int64_t& out_frame) {
    if (!input) return false;
    std::string s = input;
    size_t st = s.find_first_not_of(" \t\r\n");
    if (st == std::string::npos) return false;
    size_t en = s.find_last_not_of(" \t\r\n");
    s = s.substr(st, en - st + 1);
    if (s.empty()) return false;

    int sign = 0;
    std::string body = s;
    if (s[0] == '-') { sign = -1; body = s.substr(1); }
    else if (s[0] == '+') { sign = 1; body = s.substr(1); }

    int64_t pf = (per_foot < 1) ? 1 : per_foot;
    size_t plus_pos = body.rfind('+');
    if (plus_pos != std::string::npos) {
        std::string feet_s = body.substr(0, plus_pos);
        std::string fr_s   = body.substr(plus_pos + 1);
        int64_t feet = 0, fr = 0;
        if (!parseI64(feet_s.c_str(), feet) || !parseI64(fr_s.c_str(), fr)) return false;
        I128 v = add128(mul128(feet, pf), I128::from64(fr));
        if (v.hi != 0 && v.hi != -1) return false;
        if (v.hi == 0 && v.lo > (uint64_t)INT64_MAX) return false;
        if (v.hi == -1 && v.lo < (uint64_t)INT64_MIN) return false;
        int64_t n = (int64_t)v.lo;
        out_frame = (sign < 0) ? -n : n;
    } else {
        int64_t v = 0;
        if (!parseI64(body.c_str(), v)) return false;
        if (sign == -1) out_frame = sat_sub(current, v);
        else if (sign == 1) out_frame = sat_add(current, v);
        else out_frame = v;
    }
    return true;
}

} // namespace Project
