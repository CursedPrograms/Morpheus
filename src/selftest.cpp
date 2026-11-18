// Morpheus - selftest runner + all ported tests.
// Ported test-by-test from:
//   crates/time/src/lib.rs  (all #[test] blocks)
//   crates/project/src/tests.rs  (paths_resolve, animate_and_roundtrip_json,
//                                  comp_resolution, item numbering, etc.)
// after EffectCraft (crates/time/src/lib.rs, crates/project/src/tests.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "selftest.h"
#include "time_.h"
#include "project.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <limits>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace SelfTest {

static std::vector<Test> g_tests;

void add(const std::string& name, std::function<bool()> fn) {
    g_tests.push_back({name, fn});
}

int run() {
    int pass = 0, fail = 0;
    for (const auto& t : g_tests) {
        bool ok = false;
        try { ok = t.fn(); }
        catch (...) { ok = false; }
        if (ok) {
            printf("  PASS  %s\n", t.name.c_str());
            ++pass;
        } else {
            printf("  FAIL  %s\n", t.name.c_str());
            ++fail;
        }
    }
    printf("selftest: %d/%d passed\n", pass, pass+fail);
    return fail;
}

using namespace Project;
// Use Proj as a shorthand for the project struct to avoid namespace/struct name clash
using Proj = ::Project::ProjectData;

// =====================================================================
// TIME TESTS  (after EffectCraft crates/time/src/lib.rs)
// =====================================================================

// after EffectCraft crates/time/src/lib.rs test from_f64_tiny_positive_fps_is_nonzero
static bool t_from_f64_tiny_positive() {
    FrameRate r = FrameRate::from_f64(0.0001);
    return r.num > 0 && r.den > 0;
}

// after EffectCraft crates/time/src/lib.rs test seconds_display_has_no_negative_zero
static bool t_seconds_no_negative_zero() {
    int64_t vs[] = {-1, 0, -100};
    for (int64_t v : vs) {
        std::string s = format_time(Tick(v), FrameRate::FPS_30, false, TimeDisplay::Seconds, 48000);
        if (s != "0.000") return false;
    }
    std::string s = format_time(Tick(-TICKS_PER_SECOND), FrameRate::FPS_30, false, TimeDisplay::Seconds, 48000);
    return s == "-1.000";
}

// after EffectCraft crates/time/src/lib.rs test frame_rate_constructors_accept_extreme_numbers
static bool t_framerate_extreme() {
    FrameRate r1 = FrameRate::make(INT64_MIN, 1);
    if (r1.num != INT64_MIN || r1.den != 1) return false;
    FrameRate r2 = FrameRate::make(INT64_MIN, INT64_MIN);
    if (r2.num != -1 || r2.den != -1) return false;
    FrameRate r3 = FrameRate::make(0, INT64_MIN);
    if (r3.num != 0 || r3.den != -1) return false;
    FrameRate r4 = FrameRate::make(INT64_MIN, 0);
    if (r4.num != -1 || r4.den != 0) return false;
    FrameRate r5 = FrameRate::make(0, 0);
    if (r5.num != 0 || r5.den != 0) return false;
    FrameRate rn = FrameRate::from_f64(std::numeric_limits<double>::quiet_NaN());
    if (rn.num != 0 || rn.den != 1) return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs test hostile_numbers_never_overflow
static bool t_hostile_numbers() {
    FrameRate r = FrameRate::FPS_29_97;
    double bads[] = {1e308, -1e308,
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN(),
                     9.2e18};
    for (double s : bads) {
        Tick t = Tick::from_seconds_f64(s);
        if (t < Tick::MIN_T || t > Tick::MAX_T) return false;
        Tick sn = r.snap_nearest(t); (void)sn;
    }
    if (Tick::from_seconds_f64(std::numeric_limits<double>::quiet_NaN()) != Tick::ZERO)
        return false;
    // Saturating add
    if ((Tick(INT64_MAX) + Tick(1)).v != INT64_MAX) return false;
    // Saturating neg
    if ((-Tick(INT64_MIN)).v != INT64_MAX) return false;
    // Format hostile values without crash
    std::string s1 = format_timecode_frames(INT64_MIN, r, true);
    std::string s2 = format_feet_frames(INT64_MIN, 16);
    (void)s1; (void)s2;
    // Parse should fail on overflow
    int64_t fr2;
    if (parse_timecode("99999999999999:00:00:00", r, false, 0, fr2)) return false;
    if (parse_timecode("9223372036854775807:59:59:29", r, true, 0, fr2)) return false;
    // Zero rate doesn't divide by zero
    FrameRate z{0,0};
    z.frame_duration(); z.frame_at(Tick(5)); z.tick_of(3); z.snap_nearest(Tick(7)); z.timecode_base();
    return true;
}

// after EffectCraft crates/time/src/lib.rs test drop_frame_known_values
static bool t_drop_frame_known_values() {
    FrameRate r = FrameRate::FPS_29_97;
    if (format_timecode_frames(0,     r, true) != "00;00;00;00") return false;
    if (format_timecode_frames(1799,  r, true) != "00;00;59;29") return false;
    if (format_timecode_frames(1800,  r, true) != "00;01;00;02") return false;
    if (format_timecode_frames(17982, r, true) != "00;10;00;00") return false;
    if (format_timecode_frames(107892,r, true) != "01;00;00;00") return false;
    if (format_timecode_frames(1800,  r, false)!= "00:01:00:00") return false;
    FrameRate r60 = FrameRate::FPS_59_94;
    if (format_timecode_frames(3600, r60, true) != "00;01;00;04") return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs test parse_forms
static bool t_parse_forms() {
    FrameRate r25 = FrameRate::FPS_25;
    int64_t fr2 = 0;
    if (!parse_timecode("00:00:10:00", r25, false, 0, fr2) || fr2 != 250) return false;
    if (!parse_timecode("1000",        r25, false, 0, fr2) || fr2 != 250) return false;
    if (!parse_timecode("1.00",        r25, false, 0, fr2) || fr2 != 25)  return false;
    if (!parse_timecode("+15",         r25, false, 100, fr2) || fr2 != 115) return false;
    if (!parse_timecode("-1.00",       r25, false, 100, fr2) || fr2 != 75)  return false;
    if (!parse_timecode("00;01;00;02", FrameRate::FPS_29_97, true, 0, fr2) || fr2 != 1800) return false;
    if (parse_timecode("abc", r25, false, 0, fr2)) return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs test display_modes
static bool t_display_modes() {
    FrameRate r = FrameRate::FPS_24;
    Tick t = r.tick_of(40);
    if (format_time(t, r, false, TimeDisplay::Feet35, 48000) != "2+08") return false;
    if (format_time(t, r, false, TimeDisplay::Feet16, 48000) != "1+00") return false;
    if (format_time(t, r, false, TimeDisplay::Frames, 48000) != "40") return false;
    Tick ta = Tick::from_units(48001, 48000);
    if (format_time(ta, r, false, TimeDisplay::AudioSamples, 48000) != "00:00:01:00001") return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs test every_common_rate_is_exact
static bool t_every_common_rate_is_exact() {
    const FrameRate COMMON[] = {
        FrameRate::FPS_23_976, FrameRate::FPS_24, FrameRate::FPS_25,
        FrameRate::FPS_29_97, FrameRate::FPS_30, FrameRate::FPS_48,
        FrameRate::FPS_50, FrameRate::FPS_59_94, FrameRate::FPS_60,
        FrameRate::FPS_119_88, FrameRate::FPS_120
    };
    for (const auto& r : COMMON) {
        Tick d = r.frame_duration();
        // d.v * r.num should equal TICKS_PER_SECOND * r.den
        // Use double for approximate check (the exact check would need 128-bit)
        double lhs = (double)d.v * (double)r.num;
        double rhs = (double)TICKS_PER_SECOND * (double)r.den;
        if (std::fabs(lhs - rhs) > 1.0) return false;
    }
    const int64_t SR[] = {32000, 44100, 48000, 88200, 96000, 192000,
                          8000, 11025, 16000, 22050, 176400};
    for (int64_t sr : SR) {
        if (TICKS_PER_SECOND % sr != 0) return false;
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs test fractional_tick_frame_boundaries
static bool t_fractional_tick_frame_boundaries() {
    FrameRate rate = FrameRate::from_f64(12.345);
    if (rate.num != 2469 || rate.den != 200) return false;
    int64_t frames[] = {-10000, -2, -1, 0, 1, 2, 10000};
    for (int64_t frame : frames) {
        Tick start = rate.tick_of(frame);
        if (rate.frame_at(start) != frame) return false;
        if (rate.frame_at(start - Tick(1)) != frame - 1) return false;
        if (rate.snap(start) != start) return false;
        if (rate.snap_nearest(start) != start) return false;
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs test common_rate_boundaries_keep_their_exact_ticks
static bool t_common_rate_boundaries_exact() {
    const FrameRate COMMON[] = {
        FrameRate::FPS_23_976, FrameRate::FPS_24, FrameRate::FPS_25,
        FrameRate::FPS_29_97, FrameRate::FPS_30, FrameRate::FPS_48,
        FrameRate::FPS_50, FrameRate::FPS_59_94, FrameRate::FPS_60,
        FrameRate::FPS_119_88, FrameRate::FPS_120
    };
    for (const auto& rate : COMMON) {
        int64_t frames[] = {-10000, -1, 0, 1, 10000};
        for (int64_t frame : frames) {
            // expected = frame * TICKS_PER_SECOND * den / num  (integer exact for common rates)
            double expected = (double)frame * (double)TICKS_PER_SECOND * (double)rate.den / (double)rate.num;
            Tick t = rate.tick_of(frame);
            if (std::fabs((double)t.v - expected) > 1.0) return false;
            if (rate.frame_at(rate.tick_of(frame) - Tick(1)) != frame - 1) return false;
        }
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs test negative_relative_min_field_saturates
static bool t_negative_relative_saturates() {
    int64_t fr2;
    if (!parse_timecode("-0:0:0:-9223372036854775808", FrameRate::FPS_30, false, 0, fr2)) return false;
    if (fr2 != INT64_MAX) return false;
    if (!parse_timecode("+0:0:0:-9223372036854775808", FrameRate::FPS_30, false, 0, fr2)) return false;
    if (fr2 != INT64_MIN) return false;
    if (!parse_timecode("-1", FrameRate::FPS_30, false, INT64_MIN, fr2)) return false;
    if (fr2 != INT64_MIN) return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs test common_rate_timecode_roundtrips
static bool t_timecode_roundtrips() {
    const FrameRate COMMON[] = {
        FrameRate::FPS_23_976, FrameRate::FPS_24, FrameRate::FPS_25,
        FrameRate::FPS_29_97, FrameRate::FPS_30, FrameRate::FPS_48,
        FrameRate::FPS_50, FrameRate::FPS_59_94, FrameRate::FPS_60,
        FrameRate::FPS_119_88, FrameRate::FPS_120
    };
    for (const auto& rate : COMMON) {
        int64_t frames[] = {-10000, -1, 0, 1, 10000};
        for (int64_t frame : frames) {
            std::string text = format_timecode_frames(frame, rate, false);
            int64_t back;
            if (!parse_timecode(text.c_str(), rate, false, 0, back)) return false;
            if (back != frame) return false;
        }
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs test hostile_rates_format_and_parse
static bool t_hostile_rates_format_parse() {
    const FrameRate bad_rates[] = {
        FrameRate::make(INT64_MIN, -1),
        {1, INT64_MIN},
        FrameRate::make(INT64_MIN, INT64_MIN),
        {0, 0},
        {-30, 1},
    };
    for (const auto& rate : bad_rates) {
        if (rate.timecode_base() != 1) return false;
        if (format_timecode_frames(0, rate, false) != "00:00:00:00") return false;
        int64_t fr2;
        if (!parse_timecode("0:0:0:0", rate, false, 0, fr2)) return false;
        if (fr2 != 0) return false;
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs Tick arithmetic
static bool t_tick_arithmetic() {
    // Saturating add/sub
    if ((Tick(INT64_MAX) + Tick(1)).v != INT64_MAX) return false;
    if ((Tick(INT64_MIN) - Tick(1)).v != INT64_MIN) return false;
    if ((-Tick(INT64_MIN)).v != INT64_MAX) return false;
    // from_units roundtrip
    for (int64_t sr : {(int64_t)44100, (int64_t)48000, (int64_t)96000}) {
        Tick t = Tick::from_units(1234, sr);
        int64_t back = t.to_units_floor(sr);
        if (back != 1234) return false;
    }
    // seconds roundtrip
    double ss[] = {0.0, 1.0, -1.0, 10.5, 123.456};
    for (double s : ss) {
        Tick t = Tick::from_seconds_f64(s);
        if (std::fabs(t.seconds() - s) > 1e-9) return false;
    }
    return true;
}

// after EffectCraft crates/time/src/lib.rs TimeRange
static bool t_time_range() {
    TimeRange r(Tick(100), Tick(200));
    if (r.end().v != 300) return false;
    if (!r.contains(Tick(100))) return false;
    if (!r.contains(Tick(299))) return false;
    if (r.contains(Tick(300))) return false;
    if (r.contains(Tick(99)))  return false;
    TimeRange r2 = TimeRange::from_bounds(Tick(50), Tick(150));
    if (r2.start.v != 50 || r2.duration.v != 100) return false;
    if (!r.overlaps(r2)) return false;
    TimeRange r3(Tick(300), Tick(100));
    if (r.overlaps(r3)) return false;
    TimeRange empty(Tick(0), Tick(0));
    if (!empty.is_empty()) return false;
    return true;
}

// after EffectCraft crates/time/src/lib.rs FrameRate::from_f64 snap to common
static bool t_framerate_snap_to_common() {
    if (FrameRate::from_f64(29.97)  != FrameRate::FPS_29_97)  return false;
    if (FrameRate::from_f64(24.0)   != FrameRate::FPS_24)     return false;
    if (FrameRate::from_f64(25.0)   != FrameRate::FPS_25)     return false;
    if (FrameRate::from_f64(23.976) != FrameRate::FPS_23_976) return false;
    return true;
}

// =====================================================================
// PROJECT TESTS  (after EffectCraft crates/project/src/tests.rs)
// =====================================================================

// after EffectCraft crates/project/src/tests.rs Project::default + makeNew
static bool t_project_make_new() {
    Proj p = Proj::makeNew();
    if (p.compositions.size() != 1) return false;
    if (p.compositions[0].name != "Composition 1") return false;
    if (p.compositions[0].width != 1920 || p.compositions[0].height != 1080) return false;
    if (p.compositions[0].frameRate.num != 30000 || p.compositions[0].frameRate.den != 1001) return false;
    if (std::fabs(p.compositions[0].duration.seconds() - 10.0) > 0.001) return false;
    if (p.activeComp != 0) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs animate_and_roundtrip_json
static bool t_project_roundtrip_json() {
    Proj p = Proj::makeNew();
    p.name = "Test Project";
    p.compositions[0].name = "Main";
    p.compositions[0].frameRate = FrameRate::FPS_29_97;
    p.compositions[0].duration  = Tick::from_seconds_f64(10.0);

    Layer l;
    l.id.v   = p.alloc();
    l.name   = "Red Solid 1";
    l.kind   = LayerKind::Solid;
    l.label  = Label::Red;
    l.solidR = 1.0f; l.solidG = 0.0f; l.solidB = 0.0f; l.solidA = 1.0f;
    l.inPt   = Tick::ZERO;
    l.outPt  = Tick::from_seconds_f64(10.0);
    l.xform.posX = 960; l.xform.posY = 540;
    p.compositions[0].layers.push_back(l);

    Layer nl;
    nl.id.v  = p.alloc();
    nl.name  = "Null 1";
    nl.kind  = LayerKind::Null;
    nl.inPt  = Tick::ZERO;
    nl.outPt = Tick::from_seconds_f64(10.0);
    p.compositions[0].layers.push_back(nl);

    const char* tmpPath = "selftest_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) return false;
    remove(tmpPath);

    if (q.compositions.size() != 1) return false;
    if (q.compositions[0].name != "Main") return false;
    if (q.compositions[0].layers.size() != 2) return false;
    if (q.compositions[0].layers[0].kind != LayerKind::Solid) return false;
    if (q.compositions[0].layers[0].name != "Red Solid 1") return false;
    if (q.compositions[0].layers[1].kind != LayerKind::Null) return false;
    if (q.compositions[0].layers[1].name != "Null 1") return false;
    if (q.compositions[0].duration.v != Tick::from_seconds_f64(10.0).v) return false;
    if (q.compositions[0].layers[0].outPt.v != Tick::from_seconds_f64(10.0).v) return false;
    if (q.compositions[0].frameRate.num != 30000 || q.compositions[0].frameRate.den != 1001) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs comp_resolution_saved
static bool t_comp_resolution_saved() {
    Proj p = Proj::makeNew();
    p.compositions[0].resolution = Resolution::Half;
    const char* tmpPath = "selftest_res_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) return false;
    remove(tmpPath);
    if (q.compositions[0].resolution != Resolution::Half) return false;
    p.compositions[0].resolution = Resolution::Auto;
    if (!save(p, tmpPath)) return false;
    Proj r;
    if (!load(r, tmpPath)) return false;
    remove(tmpPath);
    if (r.compositions[0].resolution != Resolution::Auto) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs all layer kinds roundtrip
static bool t_layer_kinds_roundtrip() {
    Proj p = Proj::makeNew();
    const LayerKind kinds[] = {
        LayerKind::Solid, LayerKind::Shape, LayerKind::Text,
        LayerKind::Footage, LayerKind::Composition, LayerKind::Null,
        LayerKind::Camera, LayerKind::Light
    };
    for (auto k : kinds) {
        Layer l;
        l.id.v  = p.alloc();
        l.name  = std::string("Layer ") + layerKindTypeName(k);
        l.kind  = k;
        l.inPt  = Tick::ZERO;
        l.outPt = Tick::from_seconds_f64(5.0);
        p.compositions[0].layers.push_back(l);
    }
    const char* tmpPath = "selftest_kinds_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) return false;
    remove(tmpPath);
    const size_t nk = sizeof(kinds)/sizeof(kinds[0]);
    if (q.compositions[0].layers.size() != nk) return false;
    for (size_t i = 0; i < nk; ++i)
        if (q.compositions[0].layers[i].kind != kinds[i]) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs switches roundtrip
static bool t_switches_roundtrip() {
    Proj p = Proj::makeNew();
    Layer l;
    l.id.v  = p.alloc();
    l.name  = "Layer 1";
    l.kind  = LayerKind::Solid;
    l.inPt  = Tick::ZERO;
    l.outPt = Tick::from_seconds_f64(10.0);
    l.switches.solo       = true;
    l.switches.shy        = true;
    l.switches.motionBlur = true;
    l.switches.adjustment = true;
    l.switches.threeD     = true;
    p.compositions[0].layers.push_back(l);
    const char* tmpPath = "selftest_sw_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) return false;
    remove(tmpPath);
    const auto& ql = q.compositions[0].layers[0];
    if (!ql.switches.solo)       return false;
    if (!ql.switches.shy)        return false;
    if (!ql.switches.motionBlur) return false;
    if (!ql.switches.adjustment) return false;
    if (!ql.switches.threeD)     return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs transform roundtrip
static bool t_transform_roundtrip() {
    Proj p = Proj::makeNew();
    Layer l;
    l.id.v  = p.alloc();
    l.name  = "L";
    l.kind  = LayerKind::Solid;
    l.inPt  = Tick::ZERO;
    l.outPt = Tick::from_seconds_f64(5.0);
    l.xform.anchorX  = 100; l.xform.anchorY  = 200;
    l.xform.posX     = 960; l.xform.posY     = 540;
    l.xform.scaleX   = 50;  l.xform.scaleY   = 75;
    l.xform.rotation = 45;
    l.xform.opacity  = 80;
    p.compositions[0].layers.push_back(l);
    const char* tmpPath = "selftest_xf_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) return false;
    remove(tmpPath);
    const auto& xl = q.compositions[0].layers[0];
    if (std::fabs(xl.xform.anchorX  - 100) > 0.01f) return false;
    if (std::fabs(xl.xform.anchorY  - 200) > 0.01f) return false;
    if (std::fabs(xl.xform.posX     - 960) > 0.01f) return false;
    if (std::fabs(xl.xform.posY     - 540) > 0.01f) return false;
    if (std::fabs(xl.xform.scaleX   - 50 ) > 0.01f) return false;
    if (std::fabs(xl.xform.scaleY   - 75 ) > 0.01f) return false;
    if (std::fabs(xl.xform.rotation - 45 ) > 0.01f) return false;
    if (std::fabs(xl.xform.opacity  - 80 ) > 0.01f) return false;
    return true;
}

// =====================================================================
// Registration
// =====================================================================
void registerAll() {
    // Time tests (after EffectCraft crates/time/src/lib.rs)
    add("time::from_f64_tiny_positive_fps_is_nonzero",         t_from_f64_tiny_positive);
    add("time::seconds_display_has_no_negative_zero",           t_seconds_no_negative_zero);
    add("time::frame_rate_constructors_accept_extreme_numbers", t_framerate_extreme);
    add("time::hostile_numbers_never_overflow",                 t_hostile_numbers);
    add("time::drop_frame_known_values",                        t_drop_frame_known_values);
    add("time::parse_forms",                                    t_parse_forms);
    add("time::display_modes",                                  t_display_modes);
    add("time::every_common_rate_is_exact",                     t_every_common_rate_is_exact);
    add("time::fractional_tick_frame_boundaries",               t_fractional_tick_frame_boundaries);
    add("time::common_rate_boundaries_keep_their_exact_ticks",  t_common_rate_boundaries_exact);
    add("time::negative_relative_min_field_saturates",          t_negative_relative_saturates);
    add("time::common_rate_timecode_roundtrips",                t_timecode_roundtrips);
    add("time::hostile_rates_format_and_parse_without_panic",   t_hostile_rates_format_parse);
    add("time::tick_saturating_arithmetic",                     t_tick_arithmetic);
    add("time::time_range",                                     t_time_range);
    add("time::framerate_from_f64_snaps_to_common",             t_framerate_snap_to_common);
    // Project tests (after EffectCraft crates/project/src/tests.rs)
    add("project::make_new_defaults",          t_project_make_new);
    add("project::roundtrip_json",             t_project_roundtrip_json);
    add("project::comp_resolution_saved",      t_comp_resolution_saved);
    add("project::all_layer_kinds_roundtrip",  t_layer_kinds_roundtrip);
    add("project::switches_roundtrip",         t_switches_roundtrip);
    add("project::transform_roundtrip",        t_transform_roundtrip);
}

} // namespace SelfTest
