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
#include "icons.h"
#include "keyframe.h"
#include "props.h"
#include "raster.h"
#include "render.h"
#include "timeline.h"

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
        fflush(stdout);
    }
    printf("selftest: %d/%d passed\n", pass, pass+fail);
    fflush(stdout);
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

// after EffectCraft crates/project/src/build.rs contents, shape_group, shape_rect and shape_fill
static bool t_shape_contents_roundtrip() {
    Proj p = Proj::makeNew();
    Layer layer;
    layer.id.v = p.alloc();
    layer.name = "Shape Layer";
    layer.kind = LayerKind::Shape;
    layer.inPt = Tick::ZERO;
    layer.outPt = p.compositions[0].duration;
    layer.initProps(&p.nextId);
    Props::PropGroup* contents = layer.props.sub_mut("contents");
    if (!contents || !contents->children.empty()) return false;
    Props::Ids ids(&p.nextId);
    std::vector<Props::PropGroup> items;
    items.push_back(Props::Build::shapeRect(ids, {80.0, 40.0}, {12.0, -6.0}, 5.0));
    items.push_back(Props::Build::shapeFill(ids, {0.1, 0.2, 0.3, 0.8}));
    contents->children.push_back(Props::Node(Props::Build::shapeGroup(ids, "Group 1", std::move(items))));
    p.compositions[0].layers.push_back(std::move(layer));
    const char* tmpPath = "selftest_shape_tmp.morpheus";
    if (!save(p, tmpPath)) return false;
    Proj q;
    if (!load(q, tmpPath)) {
        remove(tmpPath);
        return false;
    }
    remove(tmpPath);
    const auto* group = q.compositions[0].layers[0].props.group("contents/group#1");
    const auto* rect = group ? group->group("contents/rect#1") : nullptr;
    const auto* size = rect ? rect->prop("size") : nullptr;
    const auto* fill = group ? group->group("contents/fill#1") : nullptr;
    const auto* color = fill ? fill->prop("color") : nullptr;
    return size && size->value == KeyframeModel::Value::Vec2(80.0, 40.0) &&
        color && color->value == KeyframeModel::Value::Color(0.1, 0.2, 0.3, 0.8);
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

static std::pair<Proj, size_t> project_with_layer() {
    Proj p = Proj::makeNew();
    auto& comp = p.compositions[0];
    comp.name = "Main";
    comp.width = 1920;
    comp.height = 1080;
    comp.frameRate = FrameRate::FPS_29_97;
    comp.duration = Tick::from_seconds_f64(10.0);
    comp.layers.clear();

    Layer l;
    l.id.v = p.alloc();
    l.name = "Red Solid 1";
    l.kind = LayerKind::Solid;
    l.label = Label::Red;
    l.solidR = 1.0f; l.solidG = 0.0f; l.solidB = 0.0f; l.solidA = 1.0f;
    l.inPt = Tick::ZERO;
    l.outPt = Tick::from_seconds_f64(10.0);
    l.initProps(&p.nextId, 960.0, 540.0, 960.0, 540.0);
    comp.layers.push_back(l);

    return {p, 0};
}

// after EffectCraft crates/project/src/tests.rs paths_resolve
static bool t_project_paths_resolve() {
    auto [p, cid] = project_with_layer();
    const auto& l = p.compositions[cid].layers[0];
    const auto* pos = l.props.prop("transform/position");
    if (!pos || pos->value != KeyframeModel::Value::Vec3(960.0, 540.0, 0.0)) return false;
    const auto* op = l.props.prop("Transform/Opacity");
    if (!op || op->value != KeyframeModel::Value::Scalar(100.0)) return false;
    const auto* rot = l.props.prop("transform/#7");
    if (!rot || rot->match_id != "rotation") return false;
    const auto* sc = l.props.prop("transform/scale");
    if (!sc) return false;
    auto path = l.props.path_of(sc->uid);
    if (!path.has_value()) return false;
    const auto* sc2 = l.props.prop(*path);
    if (!sc2 || sc2->match_id != "scale") return false;
    auto npath = l.props.name_path_of(sc->uid);
    if (!npath.has_value() || *npath != "Transform/Scale") return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs masks_and_occurrence_paths
static bool t_project_masks_and_occurrence_paths() {
    auto [p, cid] = project_with_layer();
    Props::Ids ids(&p.nextId);
    auto m1 = Props::Build::mask(ids, "Mask 1", KeyframeModel::ShapePath::rect({0.0, 0.0}, 10.0, 10.0), Props::MaskMode::Add, {255, 0, 0});
    auto m2 = Props::Build::mask(ids, "Mask 2", KeyframeModel::ShapePath::ellipse({0.0, 0.0}, 10.0, 10.0), Props::MaskMode::Subtract, {0, 255, 0});
    auto& l = p.compositions[cid].layers[0];
    auto* masks = l.props.sub_mut("masks");
    if (!masks) return false;
    masks->children.push_back(Props::Node(std::move(m1)));
    masks->children.push_back(Props::Node(std::move(m2)));
    const auto* g2 = l.props.group("masks/mask#2");
    if (!g2 || g2->name != "Mask 2") return false;
    const auto* g2n = l.props.group("masks/Mask 2");
    if (!g2n || g2n->name != "Mask 2") return false;
    if (!l.props.prop("masks/mask#2/feather")) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs animate_and_roundtrip_json
static bool t_project_animate_and_roundtrip_json() {
    auto [p, cid] = project_with_layer();
    {
        auto& l = p.compositions[cid].layers[0];
        auto* op = l.props.prop_mut("transform/opacity");
        if (!op) return false;
        op->set_animated(true, Tick::ZERO);
        op->set_value_at(Tick::from_seconds_f64(1.0), KeyframeModel::Value::Scalar(0.0));
        if (op->keys.size() != 2) return false;
        if (op->value_at(Tick::from_seconds_f64(0.5)) != KeyframeModel::Value::Scalar(50.0)) return false;
    }
    std::string json = p.toJson();
    Proj q;
    if (!Proj::fromJson(q, json)) return false;
    if (p != q) return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs numbered_names_keep_digits_that_end_a_word
static bool t_project_numbered_names() {
    const std::pair<std::string, std::string> rows[] = {
        {"v-b1.mp4", "v-b1.mp4 2"},
        {"clip2.mov", "clip2.mov 2"},
        {"shot 2", "shot 3"},
        {"a.b.c", "a.b.c 2"},
        {"Background", "Background 2"},
        {"Red Solid 1", "Red Solid 2"},
    };
    for (const auto& [name, next] : rows) {
        Proj p = Proj::makeNew();
        auto& comp = p.compositions[0];
        comp.layers.clear();
        std::string n1 = comp.uniqueLayerName(name);
        if (n1 != name) return false;
        Layer l1; l1.name = n1;
        comp.layers.push_back(l1);
        std::string n2 = comp.uniqueLayerName(name);
        if (n2 != next) return false;
        std::string item_copy = p.numberedItemName(name, [&](const std::string& n) { return n == name; });
        if (item_copy != next) return false;
    }
    return true;
}

// after EffectCraft crates/project/src/tests.rs unique_names_and_cycles
static bool t_project_unique_names() {
    auto [p, cid] = project_with_layer();
    const auto& c = p.compositions[cid];
    if (c.uniqueLayerName("Red Solid 1") != "Red Solid 2") return false;
    if (c.uniqueLayerName("Shape Layer 1") != "Shape Layer 1") return false;

    Composition d = c;
    for (const char* n : {"Red Solid 02", "Red Solid 3"}) {
        Layer l = d.layers[0];
        l.name = n;
        d.layers.push_back(l);
    }
    if (d.uniqueLayerName("Red Solid 1") != "Red Solid 2") return false;

    Composition e = c;
    e.layers[0].name = "v-b1.mp4";
    if (e.uniqueLayerName("v-b1.mp4") != "v-b1.mp4 2") return false;
    return true;
}

// after EffectCraft crates/project/src/tests.rs same_content_sees_allocated_ids_and_comp_edits
static bool t_project_same_content() {
    auto [p, cid] = project_with_layer();
    Proj q = p;
    if (q != p) return false;
    q.alloc();
    if (q == p) return false;
    return true;
}

static Tick kf_s(double x) {
    return Tick::from_seconds_f64(x);
}

// after EffectCraft crates/keyframe/src/tests.rs missing_spatial_keys_have_no_tangents_or_length
static bool t_kf_missing_spatial_keys() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0))
    };
    for (size_t i : { (size_t)1, std::numeric_limits<size_t>::max() }) {
        auto t = KeyframeModel::spatial_tangents(keys, i);
        if (t.first != std::array<double, 3>{0,0,0} || t.second != std::array<double, 3>{0,0,0}) return false;
        if (KeyframeModel::spatial_segment_length(keys, i) != 0.0) return false;
    }
    std::vector<KeyframeModel::Keyframe> empty;
    auto t0 = KeyframeModel::spatial_tangents(empty, 0);
    if (t0.first != std::array<double, 3>{0,0,0} || t0.second != std::array<double, 3>{0,0,0}) return false;
    if (KeyframeModel::spatial_segment_length(empty, std::numeric_limits<size_t>::max()) != 0.0) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs time_reverse_handles_deserialized_tick_extremes
static bool t_kf_time_reverse_extremes() {
    const std::pair<int64_t, int64_t> ranges[] = {
        { std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::min() + 10 },
        { std::numeric_limits<int64_t>::max() - 10, std::numeric_limits<int64_t>::max() },
        { std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() }
    };
    for (auto [first, last] : ranges) {
        std::vector<KeyframeModel::Keyframe> keys = {
            KeyframeModel::Keyframe(Tick(first), KeyframeModel::Value::Scalar(1.0)),
            KeyframeModel::Keyframe(Tick(last), KeyframeModel::Value::Scalar(2.0))
        };
        KeyframeModel::time_reverse(keys);
        if (keys[0].time.v != first || keys[0].value != KeyframeModel::Value::Scalar(2.0)) return false;
        if (keys[1].time.v != last || keys[1].value != KeyframeModel::Value::Scalar(1.0)) return false;
        KeyframeModel::time_reverse(keys);
        if (keys[0].value != KeyframeModel::Value::Scalar(1.0)) return false;
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs replacing_key_preserves_spatial_metadata
static bool t_kf_replacing_key_preserves_spatial_metadata() {
    KeyframeModel::Keyframe key(kf_s(1.0), KeyframeModel::Value::Vec2(20.0, 30.0));
    key.roving = true;
    key.spatial_continuous = true;
    std::vector<KeyframeModel::Keyframe> keys = { key };
    KeyframeModel::set_key(keys, KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec2(40.0, 50.0)));
    if (!keys[0].roving) return false;
    if (!keys[0].spatial_continuous) return false;
    if (keys[0].value != KeyframeModel::Value::Vec2(40.0, 50.0)) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs temporal_ease_matches_live_ae_with_overlapping_influences
static bool t_kf_temporal_ease_matches_live_ae() {
    struct Case {
        double from, to, out_speed, out_inf, in_speed, in_inf;
        std::vector<std::pair<double, double>> samples;
    };
    const Case cases[] = {
        {0.0, 100.0, 0.0, 0.8, 0.0, 0.8, {{0.1, 0.59243939036327}, {0.25, 4.76438030608764}, {0.4, 18.4284394271562}, {0.75, 95.2356196939123}}},
        {0.0, 100.0, 20.0, 0.8, 80.0, 0.6, {{0.1, 2.25754942621577}, {0.25, 6.99841889412931}, {0.4, 14.9334636959052}, {0.75, 73.8196785378389}}},
        {100.0, 0.0, -20.0, 0.6, -80.0, 0.8, {{0.1, 97.6395466334594}, {0.25, 91.597048571292}, {0.4, 74.5367957474338}, {0.75, 22.3789127753126}}}
    };
    for (const auto& c : cases) {
        KeyframeModel::Keyframe a(kf_s(0.0), KeyframeModel::Value::Scalar(c.from));
        KeyframeModel::Keyframe b(kf_s(1.0), KeyframeModel::Value::Scalar(c.to));
        a.out_interp = KeyframeModel::Interp::Bezier;
        b.in_interp = KeyframeModel::Interp::Bezier;
        a.out_ease = { KeyframeModel::Ease{c.out_speed, c.out_inf} };
        b.in_ease = { KeyframeModel::Ease{c.in_speed, c.in_inf} };
        std::vector<KeyframeModel::Keyframe> keys = { a, b };
        for (auto [t, expected] : c.samples) {
            auto val = KeyframeModel::evaluate(keys, kf_s(t), false);
            if (!val.has_value()) return false;
            if (std::fabs(val->as_f64() - expected) >= 1e-7) return false;
            double norm = KeyframeModel::ease_progress(c.out_speed / (c.to - c.from), c.out_inf,
                                                      c.in_speed / (c.to - c.from), c.in_inf, t);
            if (std::fabs((c.from + norm * (c.to - c.from)) - expected) >= 1e-7) return false;
        }
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs maximum_influence_resolves_flat_midpoint_time_curve
static bool t_kf_maximum_influence() {
    auto bez = [](double p0, double p1, double p2, double p3, double u) {
        double v = 1.0 - u;
        return v * v * v * p0 + 3.0 * v * v * u * p1 + 3.0 * v * u * u * p2 + u * u * u * p3;
    };
    for (double time : { 0.5 - 1e-9, 0.5 - 1e-12, 0.5, 0.5 + 1e-12, 0.5 + 1e-9 }) {
        double u = 0.5 + std::cbrt((time - 0.5) / 4.0);
        double expected = bez(0.0, 0.0, 1.0, 1.0, u);
        double actual = KeyframeModel::ease_progress(0.0, 1.0, 0.0, 1.0, time);
        if (std::fabs(actual - expected) >= 1e-10) return false;
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs keyed_orientation_matches_after_effects
static bool t_kf_keyed_orientation() {
    auto close = [](const KeyframeModel::Value& v, std::array<double, 3> e) {
        auto v3 = v.as_vec3();
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(v3[i] - e[i]) >= 2e-3) return false;
        }
        return true;
    };
    std::vector<KeyframeModel::Keyframe> lin = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec3(60.0, 80.0, 40.0))
    };
    auto v0_5 = KeyframeModel::evaluate_orientation(lin, kf_s(0.5));
    if (!v0_5 || !close(*v0_5, {16.9201, 16.0688, 16.0629})) return false;
    auto v1_0 = KeyframeModel::evaluate_orientation(lin, kf_s(1.0));
    if (!v1_0 || !close(*v1_0, {30.6186, 35.7617, 28.4317})) return false;
    auto v2_0 = KeyframeModel::evaluate_orientation(lin, kf_s(2.0));
    if (!v2_0 || !close(*v2_0, {60.0, 80.0, 40.0})) return false;

    // Middle key
    std::vector<KeyframeModel::Keyframe> three = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec3(0.0, 120.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0))
    };
    auto mid = KeyframeModel::evaluate_orientation(three, kf_s(1.0));
    if (!mid || *mid != KeyframeModel::Value::Vec3(0.0, 120.0, 0.0)) return false;

    // Eased
    std::vector<KeyframeModel::Keyframe> eased = lin;
    for (auto& k : eased) {
        k = k.eased();
        k.in_ease = { KeyframeModel::Ease{0.0, 0.33} };
        k.out_ease = { KeyframeModel::Ease{0.0, 0.33} };
    }
    auto ev0_5 = KeyframeModel::evaluate_orientation(eased, kf_s(0.5));
    if (!ev0_5 || !close(*ev0_5, {11.0974, 9.5750, 10.5916})) return false;
    auto ev1_0 = KeyframeModel::evaluate_orientation(eased, kf_s(1.0));
    if (!ev1_0 || !close(*ev1_0, {30.6186, 35.7617, 28.4317})) return false;
    auto ev1_5 = KeyframeModel::evaluate_orientation(eased, kf_s(1.5));
    if (!ev1_5 || !close(*ev1_5, {47.6915, 65.7590, 40.3338})) return false;

    // Held
    std::vector<KeyframeModel::Keyframe> held = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec3(1.0, 2.0, 3.0)).hold(),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0))
    };
    auto hv = KeyframeModel::evaluate_orientation(held, kf_s(0.5));
    if (!hv || !close(*hv, {1.0, 2.0, 3.0})) return false;

    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs linear_and_hold
static bool t_kf_linear_and_hold() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(100.0))
    };
    if (KeyframeModel::evaluate(keys, kf_s(1.0), false) != KeyframeModel::Value::Scalar(50.0)) return false;
    if (KeyframeModel::evaluate(keys, kf_s(-1.0), false) != KeyframeModel::Value::Scalar(0.0)) return false;
    if (KeyframeModel::evaluate(keys, kf_s(3.0), false) != KeyframeModel::Value::Scalar(100.0)) return false;

    std::vector<KeyframeModel::Keyframe> held = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)).hold(),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(100.0))
    };
    if (KeyframeModel::evaluate(held, kf_s(1.999), false) != KeyframeModel::Value::Scalar(0.0)) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs hold_key_keeps_its_incoming_segment_linear
static bool t_kf_hold_key_incoming_linear() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(100.0)).hold()
    };
    if (KeyframeModel::evaluate(keys, kf_s(0.5), false) != KeyframeModel::Value::Scalar(25.0)) return false;

    std::vector<KeyframeModel::Keyframe> spatial_keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(100.0, 0.0)).hold()
    };
    auto p = KeyframeModel::evaluate(spatial_keys, kf_s(0.5), true);
    if (!p) return false;
    auto v2 = p->as_vec2();
    if (std::fabs(v2[0] - 25.0) >= 0.01) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs easy_ease_is_symmetric_and_slow_at_ends
static bool t_kf_easy_ease() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)).eased(),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Scalar(100.0)).eased()
    };
    auto mid = KeyframeModel::evaluate(keys, kf_s(0.5), false);
    if (!mid || std::fabs(mid->as_f64() - 50.0) >= 1e-6) return false;
    auto early = KeyframeModel::evaluate(keys, kf_s(0.1), false);
    if (!early || early->as_f64() >= 10.0 || early->as_f64() <= 0.0) return false;
    double v0 = KeyframeModel::velocity(keys, kf_s(0.01), false)[0];
    double vm = KeyframeModel::velocity(keys, kf_s(0.5), false)[0];
    if (v0 >= vm) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs bezier_with_linear_speed_matches_linear
static bool t_kf_bezier_linear_speed() {
    KeyframeModel::Keyframe a(kf_s(0.0), KeyframeModel::Value::Scalar(0.0));
    KeyframeModel::Keyframe b(kf_s(2.0), KeyframeModel::Value::Scalar(100.0));
    a.out_interp = KeyframeModel::Interp::Bezier;
    b.in_interp = KeyframeModel::Interp::Bezier;
    a.out_ease = { KeyframeModel::Ease{50.0, 1.0 / 3.0} };
    b.in_ease = { KeyframeModel::Ease{50.0, 1.0 / 3.0} };
    std::vector<KeyframeModel::Keyframe> keys = { a, b };
    for (int i = 0; i <= 20; ++i) {
        double t = i * 0.1;
        auto v = KeyframeModel::evaluate(keys, kf_s(t), false);
        if (!v || std::fabs(v->as_f64() - 50.0 * t) >= 1e-6) return false;
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs a_linear_side_next_to_an_eased_side_matches_after_effects
static bool t_kf_linear_side_next_to_eased() {
    KeyframeModel::Keyframe a(kf_s(2.0), KeyframeModel::Value::Scalar(3.5));
    a.out_interp = KeyframeModel::Interp::Bezier;
    a.out_ease = { KeyframeModel::Ease{0.0, 0.5} };
    std::vector<KeyframeModel::Keyframe> keys = { a, KeyframeModel::Keyframe(kf_s(4.0), KeyframeModel::Value::Scalar(0.5)) };
    for (auto [t, ae] : std::vector<std::pair<double, double>>{{2.5, 3.290}, {3.0, 2.657}}) {
        auto v = KeyframeModel::evaluate(keys, kf_s(t), false);
        if (!v || std::fabs(v->as_f64() - ae) >= 2.0 / 455.0) return false;
    }
    // Mirror
    KeyframeModel::Keyframe b(kf_s(4.0), KeyframeModel::Value::Scalar(0.5));
    b.in_interp = KeyframeModel::Interp::Bezier;
    b.in_ease = { KeyframeModel::Ease{0.0, 0.5} };
    std::vector<KeyframeModel::Keyframe> mirror = { KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(3.5)), b };
    auto v = KeyframeModel::evaluate(mirror, kf_s(3.5), false);
    if (!v || std::fabs(v->as_f64() - (4.0 - 3.290)) >= 2.0 / 455.0) return false;
    auto se = KeyframeModel::side_ease(keys, 1, 0, false, false);
    if (!se || std::fabs(se->influence - KeyframeModel::LINEAR_SIDE_INFLUENCE) >= 1e-9) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs orientation_linear_side_next_to_an_eased_side_matches_a_scalar
static bool t_kf_orientation_linear_side() {
    std::vector<KeyframeModel::Ease> ease = { KeyframeModel::Ease{0.0, 0.5} };
    KeyframeModel::Keyframe a(kf_s(2.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0));
    KeyframeModel::Keyframe sa(kf_s(2.0), KeyframeModel::Value::Scalar(0.0));
    a.out_interp = KeyframeModel::Interp::Bezier; a.out_ease = ease;
    sa.out_interp = KeyframeModel::Interp::Bezier; sa.out_ease = ease;
    std::vector<KeyframeModel::Keyframe> orient = { a, KeyframeModel::Keyframe(kf_s(4.0), KeyframeModel::Value::Vec3(0.0, 0.0, 90.0)) };
    std::vector<KeyframeModel::Keyframe> scalar = { sa, KeyframeModel::Keyframe(kf_s(4.0), KeyframeModel::Value::Scalar(90.0)) };

    KeyframeModel::Keyframe b(kf_s(4.0), KeyframeModel::Value::Vec3(0.0, 0.0, 90.0));
    KeyframeModel::Keyframe sb(kf_s(4.0), KeyframeModel::Value::Scalar(90.0));
    b.in_interp = KeyframeModel::Interp::Bezier; b.in_ease = ease;
    sb.in_interp = KeyframeModel::Interp::Bezier; sb.in_ease = ease;
    std::vector<KeyframeModel::Keyframe> orient_m = { KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec3(0.0, 0.0, 0.0)), b };
    std::vector<KeyframeModel::Keyframe> scalar_m = { KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(0.0)), sb };

    for (auto [o, sc] : std::vector<std::pair<const std::vector<KeyframeModel::Keyframe>*, const std::vector<KeyframeModel::Keyframe>*>>{{&orient, &scalar}, {&orient_m, &scalar_m}}) {
        for (double t : {2.25, 2.5, 3.0, 3.5, 3.75}) {
            auto v = KeyframeModel::evaluate_orientation(*o, kf_s(t));
            auto want = KeyframeModel::evaluate(*sc, kf_s(t), false);
            if (!v || !want) return false;
            auto v3 = v->as_vec3();
            if (std::fabs(v3[2] - want->as_f64()) >= 1e-6 || std::fabs(v3[0]) >= 1e-9 || std::fabs(v3[1]) >= 1e-9) return false;
        }
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs spatial_linear_constant_speed
static bool t_kf_spatial_linear_constant_speed() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec2(300.0, 400.0))
    };
    auto p = KeyframeModel::evaluate(keys, kf_s(0.5), true);
    if (!p) return false;
    auto v2 = p->as_vec2();
    if (std::fabs(v2[0] - 150.0) >= 0.5 || std::fabs(v2[1] - 200.0) >= 0.5) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs spatial_curved_path_has_smooth_constant_speed
static bool t_kf_spatial_curved_speed() {
    KeyframeModel::Keyframe a(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0));
    KeyframeModel::Keyframe b(kf_s(2.0), KeyframeModel::Value::Vec2(400.0, 0.0));
    a.spatial_auto = false;
    b.spatial_auto = false;
    a.spatial_out = {50.0, -300.0, 0.0};
    b.spatial_in = {80.0, 250.0, 0.0};
    std::vector<KeyframeModel::Keyframe> keys = { a, b };
    double len = KeyframeModel::spatial_segment_length(keys, 0);
    for (int i = 1; i < 400; ++i) {
        double v = KeyframeModel::speed(keys, kf_s(i * 0.005), true);
        if (std::fabs(v - len / 2.0) >= 1e-3 * len) return false;
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs spatial_auto_bezier_passes_through_keys
static bool t_kf_spatial_auto_bezier_passes() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec2(100.0, 100.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(200.0, 0.0))
    };
    auto p = KeyframeModel::evaluate(keys, kf_s(1.0), true);
    if (!p || p->as_vec2() != std::array<double, 2>{100.0, 100.0}) return false;
    auto m = KeyframeModel::evaluate(keys, kf_s(0.5), true);
    if (!m) return false;
    auto mv = m->as_vec2();
    if (std::fabs(mv[0] - mv[1]) <= 1.0) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs straight_auto_bezier_path_is_exact
static bool t_kf_straight_auto_bezier() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(40.0, 50.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(120.0, 50.0))
    };
    if (KeyframeModel::segment_value(keys, 0, kf_s(1.0), true) != KeyframeModel::Value::Vec2(80.0, 50.0)) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs overflowing_or_nan_motion_paths_evaluate
static bool t_kf_overflowing_motion_paths() {
    const std::vector<std::vector<std::array<double, 2>>> paths = {
        {{-1.7e308, 0.0}, {1.7e308, 0.0}, {-1.7e308, 5.0}},
        {{0.0, 0.0}, {std::numeric_limits<double>::quiet_NaN(), 10.0}, {20.0, 0.0}},
        {{std::numeric_limits<double>::infinity(), 0.0}, {0.0, -std::numeric_limits<double>::infinity()}, {20.0, 0.0}}
    };
    for (const auto& path : paths) {
        std::vector<KeyframeModel::Keyframe> keys;
        for (size_t i = 0; i < path.size(); ++i) {
            keys.push_back(KeyframeModel::Keyframe(kf_s((double)i * 2.0), KeyframeModel::Value::Vec2(path[i][0], path[i][1])));
        }
        for (bool eased : {false, true}) {
            if (eased) {
                for (auto& k : keys) k = k.eased();
            }
            for (int f = 0; f <= 40; ++f) {
                Tick t = kf_s(f * 0.1);
                auto val = KeyframeModel::evaluate(keys, t, true);
                if (!val.has_value()) return false;
                KeyframeModel::speed(keys, t, true);
            }
        }
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs spatial_auto_bezier_tangents_follow_after_effects
static bool t_kf_spatial_auto_bezier_tangents() {
    const std::array<double, 3> p[4] = {
        {40.0, 300.0, 0.0}, {260.0, 80.0, 20.0}, {700.0, 420.0, -40.0}, {1100.0, 150.0, 0.0}
    };
    const double times[4] = {0.0, 0.2, 1.5, 1.8};
    std::vector<KeyframeModel::Keyframe> keys;
    for (int i = 0; i < 4; ++i) {
        keys.push_back(KeyframeModel::Keyframe(kf_s(times[i]), KeyframeModel::Value::Vec3(p[i][0], p[i][1], p[i][2])));
    }
    auto sixth = [](std::array<double, 3> a, std::array<double, 3> b) -> std::array<double, 3> {
        return {(b[0] - a[0]) / 6.0, (b[1] - a[1]) / 6.0, (b[2] - a[2]) / 6.0};
    };
    auto neg = [](std::array<double, 3> v) -> std::array<double, 3> {
        return {-v[0], -v[1], -v[2]};
    };
    auto close = [](std::array<double, 3> a, std::array<double, 3> b) {
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(a[i] - b[i]) >= 1e-9) return false;
        }
        return true;
    };
    const std::array<double, 3> want[4] = {sixth(p[0], p[1]), sixth(p[0], p[2]), sixth(p[1], p[3]), sixth(p[2], p[3])};
    for (size_t i = 0; i < 4; ++i) {
        auto [tin, tout] = KeyframeModel::spatial_tangents(keys, i);
        if (!close(tout, want[i]) || !close(tin, neg(want[i]))) return false;
    }
    keys[1].time = kf_s(1.2);
    auto t1 = KeyframeModel::spatial_tangents(keys, 1);
    if (!close(t1.second, sixth(p[0], p[2]))) return false;

    // Coincident
    std::vector<KeyframeModel::Keyframe> still = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(5.0, 5.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec2(9.0, 1.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(5.0, 5.0))
    };
    auto st1 = KeyframeModel::spatial_tangents(still, 1);
    if (st1.first != std::array<double, 3>{0,0,0} || st1.second != std::array<double, 3>{0,0,0}) return false;
    std::vector<KeyframeModel::Keyframe> lone = { still[0] };
    auto st0 = KeyframeModel::spatial_tangents(lone, 0);
    if (st0.first != std::array<double, 3>{0,0,0} || st0.second != std::array<double, 3>{0,0,0}) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs auto_spatial_holds_between_identical_keys
static bool t_kf_auto_spatial_holds() {
    const std::pair<double, std::array<double, 2>> path[4] = {
        {0.0, {0.0, 540.0}}, {0.6, {400.0, 540.0}}, {4.4, {400.0, 540.0}}, {5.0, {0.0, 540.0}}
    };
    for (bool eased : {false, true}) {
        std::vector<KeyframeModel::Keyframe> keys;
        for (const auto& pt : path) {
            auto k = KeyframeModel::Keyframe(kf_s(pt.first), KeyframeModel::Value::Vec2(pt.second[0], pt.second[1]));
            if (eased) k = k.eased();
            keys.push_back(k);
        }
        if (KeyframeModel::spatial_tangents(keys, 1).second != std::array<double, 3>{0,0,0}) return false;
        if (KeyframeModel::spatial_tangents(keys, 2).first != std::array<double, 3>{0,0,0}) return false;
        for (double t : {0.7, 1.0, 2.5, 4.0, 4.3}) {
            auto p = KeyframeModel::evaluate(keys, kf_s(t), true);
            if (!p) return false;
            auto v3 = p->as_vec3();
            if (std::fabs(v3[0] - 400.0) >= 1e-6 || std::fabs(v3[1] - 540.0) >= 1e-6) return false;
        }
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs set_key_replaces_same_time
static bool t_kf_set_key_replaces() {
    std::vector<KeyframeModel::Keyframe> keys;
    KeyframeModel::set_key(keys, KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Scalar(1.0)).eased());
    KeyframeModel::set_key(keys, KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)));
    KeyframeModel::set_key(keys, KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Scalar(5.0)));
    if (keys.size() != 2) return false;
    if (keys[1].value != KeyframeModel::Value::Scalar(5.0)) return false;
    if (keys[1].in_interp != KeyframeModel::Interp::Bezier) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs colors_and_paths_interpolate
static bool t_kf_colors_and_paths_interpolate() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Color(0.0, 0.0, 0.0, 1.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Color(1.0, 0.5, 0.0, 1.0))
    };
    auto c = KeyframeModel::evaluate(keys, kf_s(0.5), false);
    if (!c || c->as_color() != std::array<float, 4>{0.5f, 0.25f, 0.0f, 1.0f}) return false;

    auto a = KeyframeModel::ShapePath::rect({0.0, 0.0}, 10.0, 10.0);
    auto b = KeyframeModel::ShapePath::rect({0.0, 0.0}, 20.0, 20.0);
    std::vector<KeyframeModel::Keyframe> pkeys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Path(a)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Path(b))
    };
    auto v = KeyframeModel::evaluate(pkeys, kf_s(0.5), false);
    if (!v || !v->as_path()) return false;
    if (std::fabs(v->as_path()->vertices[0][0] - (-7.5)) > 1e-6 ||
        std::fabs(v->as_path()->vertices[0][1] - (-7.5)) > 1e-6) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs roving_keys_get_constant_speed_times
static bool t_kf_roving_keys_constant_speed() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(0.2), KeyframeModel::Value::Vec2(300.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(400.0, 0.0))
    };
    for (auto& k : keys) k.spatial_auto = false;
    keys[1].roving = true;
    if (!KeyframeModel::retime_roving(keys, true)) return false;
    if (std::fabs(keys[1].time.seconds() - 1.5) >= 1e-6) return false;
    double v0 = KeyframeModel::speed(keys, kf_s(0.5), true);
    double v1 = KeyframeModel::speed(keys, kf_s(1.8), true);
    if (std::fabs(v0 - 200.0) >= 1.0 || std::fabs(v1 - 200.0) >= 1.0) return false;
    // First/last keys can't rove
    keys[0].roving = true;
    KeyframeModel::retime_roving(keys, true);
    if (keys[0].roving) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs roving_keys_never_share_a_time_with_a_fixed_key
static bool t_kf_roving_keys_never_share_time() {
    const double xs_cases[2][4] = {
        {0.0, 40.0, 120.0, 120.0},
        {0.0, 0.0, 40.0, 120.0}
    };
    for (const auto& xs : xs_cases) {
        std::vector<KeyframeModel::Keyframe> keys;
        for (int i = 0; i < 4; ++i) {
            keys.push_back(KeyframeModel::Keyframe(kf_s(i), KeyframeModel::Value::Vec2(xs[i], 64.0)));
        }
        for (auto& k : keys) k.spatial_auto = false;
        keys[1].roving = true;
        keys[2].roving = true;
        KeyframeModel::retime_roving(keys, true);
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (keys[i].time.v >= keys[i + 1].time.v) return false;
        }
    }
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs side_ease_reports_linear_and_eased_sides
static bool t_kf_side_ease() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(100.0)).eased()
    };
    auto out = KeyframeModel::side_ease(keys, 0, 0, false, true);
    if (!out || std::fabs(out->speed - 50.0) >= 1e-9) return false;
    auto inn = KeyframeModel::side_ease(keys, 1, 0, false, false);
    if (!inn || *inn != KeyframeModel::Ease::EASY) return false;
    if (KeyframeModel::side_ease(keys, 0, 0, false, false).has_value()) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs time_reverse_mirrors_keys
static bool t_kf_time_reverse_mirrors() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Scalar(0.0)).eased(),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(5.0)),
        KeyframeModel::Keyframe(kf_s(4.0), KeyframeModel::Value::Scalar(10.0))
    };
    keys[0].out_interp = KeyframeModel::Interp::Hold;
    KeyframeModel::time_reverse(keys);
    if (std::llround(keys[0].time.seconds()) != 1) return false;
    if (std::llround(keys[1].time.seconds()) != 3) return false;
    if (std::llround(keys[2].time.seconds()) != 4) return false;
    if (keys[0].value != KeyframeModel::Value::Scalar(10.0)) return false;
    if (keys[2].in_interp != KeyframeModel::Interp::Hold) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs muted_keys_are_skipped_by_evaluation
static bool t_kf_muted_keys_skipped() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Scalar(0.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Scalar(100.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Scalar(300.0))
    };
    auto at = [&](double t) { return KeyframeModel::evaluate(keys, kf_s(t), false)->as_f64(); };
    if (std::fabs(at(0.5) - 50.0) >= 1e-9) return false;
    if (std::fabs(at(1.5) - 200.0) >= 1e-9) return false;
    keys[1].muted = true;
    if (std::fabs(at(0.5) - 75.0) >= 1e-9) return false;
    if (std::fabs(at(1.0) - 150.0) >= 1e-9) return false;
    if (std::fabs(at(1.5) - 225.0) >= 1e-9) return false;
    for (auto& k : keys) k.muted = true;
    if (KeyframeModel::evaluate(keys, kf_s(0.5), false).has_value()) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs muted_key_automation_uses_the_active_neighbours
static bool t_kf_muted_key_automation() {
    std::vector<KeyframeModel::Keyframe> keys = {
        KeyframeModel::Keyframe(kf_s(0.0), KeyframeModel::Value::Vec2(0.0, 0.0)),
        KeyframeModel::Keyframe(kf_s(1.0), KeyframeModel::Value::Vec2(50.0, 100.0)),
        KeyframeModel::Keyframe(kf_s(2.0), KeyframeModel::Value::Vec2(100.0, 0.0))
    };
    for (auto& k : keys) {
        k.auto_bezier = true;
        k.in_interp = KeyframeModel::Interp::Bezier;
        k.out_interp = KeyframeModel::Interp::Bezier;
    }
    auto all = keys;
    auto active = keys;
    active.erase(active.begin() + 1);
    keys[1].muted = true;
    for (double t : {0.0, 0.25, 0.5, 1.0, 1.5, 1.75, 2.0}) {
        if (KeyframeModel::evaluate(keys, kf_s(t), true) != KeyframeModel::evaluate(active, kf_s(t), true)) return false;
    }
    if (KeyframeModel::evaluate(keys, kf_s(0.5), true) == KeyframeModel::evaluate(all, kf_s(0.5), true)) return false;
    return true;
}

// after EffectCraft crates/keyframe/src/tests.rs gradient_tracks_sample_separately
static bool t_kf_gradient_tracks_sample_separately() {
    KeyframeModel::Gradient g;
    g.colors = {{0.0, {1.0f, 0.0f, 0.0f, 0.8f}}, {1.0, {0.0f, 0.0f, 1.0f, 0.4f}}};
    g.opacities = {{0.0, 0.5f}, {1.0, 0.5f}};
    auto c = g.color_at(0.5);
    if (std::fabs(c[0] - 0.5f) > 1e-5f || std::fabs(c[2] - 0.5f) > 1e-5f) return false;
    if (std::fabs(c[3] - 0.6f) > 1e-5f) return false;
    if (std::fabs(g.opacity_at(0.5) - 0.5f) > 1e-5f) return false;
    if (std::fabs(g.sample(0.5)[3] - 0.3f) > 1e-5f) return false;

    auto with_stop = g;
    with_stop.opacities.insert(with_stop.opacities.begin() + 1, {0.5, g.opacity_at(0.5)});
    if (std::fabs(with_stop.opacity_at(0.5) - 0.5f) > 1e-5f) return false;
    if (std::fabs(with_stop.sample(0.5)[3] - 0.3f) > 1e-5f) return false;

    auto c_neg = g.color_at(-1.0);
    if (c_neg[0] != 1.0f || c_neg[1] != 0.0f || c_neg[2] != 0.0f || c_neg[3] != 0.8f) return false;
    auto c_pos = g.color_at(2.0);
    if (c_pos[0] != 0.0f || c_pos[1] != 0.0f || c_pos[2] != 1.0f || c_pos[3] != 0.4f) return false;

    KeyframeModel::Gradient empty;
    auto ec = empty.color_at(0.3);
    auto eo = empty.opacity_at(0.3);
    if (ec[0] != 1.0f || ec[1] != 1.0f || ec[2] != 1.0f || ec[3] != 1.0f || eo != 1.0f) return false;

    return true;
}

// =====================================================================
// Icons tests (after EffectCraft crates/ui-egui/src/icons.rs)
// =====================================================================
static bool t_icons_concave_glyphs_are_filled_as_convex_pieces() {
    int gCount = 0;
    const auto* glyphs = Icons::getConcaveGlyphs(&gCount);
    for (int g = 0; g < gCount; ++g) {
        const auto& glyph = glyphs[g];
        for (int p = 0; p < glyph.count; ++p) {
            const auto& piece = glyph.pieces[p];
            int n = piece.count;
            if (n < 3) return false;
            std::vector<float> turns(n);
            for (int i = 0; i < n; ++i) {
                ImVec2 a = piece.pts[i];
                ImVec2 b = piece.pts[(i + 1) % n];
                ImVec2 c = piece.pts[(i + 2) % n];
                turns[i] = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            }
            bool allPos = true, allNeg = true;
            for (float t : turns) {
                if (t <= 0.0f) allPos = false;
                if (t >= 0.0f) allNeg = false;
            }
            if (!allPos && !allNeg) return false;
        }
    }
    return true;
}

static bool t_icons_keyframe_bounds_follow_each_half() {
    using Icons::KeyHalf;
    ImVec2 c(100.0f, 50.0f);
    float r = 5.5f;
    struct Extent { KeyHalf h; float lx; float ly; };
    const Extent extents[] = {
        { KeyHalf::Linear, r, r },
        { KeyHalf::Hold,   r * 0.8f, r * 0.8f },
        { KeyHalf::Bezier, r * 0.75f, r },
        { KeyHalf::Auto,   r * 0.8f, r * 0.8f }
    };
    for (const auto& left : extents) {
        for (const auto& right : extents) {
            auto b = Icons::keyframeBounds(c, 11.0f, left.h, right.h);
            float maxLy = std::max(left.ly, right.ly);
            ImVec2 wantMin(c.x - left.lx - 0.25f, c.y - maxLy - 0.25f);
            ImVec2 wantMax(c.x + right.lx + 0.25f, c.y + maxLy + 0.25f);
            if (std::fabs(b.min.x - wantMin.x) > 1e-2f || std::fabs(b.min.y - wantMin.y) > 1e-2f ||
                std::fabs(b.max.x - wantMax.x) > 1e-2f || std::fabs(b.max.y - wantMax.y) > 1e-2f)
                return false;
        }
    }
    return true;
}

static bool t_icons_keyframes_are_drawn_without_a_dark_outline() {
    using Icons::KeyHalf;
    const std::pair<KeyHalf, KeyHalf> pairs[] = {
        { KeyHalf::Linear, KeyHalf::Linear },
        { KeyHalf::Bezier, KeyHalf::Hold },
        { KeyHalf::Auto,   KeyHalf::Linear }
    };
    for (const auto& p : pairs) {
        auto shapes = Icons::keyframeShapes(ImVec2(10.0f, 10.0f), 11.0f, p.first, p.second);
        if (shapes.size() != 2) return false;
        for (const auto& s : shapes) {
            if (s.size() < 3) return false;
        }
    }
    return true;
}

// =====================================================================
// COLOR & BLEND TESTS (after EffectCraft crates/color/src/blend.rs)
// =====================================================================

static inline Raster::Px test_opaque(float v) {
    return {v, v, v, 1.0f};
}

// after EffectCraft crates/color/src/blend.rs test opaque_formulas
static bool t_blend_opaque_formulas() {
    auto d = test_opaque(0.6f);
    auto s = test_opaque(0.3f);
    auto r = [&](Raster::BlendMode m) { return Raster::blend_pixel(m, d, s, 0.5f)[0]; };
    if (std::abs(r(Raster::BlendMode::Normal) - 0.3f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Multiply) - 0.18f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Screen) - (0.6f + 0.3f - 0.18f)) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Add) - 0.9f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Difference) - 0.3f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Darken) - 0.3f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Lighten) - 0.6f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Subtract) - 0.3f) >= 1e-6f) return false;
    if (!(std::abs(r(Raster::BlendMode::Divide) - 1.0f) < 1e-6f || r(Raster::BlendMode::Divide) > 1.0f)) return false;
    if (std::abs(r(Raster::BlendMode::LinearBurn) - 0.0f) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Exclusion) - (0.9f - 0.36f)) >= 1e-6f) return false;
    if (std::abs(r(Raster::BlendMode::Overlay) - (0.3f + 0.2f - 0.06f)) >= 1e-6f) return false;
    return true;
}

// after EffectCraft crates/color/src/blend.rs test transparent_source_keeps_destination
static bool t_blend_transparent_source_keeps_destination() {
    Raster::Px d = {0.2f, 0.3f, 0.4f, 0.8f};
    for (int i = 0; i <= (int)Raster::BlendMode::LuminescentPremul; ++i) {
        Raster::BlendMode m = (Raster::BlendMode)i;
        if (Raster::blend_mode_is_stencil(m)) continue;
        auto o = Raster::blend_pixel(m, d, {0.0f, 0.0f, 0.0f, 0.0f}, 0.5f);
        for (int c = 0; c < 4; ++c) {
            if (std::abs(o[c] - d[c]) >= 1e-6f) return false;
        }
    }
    return true;
}

// after EffectCraft crates/color/src/blend.rs test source_over_transparent_destination
static bool t_blend_source_over_transparent_destination() {
    Raster::Px s = {0.25f, 0.1f, 0.05f, 0.5f};
    Raster::BlendMode modes[] = {Raster::BlendMode::Normal, Raster::BlendMode::Multiply, Raster::BlendMode::Screen, Raster::BlendMode::Hue, Raster::BlendMode::Overlay};
    for (auto m : modes) {
        auto o = Raster::blend_pixel(m, {0.0f, 0.0f, 0.0f, 0.0f}, s, 0.9f);
        for (int c = 0; c < 4; ++c) {
            if (std::abs(o[c] - s[c]) >= 1e-6f) return false;
        }
    }
    return true;
}

// after EffectCraft crates/color/src/blend.rs test stencil
static bool t_blend_stencil() {
    Raster::Px d = {0.5f, 0.5f, 0.5f, 1.0f};
    auto zero = Raster::blend_pixel(Raster::BlendMode::StencilAlpha, d, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f);
    if (zero != Raster::Px{0.0f, 0.0f, 0.0f, 0.0f}) return false;
    auto sil = Raster::blend_pixel(Raster::BlendMode::SilhouetteAlpha, d, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f);
    if (sil != d) return false;
    return true;
}

// after EffectCraft crates/color/src/blend.rs test hdr_modes
static bool t_blend_hdr_modes() {
    Raster::Px d = {2.0f, 2.0f, 2.0f, 1.0f};
    Raster::Px s = {0.5f, 0.5f, 0.5f, 1.0f};
    if (std::abs(Raster::blend_pixel(Raster::BlendMode::Add, d, s, 0.5f)[0] - 2.5f) >= 1e-6f) return false;
    if (std::abs(Raster::blend_pixel(Raster::BlendMode::Screen, d, s, 0.5f)[0] - 1.5f) >= 1e-6f) return false;
    if (Raster::blend_pixel(Raster::BlendMode::SoftLight, d, s, 0.5f)[0] > 1.0f) return false;
    return true;
}

// after EffectCraft crates/color/src/blend.rs test after_effects_32bpc_formulas
static bool t_blend_ae_32bpc_formulas() {
    auto at = [](Raster::BlendMode m, float b, float o, float k) {
        return Raster::blend_pixel(m, test_opaque(b), {o * k, o * k, o * k, k}, 0.5f)[0];
    };
    auto close = [](float a, float e) { return std::abs(a - e) < 1e-5f; };
    std::pair<float, float> pairs[] = {{0.4f, 0.5f}, {0.8f, 0.9f}, {0.1f, 0.3f}};
    for (auto [b, o] : pairs) {
        float k = 0.15f;
        if (!close(at(Raster::BlendMode::ClassicColorDodge, b, o, k), std::min(1.0f, b / (1.0f - k * o)))) return false;
        if (!close(at(Raster::BlendMode::ClassicDifference, b, o, k), std::abs(b - k * o))) return false;
        if (!close(at(Raster::BlendMode::ClassicColorBurn, b, o, k), 1.0f - (1.0f - b) / (1.0f - k * (1.0f - o)))) return false;
    }
    if (!close(at(Raster::BlendMode::ClassicDifference, 0.6f, 0.3f, 1.0f), 0.3f)) return false;
    float b = 0.7f, o = 3.0f;
    if (!close(at(Raster::BlendMode::Exclusion, b, o, 1.0f), b + o - 2.0f * b * o)) return false;
    float s = 2.0f * b - 1.0f;
    if (!close(at(Raster::BlendMode::Overlay, b, o, 1.0f), o + s - o * s)) return false;
    if (!close(at(Raster::BlendMode::ColorBurn, 0.0f, 1.35f, 0.15f), 0.15f * (1.0f - 1.0f / 1.35f))) return false;
    if (!close(at(Raster::BlendMode::ClassicColorBurn, 0.0f, 0.1f, 1.0f), -9.0f)) return false;
    auto l = Raster::blend_pixel(Raster::BlendMode::Luminosity, {0.2f, 0.4f, 0.6f, 1.0f}, {2.0f, 2.0f, 2.0f, 1.0f}, 0.5f);
    auto e = Raster::set_lum({0.2f, 0.4f, 0.6f}, 2.0f);
    for (int i = 0; i < 3; ++i) {
        if (!close(l[i], e[i])) return false;
    }
    if (std::abs(l[0] - 1.0f) <= 0.1f) return false;
    return true;
}

// after EffectCraft crates/color/src/blend.rs test names_roundtrip
static bool t_blend_names_roundtrip() {
    for (int i = 0; i <= (int)Raster::BlendMode::LuminescentPremul; ++i) {
        Raster::BlendMode m = (Raster::BlendMode)i;
        auto name = Raster::blend_mode_label(m);
        auto opt = Raster::blend_mode_from_name(name);
        if (!opt.has_value() || *opt != m) return false;
    }
    if (Raster::blend_mode_from_name("soft-light") != Raster::BlendMode::SoftLight) return false;
    return true;
}

// =====================================================================
// RASTER TESTS (after EffectCraft crates/raster/src/lib.rs)
// =====================================================================

static inline uint32_t f32_to_bits(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(float));
    return u;
}

// after EffectCraft crates/raster/src/lib.rs test u8_levels_expand_to_the_quantised_pixel
static bool t_raster_u8_levels_expand() {
    std::vector<float> values;
    for (int i = 0; i <= 255 * 4; ++i) values.push_back((float)i / (255.0f * 4.0f));
    values.push_back(-0.0f);
    values.push_back(-1.0f);
    values.push_back(1.0f);
    values.push_back(1.5f);
    values.push_back(std::numeric_limits<float>::quiet_NaN());
    values.push_back(std::numeric_limits<float>::infinity());
    values.push_back(0.5f / 255.0f);
    values.push_back(254.5f / 255.0f);
    float alphas[] = {1.0f, 0.5f, 0.2f, 0.0f, 2.0f, std::numeric_limits<float>::quiet_NaN()};
    for (float a : alphas) {
        for (float v : values) {
            Raster::Px p = {v, 1.0f - v, v * 0.5f, a};
            Raster::Px want = p;
            Raster::quantize_px(want, 255.0f, 1.0f / 255.0f);
            auto got = Raster::expand_u8(Raster::quantize_px_u8(p));
            for (int c = 0; c < 4; ++c) {
                if (f32_to_bits(got[c]) != f32_to_bits(want[c])) return false;
            }
        }
    }
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test dither_keeps_exact_codes_and_averages_the_rest
static bool t_raster_dither_exact_and_average() {
    for (uint32_t code = 0; code <= 255; ++code) {
        float v = (float)code / 255.0f;
        std::pair<uint32_t, uint32_t> pts[] = {{0, 0}, {1, 0}, {17, 3}, {1919, 1079}, {4095, 4095}};
        for (auto [x, y] : pts) {
            if ((uint32_t)Raster::dither_u8(v, x, y) != code) return false;
        }
    }
    if (Raster::dither_u8(-3.0f, 5, 5) != 0) return false;
    if (Raster::dither_u8(7.0f, 5, 5) != 255) return false;
    if (Raster::dither_u8(std::numeric_limits<float>::quiet_NaN(), 5, 5) != 0) return false;
    float v = (100.0f + 0.3f) / 255.0f;
    uint32_t n = 64;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n * n; ++i) sum += (uint32_t)Raster::dither_u8(v, i % n, i / n);
    float mean = (float)sum / (float)(n * n);
    if (std::abs(mean - 100.3f) >= 0.03f) return false;
    if (Raster::dither_u8(v, 9, 4) != Raster::dither_u8(v, 9, 4)) return false;
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test dithered_conversions_match_the_plain_ones_on_exact_codes
static bool t_raster_dithered_conversions_match() {
    std::vector<uint8_t> src = {255, 128, 0, 255, 10, 20, 30, 255, 0, 0, 0, 255, 1, 2, 3, 255};
    auto img = Raster::Image::from_rgba8(2, 2, src);
    if (img.to_rgba8_dithered() != img.to_rgba8()) return false;
    std::array<float, 3> bg = {0.0f, 0.0f, 0.0f};
    if (img.to_rgba8_over_dithered(bg) != img.to_rgba8_over(bg)) return false;
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test rgba8_roundtrip
static bool t_raster_rgba8_roundtrip() {
    std::vector<uint8_t> src = {255, 128, 0, 255, 10, 20, 30, 128, 0, 0, 0, 0, 1, 2, 3, 4};
    auto img = Raster::Image::from_rgba8(2, 2, src);
    auto back = img.to_rgba8();
    for (size_t i = 0; i < 4; ++i) {
        const uint8_t* a = &src[i * 4];
        const uint8_t* b = &back[i * 4];
        if (a[3] >= 128) {
            for (int c = 0; c < 4; ++c) {
                if (std::abs((int)a[c] - (int)b[c]) > 1) return false;
            }
        } else {
            if (std::abs((int)a[3] - (int)b[3]) > 1) return false;
        }
    }
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test bilinear_at_centres_is_exact
static bool t_raster_bilinear_centres() {
    Raster::Image img(3, 3);
    Raster::Px p = {0.5f, 0.25f, 0.125f, 1.0f};
    img.set(1, 1, p);
    if (img.sample_bilinear(1.5, 1.5) != p) return false;
    auto half = img.sample_bilinear(2.0, 1.5);
    if (std::abs(half[3] - 0.5f) >= 1e-6f) return false;
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test alpha_bounds_found
static bool t_raster_alpha_bounds() {
    Raster::Image img(10, 10);
    img.set(3, 4, {1.0f, 1.0f, 1.0f, 1.0f});
    img.set(6, 7, {1.0f, 1.0f, 1.0f, 1.0f});
    auto b = img.alpha_bounds();
    if (!b.has_value()) return false;
    if (std::get<0>(*b) != 3 || std::get<1>(*b) != 4 || std::get<2>(*b) != 7 || std::get<3>(*b) != 8) return false;
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test noise_in_range
static bool t_raster_noise_in_range() {
    for (uint32_t i = 0; i < 1000; ++i) {
        float n = Raster::hash_noise(i, i * 7, 3);
        if (n < 0.0f || n >= 1.0f) return false;
    }
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test padded_bounds_hostile_pad_at_the_api
static bool t_raster_padded_bounds_hostile() {
    Raster::Image img(29990, 2);
    img.set(29989, 1, {1.0f, 0.0f, 0.0f, 1.0f});
    auto out = img.padded(std::numeric_limits<uint32_t>::max());
    if (out.width != Raster::MAX_PADDED_SIDE || out.height != 12) return false;
    if (out.get(29994, 6) != Raster::Px{1.0f, 0.0f, 0.0f, 1.0f}) return false;
    if (out.get(0, 0) != Raster::Px{0.0f, 0.0f, 0.0f, 0.0f}) return false;
    return true;
}

// after EffectCraft crates/raster/src/lib.rs test padded_leaves_legit_pads_untouched
static bool t_raster_padded_legit() {
    Raster::Image img(8, 6);
    img.set(3, 2, {1.0f, 0.0f, 0.0f, 1.0f});
    auto out = img.padded(4);
    if (out.width != 16 || out.height != 14) return false;
    if (out.get(7, 6) != Raster::Px{1.0f, 0.0f, 0.0f, 1.0f}) return false;
    return true;
}

// =====================================================================
// WARP TESTS (after EffectCraft crates/raster/src/warp.rs)
// =====================================================================

// after EffectCraft crates/raster/src/warp.rs test identity_warp_copies
static bool t_warp_identity_copies() {
    Raster::Image src(4, 4);
    src.set(1, 2, {0.5f, 0.5f, 0.5f, 1.0f});
    Raster::Image dst(4, 4);
    Raster::composite_warp(dst, src, Raster::Mat3::IDENTITY);
    if (dst.get(1, 2) != Raster::Px{0.5f, 0.5f, 0.5f, 1.0f}) return false;
    if (dst.get(0, 0) != Raster::Px{0.0f, 0.0f, 0.0f, 0.0f}) return false;
    return true;
}

// after EffectCraft crates/raster/src/warp.rs test translate_by_integer
static bool t_warp_translate_by_integer() {
    Raster::Image src(4, 4);
    src.set(0, 0, {1.0f, 1.0f, 1.0f, 1.0f});
    Raster::Image dst(8, 8);
    Raster::composite_warp(dst, src, Raster::Mat3::translate(Raster::vec2(3.0, 2.0)));
    if (dst.get(3, 2) != Raster::Px{1.0f, 1.0f, 1.0f, 1.0f}) return false;
    return true;
}

static Raster::Image make_test_image(uint32_t w, uint32_t h, uint32_t seed) {
    Raster::Image img(w, h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float a = ((x + y) % 7 == 0) ? 0.0f : Raster::hash_noise(x, y, seed);
            img.set(x, y, {
                Raster::hash_noise(x, y, seed + 1) * a,
                Raster::hash_noise(x, y, seed + 2) * a,
                Raster::hash_noise(x, y, seed + 3) * a,
                a
            });
        }
    }
    return img;
}

static Raster::Image make_sparse_image(uint32_t w, uint32_t h) {
    Raster::Image img(w, h);
    float cx = (float)w / 2.0f, cy = (float)h / 2.0f;
    float r = (float)std::min(w, h) * 0.4f;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float d = std::abs(std::hypot(dx, dy) - r);
            if (d < 1.5f) {
                float a = 1.0f - d / 1.5f;
                img.set(x, y, {0.2f * a, 0.6f * a, a, a});
            }
        }
    }
    img.set(3, 2, {0.5f, 0.5f, 0.5f, 0.5f});
    img.set(w - 1, h - 1, {1.0f, 1.0f, 1.0f, 1.0f});
    img.set(w / 2, 1, {0.0f, 0.0f, 0.0f, -0.0f});
    return img;
}

// after EffectCraft crates/raster/src/warp.rs test fast_path_matches_reference
static bool t_warp_fast_matches_reference() {
    auto src = make_test_image(37, 23, 5);
    Raster::Mat3 mats[] = {
        Raster::Mat3::IDENTITY,
        Raster::Mat3::translate(Raster::vec2(5.0, -3.0)),
        Raster::Mat3::translate(Raster::vec2(-40.0, 2.0)),
        Raster::Mat3::translate(Raster::vec2(3.25, 7.5)),
        Raster::Mat3::translate(Raster::vec2(20.0, 10.0)) * Raster::Mat3::rotate_deg(23.0) * Raster::Mat3::scale(Raster::vec2(1.3, 0.8)),
        Raster::Mat3::translate(Raster::vec2(30.0, 5.0)) * Raster::Mat3::rotate_deg(-120.0) * Raster::Mat3::scale(Raster::vec2(0.7, 0.7)),
        Raster::Mat3::translate(Raster::vec2(10.0, 10.0)) * Raster::Mat3::scale(Raster::vec2(-1.0, 1.0)),
        Raster::Mat3::scale(Raster::vec2(0.3, 0.3)),
    };
    Raster::BlendMode modes[] = {
        Raster::BlendMode::Normal, Raster::BlendMode::Add, Raster::BlendMode::Multiply,
        Raster::BlendMode::Screen, Raster::BlendMode::Dissolve, Raster::BlendMode::Overlay
    };
    for (const auto& m : mats) {
        for (auto mode : modes) {
            for (auto sampling : {Raster::Sampling::Nearest, Raster::Sampling::Bilinear, Raster::Sampling::Bicubic}) {
                for (float opacity : {1.0f, 0.6f}) {
                    Raster::WarpOpts opts;
                    opts.sampling = sampling;
                    opts.opacity = opacity;
                    opts.mode = mode;
                    opts.seed = 9;
                    auto a = make_test_image(48, 40, 77);
                    auto b = a;
                    Raster::composite_warp(a, src, m, opts);
                    Raster::composite_warp_reference(b, src, m, opts);
                    for (size_t i = 0; i < a.data.size(); ++i) {
                        for (int c = 0; c < 4; ++c) {
                            if (std::abs(a.data[i][c] - b.data[i][c]) >= 1e-4f) return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

// after EffectCraft crates/raster/src/warp.rs test clear_tiles_hold_only_transparent_taps
static bool t_warp_clear_tiles() {
    auto img = make_sparse_image(150, 97);
    Raster::ClearTiles clear(img);
    size_t skipped = 0;
    for (size_t y = 0; y < img.height - 1; ++y) {
        for (size_t x = 0; x < img.width - 1; ++x) {
            if (clear.is_clear(x, y)) {
                ++skipped;
                for (int dy : {0, 1}) {
                    for (int dx : {0, 1}) {
                        if (img.get((int64_t)x + dx, (int64_t)y + dy)[3] != 0.0f) return false;
                    }
                }
            }
        }
    }
    return skipped > 100 * 70;
}

// after EffectCraft crates/raster/src/warp.rs test changed_rect_covers_the_changed_pixels
static bool t_warp_changed_rect() {
    auto src = make_sparse_image(90, 70);
    Raster::Mat3 mats[] = {
        Raster::Mat3::translate(Raster::vec2(7.0, 3.0)),
        Raster::Mat3::translate(Raster::vec2(10.25, 4.5)),
        Raster::Mat3::translate(Raster::vec2(60.0, 50.0)) * Raster::Mat3::rotate_deg(33.0) * Raster::Mat3::scale(Raster::vec2(0.9, 1.2)) * Raster::Mat3::translate(Raster::vec2(-45.0, -35.0)),
        Raster::Mat3::translate(Raster::vec2(-30.0, -20.0)),
        Raster::Mat3::scale(Raster::vec2(0.3, 0.3)),
    };
    for (const auto& m : mats) {
        for (auto mode : {Raster::BlendMode::Normal, Raster::BlendMode::Add, Raster::BlendMode::Dissolve}) {
            Raster::WarpOpts opts;
            opts.mode = mode;
            opts.seed = 3;
            auto before = make_test_image(120, 100, 4);
            auto a = before;
            auto r = Raster::composite_warp_changed(a, src, m, opts, std::nullopt);
            if (!r.has_value()) return false;
            uint32_t x0 = (*r)[0], y0 = (*r)[1], x1 = (*r)[2], y1 = (*r)[3];
            for (size_t i = 0; i < a.data.size(); ++i) {
                uint32_t x = (uint32_t)i % a.width;
                uint32_t y = (uint32_t)i / a.width;
                if (a.data[i] != before.data[i]) {
                    if (!(x >= x0 && x < x1 && y >= y0 && y < y1)) return false;
                }
            }
            auto b = before;
            Raster::composite_warp_reference(b, src, m, opts);
            for (size_t i = 0; i < a.data.size(); ++i) {
                for (int c = 0; c < 4; ++c) {
                    if (std::abs(a.data[i][c] - b.data[i][c]) >= 1e-4f) return false;
                }
            }
        }
    }
    Raster::Image dst(10, 10);
    Raster::Image dummySrc(4, 4);
    Raster::WarpOpts def;
    auto r0 = Raster::composite_warp_changed(dst, dummySrc, Raster::Mat3::IDENTITY, def, std::nullopt);
    if (!r0.has_value() || (*r0)[0] != std::numeric_limits<uint32_t>::max() || (*r0)[1] != std::numeric_limits<uint32_t>::max() || (*r0)[2] != 0 || (*r0)[3] != 0) return false;
    Raster::WarpOpts stencil;
    stencil.mode = Raster::BlendMode::StencilAlpha;
    auto r_stencil = Raster::composite_warp_changed(dst, src, Raster::Mat3::IDENTITY, stencil, 255.0f);
    if (r_stencil.has_value()) return false;
    return true;
}

// after EffectCraft crates/raster/src/warp.rs test quantize_while_blending_matches_quantizing_after
static bool t_warp_quantize_matches() {
    auto src = make_sparse_image(90, 70);
    Raster::Mat3 mats[] = {
        Raster::Mat3::translate(Raster::vec2(7.0, 3.0)),
        Raster::Mat3::translate(Raster::vec2(60.0, 50.0)) * Raster::Mat3::rotate_deg(33.0) * Raster::Mat3::translate(Raster::vec2(-45.0, -35.0))
    };
    for (const auto& m : mats) {
        for (auto mode : {Raster::BlendMode::Normal, Raster::BlendMode::Screen, Raster::BlendMode::Dissolve}) {
            std::pair<float, float> levels_op[] = {{255.0f, 1.0f}, {32768.0f, 0.7f}};
            for (auto [levels, opacity] : levels_op) {
                auto before = make_test_image(120, 100, 8);
                for (auto& p : before.data) Raster::quantize_px(p, levels, 1.0f / levels);
                Raster::WarpOpts opts;
                opts.mode = mode;
                opts.opacity = opacity;
                opts.seed = 5;
                auto after = before;
                auto r = Raster::composite_warp_changed(after, src, m, opts, std::nullopt);
                if (!r.has_value()) return false;
                uint32_t x0 = (*r)[0], y0 = (*r)[1], x1 = (*r)[2], y1 = (*r)[3];
                for (uint32_t y = y0; y < y1; ++y) {
                    for (uint32_t x = x0; x < x1; ++x) {
                        size_t i = after.idx(x, y);
                        Raster::quantize_px(after.data[i], levels, 1.0f / levels);
                    }
                }
                auto fused = before;
                auto rf = Raster::composite_warp_changed(fused, src, m, opts, levels);
                if (rf != r) return false;
                for (size_t i = 0; i < fused.data.size(); ++i) {
                    for (int c = 0; c < 4; ++c) {
                        if (f32_to_bits(fused.data[i][c]) != f32_to_bits(after.data[i][c])) return false;
                    }
                }
            }
        }
    }
    return true;
}

// after EffectCraft crates/raster/src/warp.rs test accumulate_matches_per_sample_images
static bool t_warp_accumulate() {
    auto src = make_test_image(30, 20, 11);
    std::array<Raster::Vec2, 4> quad = {Raster::vec2(5.0, 4.0), Raster::vec2(40.0, 2.0), Raster::vec2(44.0, 30.0), Raster::vec2(2.0, 35.0)};
    auto persp = Raster::Mat3::square_to_quad(quad) * Raster::Mat3::scale(Raster::vec2(1.0 / 30.0, 1.0 / 20.0));
    Raster::Mat3 mats[] = {
        Raster::Mat3::translate(Raster::vec2(3.0, 4.0)),
        Raster::Mat3::translate(Raster::vec2(5.5, 4.25)) * Raster::Mat3::rotate_deg(12.0),
        persp
    };
    float k = 1.0f / 3.0f;
    Raster::Image fast(48, 40);
    Raster::Image slow(48, 40);
    for (const auto& m : mats) {
        Raster::accumulate_warp(fast, src, m, Raster::Sampling::Bilinear, k);
        Raster::Image img(48, 40);
        Raster::composite_warp_reference(img, src, m);
        for (size_t i = 0; i < slow.data.size(); ++i) {
            for (int c = 0; c < 4; ++c) slow.data[i][c] += img.data[i][c] * k;
        }
    }
    for (size_t i = 0; i < fast.data.size(); ++i) {
        for (int c = 0; c < 4; ++c) {
            if (std::abs(fast.data[i][c] - slow.data[i][c]) >= 1e-4f) return false;
        }
    }
    return true;
}

// after EffectCraft crates/raster/src/warp.rs test downscale_preserves_average
static bool t_warp_downscale() {
    auto src = Raster::Image::filled(64, 64, {0.25f, 0.5f, 0.75f, 1.0f});
    auto out = Raster::resample(src, 8, 8);
    auto p = out.get(4, 4);
    if (std::abs(p[1] - 0.5f) >= 1e-4f || std::abs(p[3] - 1.0f) >= 1e-4f) return false;
    return true;
}

// =====================================================================
// RENDER TESTS (after EffectCraft crates/render/src/tests.rs)
// =====================================================================

// after EffectCraft crates/ui-egui/src/panels/timeline.rs fit_pps
static bool t_timeline_fit_and_zoom() {
    const double fit = Timeline::fitPps(510.0, 10.0);
    if (std::abs(fit - 50.0) > 1e-9) return false;
    double start = 0.0, pps = 0.0;
    Timeline::setZoom(start, pps, 100.0, fit, 10.0, 5.0, 100.0);
    if (std::abs(pps - 100.0) > 1e-9 || std::abs(start - 4.0) > 1e-9) return false;
    Timeline::setZoom(start, pps, fit, fit, 10.0, 5.0, 100.0);
    if (pps != 0.0 || start != 0.0) return false;
    return Timeline::fitPps(0.0, 0.0) == 1.0;
}

// after EffectCraft crates/ui-egui/src/panels/timeline/tests.rs the_view_pages_to_the_cti
static bool t_timeline_pages_to_cti() {
    if (Timeline::pageToCti(10.0, 5.0, 60.0, 12.0) != 10.0) return false;
    if (Timeline::pageToCti(10.0, 5.0, 60.0, 16.0) != 16.0) return false;
    if (Timeline::pageToCti(10.0, 5.0, 60.0, 3.0) != 3.0) return false;
    if (Timeline::pageToCti(10.0, 5.0, 60.0, 58.0) != 55.0) return false;
    if (Timeline::pageToCti(10.0, 80.0, 60.0, 95.0) != 0.0) return false;
    if (Timeline::centeredOn(5.0, 60.0, 12.0) != 9.5) return false;
    return Timeline::pageToCti(10.0, std::numeric_limits<double>::quiet_NaN(), 60.0, 95.0) == 10.0;
}

// after EffectCraft crates/ui-egui/src/panels/timeline/tests.rs edge_scroll_speed_x
static bool t_timeline_edge_scroll_speed() {
    if (Timeline::edgeScrollSpeedX(100.0, 500.0, 300.0) != 0.0) return false;
    if (!(Timeline::edgeScrollSpeedX(100.0, 500.0, 501.0) > 0.0)) return false;
    if (!(Timeline::edgeScrollSpeedX(100.0, 500.0, 600.0) >
          Timeline::edgeScrollSpeedX(100.0, 500.0, 520.0))) return false;
    if (!(Timeline::edgeScrollSpeedX(100.0, 500.0, 99.0) < 0.0)) return false;
    return Timeline::edgeScrollSpeedX(100.0, 500.0, std::numeric_limits<double>::quiet_NaN()) == 0.0;
}

// after EffectCraft crates/ui-egui/src/panels/timeline/tests.rs cti_snaps_to_the_nearest_candidate
static bool t_timeline_snap_and_pixel_mapping() {
    const double targets[] = {0.0, 1.0, 2.0};
    if (Timeline::snapTime(targets, 3, 1.04, 0.1) != 1.0) return false;
    if (Timeline::snapTime(targets, 3, 1.5, 0.1) != 1.5) return false;
    if (Timeline::snapTime(targets, 3, 1.5, 0.5) != 1.0) return false;
    const auto at = Timeline::timeAtPixel(150.0, 100.0, 2.0, 50.0,
                                          FrameRate::FPS_30, Tick::from_seconds_f64(10.0));
    return at == FrameRate::FPS_30.tick_of(90);
}

// after EffectCraft crates/ui-egui/tests/it/ui_timeline_trim.rs dragging_bar_ends_trims_in_and_out
static bool t_timeline_trim_and_move_layer() {
    using Timeline::BarEdit;
    using Timeline::LayerTiming;
    const Tick frame = FrameRate::FPS_30.frame_duration();
    const LayerTiming original{Tick::from_seconds_f64(2.0), Tick::from_seconds_f64(6.0), Tick::ZERO};
    const auto trimOut = Timeline::editLayerTiming(original, BarEdit::TrimOut,
        Tick::from_seconds_f64(-1.0), frame, Tick::from_seconds_f64(10.0));
    if (trimOut.in != original.in || trimOut.out != Tick::from_seconds_f64(5.0) || trimOut.start != original.start)
        return false;
    const auto trimIn = Timeline::editLayerTiming(original, BarEdit::TrimIn,
        Tick::from_seconds_f64(1.0), frame, Tick::from_seconds_f64(10.0));
    if (trimIn.in != Tick::from_seconds_f64(3.0) || trimIn.out != original.out || trimIn.start != original.start)
        return false;
    const auto moved = Timeline::editLayerTiming(original, BarEdit::Move,
        Tick::from_seconds_f64(2.0), frame, Tick::from_seconds_f64(10.0));
    if (moved.in != Tick::from_seconds_f64(4.0) || moved.out != Tick::from_seconds_f64(8.0) ||
        moved.start != Tick::from_seconds_f64(2.0))
        return false;
    const auto minimum = Timeline::editLayerTiming(original, BarEdit::TrimOut,
        Tick::from_seconds_f64(-20.0), frame, Tick::from_seconds_f64(10.0));
    return minimum.out == original.in + frame;
}

// after EffectCraft crates/engine/src/cmd/prop.rs toggle_key_at
static bool t_timeline_key_toggle_preserves_last_value() {
    Props::Property property(1, "amount", "Amount", KeyframeModel::Value::Scalar(10.0));
    const Tick time = Tick::from_seconds_f64(1.0);
    if (!Timeline::toggleKeyframe(property, time) || property.keys.size() != 1)
        return false;
    property.set_value_at(time, KeyframeModel::Value::Scalar(42.0));
    if (Timeline::toggleKeyframe(property, time) || !property.keys.empty())
        return false;
    if (property.value != KeyframeModel::Value::Scalar(42.0))
        return false;
    property.hold_only = true;
    return Timeline::toggleKeyframe(property, time) && property.keys.size() == 1 &&
        property.keys.front().in_interp == KeyframeModel::Interp::Hold;
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs layer_reorder_drag
static bool t_timeline_layer_reorder_preserves_stack_order() {
    const std::vector<uint64_t> order{1, 2, 3, 4, 5};
    if (Timeline::reorderLayers(order, {2, 4}, 5) != std::vector<uint64_t>{1, 3, 2, 4, 5})
        return false;
    if (Timeline::reorderLayers(order, {2, 4}, std::nullopt) != std::vector<uint64_t>{1, 3, 5, 2, 4})
        return false;
    if (Timeline::reorderLayers(order, {2, 3}, 2) != order)
        return false;
    return Timeline::reorderLayers(order, {}, 3) == order;
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs test shift_locks_drag_to_dominant_axis
static bool t_graph_shift_axes() {
    if (Timeline::shiftAxes(false, 10.0f, 3.0f) != std::make_pair(true, true)) return false;
    if (Timeline::shiftAxes(true, 10.0f, -3.0f) != std::make_pair(true, false)) return false;
    if (Timeline::shiftAxes(true, 2.0f, -9.0f) != std::make_pair(false, true)) return false;
    return Timeline::shiftAxes(true, 5.0f, 5.0f) == std::make_pair(true, false);
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs HandleGrab::ease
static bool t_graph_handle_ease() {
    const KeyframeModel::Ease speed = Timeline::easeAtHandle(
        {-12.0, 0.4}, 1.0, 4.0, 1.5, 9.0, 2.0, true, true);
    if (std::abs(speed.speed + 9.0) > 1e-9 || std::abs(speed.influence - 0.25) > 1e-9)
        return false;
    const KeyframeModel::Ease value = Timeline::easeAtHandle(
        {3.0, 0.2}, 1.0, 4.0, 2.0, 10.0, 2.0, true, false);
    if (std::abs(value.speed - 6.0) > 1e-9 || std::abs(value.influence - 0.5) > 1e-9)
        return false;
    const KeyframeModel::Ease limited = Timeline::easeAtHandle(
        {5.0, 0.2}, 2.0, 0.0, 1.0, 3.0, 2.0, false, false);
    return std::abs(limited.influence - 0.5) < 1e-9 &&
        std::abs(limited.speed + 3.0) < 1e-9;
}

// after EffectCraft crates/ui-egui/src/panels/timeline/tests.rs a_keys_glyph_rect_is_what_it_draws and keys_in_box_walks_the_rows_and_tests_each_glyph
static bool t_timeline_key_marquee_glyphs() {
    const Tick one = Tick::from_seconds_f64(1.0);
    const Tick two = Tick::from_seconds_f64(2.0);
    const std::vector<Timeline::KeyMarker> markers{
        {2, 20, one, 100.0f, 10.0f, 9.0f},
        {2, 20, two, 200.0f, 10.0f, 9.0f},
        {1, 10, one, 100.0f, 90.0f, 9.0f},
        {1, 10, two, 200.0f, 90.0f, 9.0f},
    };
    const auto full = Timeline::keysInBox(markers, 0.0f, 0.0f, 300.0f, 100.0f);
    if (full.size() != 4 || full[0].layer != 2 || full[2].layer != 1) return false;
    const auto edge = Timeline::keysInBox(markers, 0.0f, 94.5f, 300.0f, 100.0f);
    if (edge.size() != 2 || edge[0].layer != 1) return false;
    const auto below = Timeline::keysInBox(markers, 0.0f, 95.1f, 300.0f, 100.0f);
    return below.empty();
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs key_select
static bool t_timeline_key_selection_add_and_toggle() {
    const Timeline::KeyRef a{1, 10, Tick::from_seconds_f64(1.0)};
    const Timeline::KeyRef b{2, 20, Tick::from_seconds_f64(2.0)};
    std::vector<Timeline::KeyRef> selected;
    if (!Timeline::selectKey(selected, a, false) || selected.size() != 1) return false;
    if (!Timeline::selectKey(selected, b, true) || selected.size() != 2) return false;
    if (Timeline::selectKey(selected, a, true) || selected.size() != 1) return false;
    return Timeline::selectKey(selected, b, false) && selected.size() == 1;
}

static std::tuple<Proj, uint64_t, Composition> render_setup() {
    Proj p;
    p.nextId = 1;
    Composition comp;
    comp.id.v = p.alloc();
    comp.name = "Comp";
    comp.width = 200;
    comp.height = 100;
    comp.frameRate = FrameRate::FPS_30;
    comp.duration = Tick::from_seconds_f64(2.0);
    p.compositions.push_back(comp);
    p.activeComp = 0;
    return {p, comp.id.v, comp};
}

static Layer make_solid(Proj& p, const Composition& comp, std::array<float, 3> color, uint32_t w, uint32_t h) {
    Layer l;
    l.id.v = p.alloc();
    l.name = "Solid";
    l.kind = LayerKind::Solid;
    l.width = w;
    l.height = h;
    l.solidR = color[0];
    l.solidG = color[1];
    l.solidB = color[2];
    l.solidA = 1.0f;
    l.inPt = Tick::ZERO;
    l.outPt = comp.duration;
    l.startTime = Tick::ZERO;
    l.stretch = 100.0;
    l.xform.anchorX = (float)w / 2.0f;
    l.xform.anchorY = (float)h / 2.0f;
    l.xform.posX = (float)comp.width / 2.0f;
    l.xform.posY = (float)comp.height / 2.0f;
    l.xform.scaleX = 100.0f;
    l.xform.scaleY = 100.0f;
    l.xform.opacity = 100.0f;
    l.xform.rotation = 0.0f;
    uint64_t next = p.nextId;
    l.initProps(&next, l.xform.anchorX, l.xform.anchorY, l.xform.posX, l.xform.posY);
    p.nextId = next;
    return l;
}

// after EffectCraft crates/render/src/tests.rs test solid_fills_comp
static bool t_render_solid_fills_comp() {
    auto [p, cid, comp] = render_setup();
    auto l = make_solid(p, comp, {1.0f, 0.0f, 0.0f}, 200, 100);
    p.compositions[0].layers.push_back(l);
    auto img = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    if (img.get(10, 10) != Raster::Px{1.0f, 0.0f, 0.0f, 1.0f}) return false;
    if (img.get(199, 99) != Raster::Px{1.0f, 0.0f, 0.0f, 1.0f}) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test position_keyframes_move_layer
static bool t_render_position_keyframes_move_layer() {
    auto [p, cid, comp] = render_setup();
    auto l = make_solid(p, comp, {0.0f, 1.0f, 0.0f}, 20, 20);
    auto* pos = l.props.prop_mut("transform/position");
    if (!pos) return false;
    pos->keys = {
        KeyframeModel::Keyframe(Tick::ZERO, KeyframeModel::Value::Vec3(20.0, 50.0, 0.0)),
        KeyframeModel::Keyframe(Tick::from_seconds_f64(1.0), KeyframeModel::Value::Vec3(180.0, 50.0, 0.0))
    };
    p.compositions[0].layers.push_back(l);
    auto a = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    if (!(a.get(20, 50)[3] > 0.99f && a.get(100, 50)[3] < 0.01f)) return false;
    auto b = Render::render_frame(&p, cid, Tick::from_seconds_f64(0.5), 1.0);
    if (!(b.get(100, 50)[3] > 0.99f && b.get(20, 50)[3] < 0.01f)) return false;
    return true;
}

// after EffectCraft crates/project/src/layer.rs layer_time, comp_time
static bool t_project_layer_time_maps_keyframes() {
    auto [p, cid, comp] = render_setup();
    auto layer = make_solid(p, comp, {0.0f, 1.0f, 0.0f}, 20, 20);
    layer.inPt = Tick::from_seconds_f64(1.0);
    layer.startTime = Tick::from_seconds_f64(1.0);
    layer.stretch = 200.0;
    const Tick compTime = Tick::from_seconds_f64(2.0);
    const Tick layerTime = Tick::from_seconds_f64(0.5);
    if (layer.layerTime(compTime) != layerTime || layer.compTime(layerTime) != compTime)
        return false;
    auto* opacity = layer.props.prop_mut("transform/opacity");
    if (!opacity) return false;
    const Tick keyTime = layer.layerTime(Tick::from_seconds_f64(1.5));
    opacity->set_animated(true, keyTime);
    if (opacity->keys.size() != 1 || opacity->keys[0].time != Tick::from_seconds_f64(0.25))
        return false;
    opacity->set_value_at(keyTime, KeyframeModel::Value::Scalar(40.0));
    if (opacity->value_at(keyTime) != KeyframeModel::Value::Scalar(40.0))
        return false;
    layer.stretch = -200.0;
    if (layer.layerTime(compTime) != Tick::from_seconds_f64(-0.5) ||
        layer.compTime(Tick::from_seconds_f64(-0.5)) != compTime)
        return false;
    layer.stretch = 0.5;
    if (layer.layerTime(compTime) != Tick::from_seconds_f64(100.0))
        return false;
    layer.stretch = 10000.0;
    if (std::abs(layer.layerTime(compTime).seconds() - 100.0 / 9900.0) > 1e-9)
        return false;
    layer.stretch = 0.0;
    if (layer.layerTime(compTime) != Tick::from_seconds_f64(1.0))
        return false;
    layer.stretch = std::numeric_limits<double>::quiet_NaN();
    return layer.layerTime(compTime) == Tick::from_seconds_f64(1.0);
}

// after EffectCraft crates/render/src/tests.rs: animated properties use layer-local time
static bool t_render_stretched_layer_keys() {
    auto [p, cid, comp] = render_setup();
    auto layer = make_solid(p, comp, {0.0f, 1.0f, 0.0f}, 20, 20);
    layer.inPt = Tick::from_seconds_f64(1.0);
    layer.outPt = comp.duration;
    layer.startTime = Tick::from_seconds_f64(1.0);
    layer.stretch = 200.0;
    auto* position = layer.props.prop_mut("transform/position");
    if (!position) return false;
    position->keys = {
        KeyframeModel::Keyframe(Tick::ZERO, KeyframeModel::Value::Vec3(20.0, 50.0, 0.0)),
        KeyframeModel::Keyframe(Tick::from_seconds_f64(1.0), KeyframeModel::Value::Vec3(180.0, 50.0, 0.0))
    };
    p.compositions[0].layers.push_back(layer);
    auto image = Render::render_frame(&p, cid, Tick::from_seconds_f64(1.5), 1.0);
    return image.get(60, 50)[3] > 0.99f && image.get(20, 50)[3] < 0.01f;
}

// after EffectCraft crates/render/src/tests.rs test opacity_and_blend_modes
static bool t_render_opacity_and_blend_modes() {
    auto [p, cid, comp] = render_setup();
    auto bottom = make_solid(p, comp, {0.5f, 0.5f, 0.5f}, 200, 100);
    auto top = make_solid(p, comp, {0.5f, 0.5f, 0.5f}, 200, 100);
    top.blendMode = Raster::BlendMode::Multiply;
    p.compositions[0].layers.push_back(top);
    p.compositions[0].layers.push_back(bottom);
    auto img = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    if (std::abs(img.get(50, 50)[0] - 0.25f) >= 1e-4f) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test half_resolution
static bool t_render_half_resolution() {
    auto [p, cid, comp] = render_setup();
    auto l = make_solid(p, comp, {1.0f, 1.0f, 1.0f}, 200, 100);
    p.compositions[0].layers.push_back(l);
    auto img = Render::render_frame(&p, cid, Tick::ZERO, 0.5);
    if (img.width != 100 || img.height != 50) return false;
    if (std::abs(img.get(50, 25)[3] - 1.0f) >= 1e-4f) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test mask_cuts_layer
static bool t_render_mask_cuts_layer() {
    auto [p, cid, comp] = render_setup();
    auto l = make_solid(p, comp, {1.0f, 1.0f, 1.0f}, 200, 100);
    uint64_t next = p.nextId;
    Props::Ids ids(&next);
    auto m = Props::Build::mask(ids, "Mask 1", KeyframeModel::ShapePath::rect({50.0, 50.0}, 40.0, 40.0), Props::MaskMode::Add, {255, 255, 0});
    p.nextId = next;
    l.props.sub_mut("masks")->children.push_back(Props::Node(m));
    p.compositions[0].layers.push_back(l);
    auto img = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    if (img.get(50, 50)[3] <= 0.99f) return false;
    if (img.get(150, 50)[3] >= 0.01f) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test first_intersect_and_darken_keep_their_rectangle
static bool t_render_first_intersect_darken() {
    auto do_render = [](const std::vector<Props::MaskMode>& modes, const std::vector<std::array<double, 2>>& centers) {
        auto [p, cid, comp] = render_setup();
        auto l = make_solid(p, comp, {1.0f, 1.0f, 1.0f}, 200, 100);
        for (size_t i = 0; i < modes.size(); ++i) {
            uint64_t next = p.nextId;
            Props::Ids ids(&next);
            auto m = Props::Build::mask(ids, "Mask " + std::to_string(i + 1), KeyframeModel::ShapePath::rect(centers[i], 40.0, 40.0), modes[i], {255, 255, 0});
            p.nextId = next;
            l.props.sub_mut("masks")->children.push_back(Props::Node(m));
        }
        p.compositions[0].layers.push_back(l);
        return Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    };

    Props::MaskMode single_modes[] = {
        Props::MaskMode::Intersect, Props::MaskMode::Darken, Props::MaskMode::Add,
        Props::MaskMode::Lighten, Props::MaskMode::Difference
    };
    for (auto mode : single_modes) {
        auto img = do_render({mode}, {{50.0, 50.0}});
        if (img.get(50, 50)[3] <= 0.99f) return false;
        if (img.get(150, 50)[3] >= 0.01f) return false;
    }
    auto hole = do_render({Props::MaskMode::Subtract}, {{50.0, 50.0}});
    if (hole.get(50, 50)[3] >= 0.01f) return false;
    if (hole.get(150, 50)[3] <= 0.99f) return false;

    auto overlap = do_render({Props::MaskMode::Add, Props::MaskMode::Intersect}, {{50.0, 50.0}, {70.0, 50.0}});
    if (overlap.get(50, 50)[3] <= 0.99f) return false;
    if (overlap.get(35, 50)[3] >= 0.01f) return false;
    if (overlap.get(85, 50)[3] >= 0.01f) return false;

    auto darkened = do_render({Props::MaskMode::Add, Props::MaskMode::Darken}, {{50.0, 50.0}, {70.0, 50.0}});
    if (!(darkened.get(50, 50)[3] > 0.99f && darkened.get(35, 50)[3] < 0.01f && darkened.get(85, 50)[3] < 0.01f)) return false;

    auto both = do_render({Props::MaskMode::Intersect, Props::MaskMode::Add}, {{50.0, 50.0}, {150.0, 50.0}});
    if (!(both.get(50, 50)[3] > 0.99f && both.get(150, 50)[3] > 0.99f && both.get(100, 50)[3] < 0.01f)) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test track_matte_alpha
static bool t_render_track_matte_alpha() {
    auto [p, cid, comp] = render_setup();
    auto matte = make_solid(p, comp, {1.0f, 1.0f, 1.0f}, 50, 50);
    matte.props.prop_mut("transform/position")->value = KeyframeModel::Value::Vec3(50.0, 50.0, 0.0);
    matte.switches.video = false;
    auto fill = make_solid(p, comp, {0.0f, 0.0f, 1.0f}, 200, 100);
    fill.hasMatte = true;
    fill.matteLayer = matte.id;
    fill.matteKind = MatteKind::Alpha;
    p.compositions[0].layers.push_back(matte);
    p.compositions[0].layers.push_back(fill);
    auto img = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
    if (img.get(50, 50)[2] <= 0.99f) return false;
    if (img.get(150, 50)[3] >= 0.01f) return false;
    return true;
}

// after EffectCraft crates/render/src/tests.rs test track_matte_kinds_keep_and_invert
static bool t_render_track_matte_kinds() {
    std::vector<std::pair<MatteKind, bool>> kinds = {
        {MatteKind::Alpha, true},
        {MatteKind::AlphaInverse, false},
        {MatteKind::Luma, true},
        {MatteKind::LumaInverse, false}
    };
    for (const auto& item : kinds) {
        MatteKind kind = item.first;
        bool inside = item.second;
        auto [p, cid, comp] = render_setup();
        auto matte = make_solid(p, comp, {1.0f, 1.0f, 1.0f}, 50, 50);
        matte.props.prop_mut("transform/position")->value = KeyframeModel::Value::Vec3(50.0, 50.0, 0.0);
        matte.switches.video = false;
        auto fill = make_solid(p, comp, {0.0f, 0.0f, 1.0f}, 200, 100);
        fill.hasMatte = true;
        fill.matteLayer = matte.id;
        fill.matteKind = kind;
        p.compositions[0].layers.push_back(matte);
        p.compositions[0].layers.push_back(fill);
        auto img = Render::render_frame(&p, cid, Tick::ZERO, 1.0);
        float a_in = img.get(50, 50)[3], a_out = img.get(150, 50)[3];
        if (inside) {
            if (!(a_in > 0.99f && a_out < 0.01f)) return false;
            if (img.get(50, 50)[2] <= 0.99f) return false;
        } else {
            if (!(a_in < 0.01f && a_out > 0.99f)) return false;
            if (img.get(150, 50)[2] <= 0.99f) return false;
        }
    }
    return true;
}

// after EffectCraft crates/render/src/frames.rs cached frame reuse and eviction semantics.
static bool t_render_frame_cache() {
    const size_t oneFrame = 4 * sizeof(Raster::Px);
    Render::FrameCache cache(oneFrame);
    cache.insert(7, 0, 0.5, Raster::Image::filled(2, 2, {1, 0, 0, 1}));
    if (!cache.find(7, 0, 0.5) || cache.size() != 1) return false;
    cache.insert(7, 1, 0.5, Raster::Image::filled(2, 2, {0, 1, 0, 1}));
    const Raster::Image* last = cache.find(7, 1, 0.5);
    if (!last || cache.size() != 1 || cache.find(7, 0, 0.5)) return false;
    if (last->get(0, 0) != Raster::Px{0, 1, 0, 1}) return false;
    cache.insert(7, 1, 0.5, Raster::Image::filled(3, 3, {1, 0, 0, 1}));
    if (cache.find(7, 1, 0.5) || cache.size() != 0) return false;
    cache.clear();
    return cache.size() == 0;
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
    add("project::make_new_defaults",                              t_project_make_new);
    add("project::roundtrip_json",                                 t_project_roundtrip_json);
    add("project::comp_resolution_saved",                          t_comp_resolution_saved);
    add("project::all_layer_kinds_roundtrip",                      t_layer_kinds_roundtrip);
    add("shape::contents_and_primitive_properties_roundtrip",       t_shape_contents_roundtrip);
    add("project::switches_roundtrip",                             t_switches_roundtrip);
    add("project::transform_roundtrip",                            t_transform_roundtrip);
    add("project::paths_resolve",                                  t_project_paths_resolve);
    add("project::masks_and_occurrence_paths",                     t_project_masks_and_occurrence_paths);
    add("project::animate_and_roundtrip_json",                     t_project_animate_and_roundtrip_json);
    add("project::numbered_names_keep_digits_that_end_a_word",     t_project_numbered_names);
    add("project::unique_names_and_cycles",                        t_project_unique_names);
    add("project::same_content_sees_allocated_ids_and_comp_edits", t_project_same_content);
    // Keyframe tests (after EffectCraft crates/keyframe/src/tests.rs)
    add("keyframe::missing_spatial_keys_have_no_tangents_or_length",               t_kf_missing_spatial_keys);
    add("keyframe::time_reverse_handles_deserialized_tick_extremes",               t_kf_time_reverse_extremes);
    add("keyframe::replacing_key_preserves_spatial_metadata",                      t_kf_replacing_key_preserves_spatial_metadata);
    add("keyframe::temporal_ease_matches_live_ae_with_overlapping_influences",     t_kf_temporal_ease_matches_live_ae);
    add("keyframe::maximum_influence_resolves_flat_midpoint_time_curve",           t_kf_maximum_influence);
    add("keyframe::keyed_orientation_matches_after_effects",                       t_kf_keyed_orientation);
    add("keyframe::linear_and_hold",                                               t_kf_linear_and_hold);
    add("keyframe::hold_key_keeps_its_incoming_segment_linear",                    t_kf_hold_key_incoming_linear);
    add("keyframe::easy_ease_is_symmetric_and_slow_at_ends",                       t_kf_easy_ease);
    add("keyframe::bezier_with_linear_speed_matches_linear",                       t_kf_bezier_linear_speed);
    add("keyframe::a_linear_side_next_to_an_eased_side_matches_after_effects",     t_kf_linear_side_next_to_eased);
    add("keyframe::orientation_linear_side_next_to_an_eased_side_matches_a_scalar", t_kf_orientation_linear_side);
    add("keyframe::spatial_linear_constant_speed",                                 t_kf_spatial_linear_constant_speed);
    add("keyframe::spatial_curved_path_has_smooth_constant_speed",                 t_kf_spatial_curved_speed);
    add("keyframe::spatial_auto_bezier_passes_through_keys",                       t_kf_spatial_auto_bezier_passes);
    add("keyframe::straight_auto_bezier_path_is_exact",                            t_kf_straight_auto_bezier);
    add("keyframe::overflowing_or_nan_motion_paths_evaluate",                      t_kf_overflowing_motion_paths);
    add("keyframe::spatial_auto_bezier_tangents_follow_after_effects",             t_kf_spatial_auto_bezier_tangents);
    add("keyframe::auto_spatial_holds_between_identical_keys",                     t_kf_auto_spatial_holds);
    add("keyframe::set_key_replaces_same_time",                                    t_kf_set_key_replaces);
    add("keyframe::colors_and_paths_interpolate",                                  t_kf_colors_and_paths_interpolate);
    add("keyframe::roving_keys_get_constant_speed_times",                          t_kf_roving_keys_constant_speed);
    add("keyframe::roving_keys_never_share_a_time_with_a_fixed_key",              t_kf_roving_keys_never_share_time);
    add("keyframe::side_ease_reports_linear_and_eased_sides",                      t_kf_side_ease);
    add("keyframe::time_reverse_mirrors_keys",                                     t_kf_time_reverse_mirrors);
    add("keyframe::muted_keys_are_skipped_by_evaluation",                          t_kf_muted_keys_skipped);
    add("keyframe::muted_key_automation_uses_the_active_neighbours",               t_kf_muted_key_automation);
    add("keyframe::gradient_tracks_sample_separately",                             t_kf_gradient_tracks_sample_separately);
    // Icons tests (after EffectCraft crates/ui-egui/src/icons.rs)
    add("icons::concave_glyphs_are_filled_as_convex_pieces",   t_icons_concave_glyphs_are_filled_as_convex_pieces);
    add("icons::keyframe_bounds_follow_each_half",             t_icons_keyframe_bounds_follow_each_half);
    add("icons::keyframes_are_drawn_without_a_dark_outline",   t_icons_keyframes_are_drawn_without_a_dark_outline);
    // Color & Blend tests (after EffectCraft crates/color/src/blend.rs)
    add("color::opaque_formulas",                               t_blend_opaque_formulas);
    add("color::transparent_source_keeps_destination",          t_blend_transparent_source_keeps_destination);
    add("color::source_over_transparent_destination",           t_blend_source_over_transparent_destination);
    add("color::stencil",                                       t_blend_stencil);
    add("color::hdr_modes",                                     t_blend_hdr_modes);
    add("color::after_effects_32bpc_formulas",                  t_blend_ae_32bpc_formulas);
    add("color::names_roundtrip",                               t_blend_names_roundtrip);
    // Raster tests (after EffectCraft crates/raster/src/lib.rs)
    add("raster::u8_levels_expand_to_the_quantised_pixel",     t_raster_u8_levels_expand);
    add("raster::dither_keeps_exact_codes_and_averages_the_rest", t_raster_dither_exact_and_average);
    add("raster::dithered_conversions_match_the_plain_ones_on_exact_codes", t_raster_dithered_conversions_match);
    add("raster::rgba8_roundtrip",                              t_raster_rgba8_roundtrip);
    add("raster::bilinear_at_centres_is_exact",                 t_raster_bilinear_centres);
    add("raster::alpha_bounds_found",                           t_raster_alpha_bounds);
    add("raster::noise_in_range",                               t_raster_noise_in_range);
    add("raster::padded_bounds_hostile_pad_at_the_api",         t_raster_padded_bounds_hostile);
    add("raster::padded_leaves_legit_pads_untouched",           t_raster_padded_legit);
    // Warp tests (after EffectCraft crates/raster/src/warp.rs)
    add("warp::identity_warp_copies",                           t_warp_identity_copies);
    add("warp::translate_by_integer",                           t_warp_translate_by_integer);
    add("warp::fast_path_matches_reference",                    t_warp_fast_matches_reference);
    add("warp::clear_tiles_hold_only_transparent_taps",         t_warp_clear_tiles);
    add("warp::changed_rect_covers_the_changed_pixels",         t_warp_changed_rect);
    add("warp::quantize_while_blending_matches_quantizing_after", t_warp_quantize_matches);
    add("warp::accumulate_matches_per_sample_images",           t_warp_accumulate);
    add("warp::downscale_preserves_average",                    t_warp_downscale);
    // Timeline tests (after EffectCraft crates/ui-egui/src/panels/timeline.rs)
    add("timeline::fit_and_zoom_preserve_the_anchor",            t_timeline_fit_and_zoom);
    add("timeline::playback_pages_the_view_to_the_cti",           t_timeline_pages_to_cti);
    add("timeline::marquee_edge_autoscroll_speed",                t_timeline_edge_scroll_speed);
    add("timeline::scrubbing_snaps_to_frames_and_nearest_targets", t_timeline_snap_and_pixel_mapping);
    add("timeline::bar_drags_trim_and_move_frame_accurately",   t_timeline_trim_and_move_layer);
    add("timeline::key_toggle_preserves_last_value",             t_timeline_key_toggle_preserves_last_value);
    add("timeline::layer_reorder_preserves_selection_order",     t_timeline_layer_reorder_preserves_stack_order);
    add("graph::shift_locks_drag_to_dominant_axis",               t_graph_shift_axes);
    add("graph::handle_drag_sets_speed_and_influence",            t_graph_handle_ease);
    add("timeline::key_marquee_hits_visible_glyphs_in_row_order", t_timeline_key_marquee_glyphs);
    add("timeline::key_selection_adds_and_toggles",                t_timeline_key_selection_add_and_toggle);
    // Render tests (after EffectCraft crates/render/src/tests.rs)
    add("render::solid_fills_comp",                             t_render_solid_fills_comp);
    add("render::position_keyframes_move_layer",                t_render_position_keyframes_move_layer);
    add("project::layer_time_maps_keyframes",                    t_project_layer_time_maps_keyframes);
    add("render::stretched_layer_keys_use_layer_time",           t_render_stretched_layer_keys);
    add("render::opacity_and_blend_modes",                      t_render_opacity_and_blend_modes);
    add("render::half_resolution",                              t_render_half_resolution);
    add("render::mask_cuts_layer",                              t_render_mask_cuts_layer);
    add("render::first_intersect_and_darken_keep_their_rectangle", t_render_first_intersect_darken);
    add("render::track_matte_alpha",                            t_render_track_matte_alpha);
    add("render::track_matte_kinds_keep_and_invert",            t_render_track_matte_kinds);
    add("render::frame_cache_reuses_and_evicts_within_budget",   t_render_frame_cache);
}

} // namespace SelfTest
