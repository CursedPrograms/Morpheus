// Morpheus - exact media time, ported function-by-function from
// crates/time/src/lib.rs in EffectCraft.
//
// All time is an integer number of Ticks at TICKS_PER_SECOND = 254016000000/s.
// That rate divides evenly into every broadcast frame duration and common audio
// sample rate, so edits and frame maths never drift.
// Timecode, feet+frames, frames, samples are display-only conversions of Ticks.
//
// after EffectCraft (crates/time/src/lib.rs)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <string>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Project {

// =====================================================================
// Tick  (after EffectCraft crates/time/src/lib.rs Tick)
// =====================================================================

/// Ticks per second (254 016 000 000).
static constexpr int64_t TICKS_PER_SECOND = 254'016'000'000LL;

/// A saturating integer representing a point or duration in time, in ticks.
struct Tick {
    int64_t v;

    static const Tick ZERO;
    static const Tick MAX_T;
    static const Tick MIN_T;

    constexpr explicit Tick(int64_t x = 0) : v(x) {}

    // after EffectCraft Tick::from_seconds_f64
    static Tick from_seconds_f64(double s);
    // after EffectCraft Tick::seconds
    double seconds() const;
    // after EffectCraft Tick::from_units
    static Tick from_units(int64_t units, int64_t per_second);
    // after EffectCraft Tick::to_units_floor
    int64_t to_units_floor(int64_t per_second) const;
    // after EffectCraft Tick::from_rational
    static Tick from_rational(int64_t pts, int64_t num, int64_t den);
    // after EffectCraft Tick::to_rational_floor
    int64_t to_rational_floor(int64_t num, int64_t den) const;
    // after EffectCraft Tick::abs
    Tick abs() const;
    // after EffectCraft Tick::min / max / clamp
    Tick min_t(Tick o) const;
    Tick max_t(Tick o) const;
    Tick clamp_t(Tick lo, Tick hi) const;
    // after EffectCraft Tick::mul_ratio
    Tick mul_ratio(int64_t num, int64_t den) const;

    // Saturating arithmetic (after EffectCraft ops Add/Sub/Neg/AddAssign/SubAssign)
    Tick operator+(Tick o) const;
    Tick operator-(Tick o) const;
    Tick operator-() const;
    Tick& operator+=(Tick o);
    Tick& operator-=(Tick o);

    bool operator==(Tick o) const { return v == o.v; }
    bool operator!=(Tick o) const { return v != o.v; }
    bool operator< (Tick o) const { return v <  o.v; }
    bool operator<=(Tick o) const { return v <= o.v; }
    bool operator> (Tick o) const { return v >  o.v; }
    bool operator>=(Tick o) const { return v >= o.v; }
};

// =====================================================================
// TimeRange  (after EffectCraft crates/time/src/lib.rs TimeRange)
// =====================================================================
struct TimeRange {
    Tick start;
    Tick duration;

    TimeRange() : start(Tick::ZERO), duration(Tick::ZERO) {}
    TimeRange(Tick s, Tick d) : start(s), duration(d) {}

    // after EffectCraft TimeRange::from_bounds
    static TimeRange from_bounds(Tick start, Tick end);
    // after EffectCraft TimeRange::end
    Tick end() const;
    // after EffectCraft TimeRange::contains
    bool contains(Tick t) const;
    // after EffectCraft TimeRange::overlaps
    bool overlaps(const TimeRange& o) const;
    // after EffectCraft TimeRange::intersect
    bool intersect(const TimeRange& o, TimeRange& out) const;
    // after EffectCraft TimeRange::is_empty
    bool is_empty() const;
};

// =====================================================================
// FrameRate  (after EffectCraft crates/time/src/lib.rs FrameRate)
// =====================================================================
struct FrameRate {
    int64_t num;
    int64_t den;

    // Named constants (after EffectCraft FrameRate::FPS_*)
    static const FrameRate FPS_23_976;
    static const FrameRate FPS_24;
    static const FrameRate FPS_25;
    static const FrameRate FPS_29_97;
    static const FrameRate FPS_30;
    static const FrameRate FPS_48;
    static const FrameRate FPS_50;
    static const FrameRate FPS_59_94;
    static const FrameRate FPS_60;
    static const FrameRate FPS_119_88;
    static const FrameRate FPS_120;

    // after EffectCraft FrameRate::new (reduces by GCD, handles 0/MIN edge cases)
    static FrameRate make(int64_t num, int64_t den);
    // after EffectCraft FrameRate::from_f64 (snap to nearest common rate)
    static FrameRate from_f64(double fps);
    // after EffectCraft FrameRate::as_f64
    double as_f64() const;
    // after EffectCraft FrameRate::frame_duration
    Tick frame_duration() const;
    // after EffectCraft FrameRate::frame_at (floor)
    int64_t frame_at(Tick t) const;
    // after EffectCraft FrameRate::tick_of (ceiling at fractional boundary)
    Tick tick_of(int64_t f) const;
    // after EffectCraft FrameRate::snap (floor to frame boundary)
    Tick snap(Tick t) const;
    // after EffectCraft FrameRate::snap_nearest
    Tick snap_nearest(Tick t) const;
    // after EffectCraft FrameRate::timecode_base
    int64_t timecode_base() const;
    // after EffectCraft FrameRate::is_ntsc
    bool is_ntsc() const;
    // after EffectCraft FrameRate::supports_drop_frame
    bool supports_drop_frame() const;
    // after EffectCraft FrameRate::label
    std::string label() const;

    bool operator==(const FrameRate& o) const { return num==o.num && den==o.den; }
    bool operator!=(const FrameRate& o) const { return !(*this==o); }
};

// Common audio sample rates (after EffectCraft crates/time/src/lib.rs SAMPLE_RATES)
static constexpr int64_t SAMPLE_RATES[] = {32000, 44100, 48000, 88200, 96000, 192000};

// =====================================================================
// TimeDisplay  (after EffectCraft crates/time/src/lib.rs TimeDisplay)
// =====================================================================
enum class TimeDisplay {
    Timecode,
    Frames,
    Feet16,   // Feet + Frames 16mm (40 frames per foot)
    Feet35,   // Feet + Frames 35mm (16 frames per foot)
    AudioSamples,
    Seconds,
};

// =====================================================================
// Drop-frame helpers  (after EffectCraft crates/time/src/lib.rs)
// =====================================================================

// Convert a frame count to SMPTE fields (neg, h, m, s, f).
// after EffectCraft crates/time/src/lib.rs frames_to_fields
void frames_to_fields(int64_t frame, FrameRate rate, bool drop_frame,
                      bool& neg, int64_t& h, int64_t& m, int64_t& s, int64_t& f);

// Convert SMPTE fields back to a frame count.
// after EffectCraft crates/time/src/lib.rs fields_to_frames
int64_t fields_to_frames(int64_t h, int64_t m, int64_t s, int64_t f,
                          FrameRate rate, bool drop_frame);

// Format as SMPTE timecode "HH:MM:SS:FF" (or ";;" for drop-frame).
// after EffectCraft crates/time/src/lib.rs format_timecode_frames
std::string format_timecode_frames(int64_t frame, FrameRate rate, bool drop_frame);

// Format as AE-style "0:MM:SS:FF" (hours not zero-padded).
// after EffectCraft crates/time/src/lib.rs format_timecode_ae
std::string format_timecode_ae(int64_t frame, FrameRate rate, bool drop_frame);

// Format a tick according to a display mode.
// after EffectCraft crates/time/src/lib.rs format_time
std::string format_time(Tick t, FrameRate rate, bool drop_frame,
                         TimeDisplay display, int64_t sample_rate = 48000);

// Format as feet+frames ("0012+07").
// after EffectCraft crates/time/src/lib.rs format_feet_frames
std::string format_feet_frames(int64_t frame, int64_t per_foot);

// Parse user timecode text into a frame count.
// after EffectCraft crates/time/src/lib.rs parse_timecode
// Returns true on success; sets frame.
bool parse_timecode(const char* input, FrameRate rate, bool drop_frame,
                    int64_t current, int64_t& frame);

// Parse feet+frames into a frame count.
// after EffectCraft crates/time/src/lib.rs parse_feet_frames
bool parse_feet_frames(const char* input, int64_t per_foot,
                       int64_t current, int64_t& frame);

} // namespace Project
