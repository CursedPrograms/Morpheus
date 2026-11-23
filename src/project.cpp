// Morpheus - project model: save/load and construction.
// Ported from EffectCraft crates/project (item.rs, comp.rs, layer.rs, build.rs, tests.rs).
// JSON format matches the .morpheus schema.
// after EffectCraft (crates/project/src/item.rs, crates/project/src/comp.rs,
//                    crates/project/src/layer.rs, crates/project/src/build.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "project.h"
#include "time_.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// We use a minimal hand-written JSON reader/writer rather than a third-party
// library so the build stays self-contained.
// (A proper port would use serde_json; here we emit/parse just the fields
// actually used by the .morpheus format.)

namespace Project {

// =====================================================================
// Project::makeNew  (after EffectCraft Project::default + a "Composition 1")
// =====================================================================
ProjectData ProjectData::makeNew() {
    Project p;
    p.name = "Untitled";
    p.dirty = true;
    p.nextId = 1;

    Composition comp;
    comp.id.v       = p.alloc();
    comp.name       = "Composition 1";
    comp.width      = 1920;
    comp.height     = 1080;
    comp.frameRate  = FrameRate::FPS_29_97;
    comp.duration   = Tick::from_seconds_f64(10.0);
    comp.workAreaStart = Tick::ZERO;
    comp.workAreaDur   = comp.duration;
    p.compositions.push_back(comp);
    p.activeComp = 0;
    return p;
}

// =====================================================================
// Minimal JSON helpers (write)
// =====================================================================

// Escape a string for JSON.
static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20) {
            char buf[8]; snprintf(buf, sizeof buf, "\\u%04x", (unsigned)c);
            out += buf;
        } else {
            out += (char)c;
        }
    }
    out += '"';
    return out;
}

// Write a double with enough precision, omitting trailing zeros.
static std::string jsonDouble(double v) {
    if (std::isnan(v) || std::isinf(v)) return "0";
    char buf[64]; snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

// =====================================================================
// Property Tree JSON Serialization (after EffectCraft crates/project/src/props.rs)
// =====================================================================

static std::string valueToJson(const KeyframeModel::Value& v) {
    std::string o = "{\"t\":";
    switch (v.kind) {
    case KeyframeModel::ValueKind::Scalar:
        o += "\"Scalar\",\"v\":" + jsonDouble(v.scalar) + "}";
        break;
    case KeyframeModel::ValueKind::Vec2:
        o += "\"Vec2\",\"v\":[" + jsonDouble(v.v2[0]) + "," + jsonDouble(v.v2[1]) + "]}";
        break;
    case KeyframeModel::ValueKind::Vec3:
        o += "\"Vec3\",\"v\":[" + jsonDouble(v.v3[0]) + "," + jsonDouble(v.v3[1]) + "," + jsonDouble(v.v3[2]) + "]}";
        break;
    case KeyframeModel::ValueKind::Color:
        o += "\"Color\",\"v\":[" + jsonDouble(v.color[0]) + "," + jsonDouble(v.color[1]) + "," + jsonDouble(v.color[2]) + "," + jsonDouble(v.color[3]) + "]}";
        break;
    case KeyframeModel::ValueKind::Bool:
        o += std::string("\"Bool\",\"v\":") + (v.b ? "true}" : "false}");
        break;
    case KeyframeModel::ValueKind::Enum:
        o += "\"Enum\",\"v\":" + std::to_string(v.e) + "}";
        break;
    case KeyframeModel::ValueKind::Str:
        o += "\"Str\",\"v\":" + jsonStr(v.str) + "}";
        break;
    case KeyframeModel::ValueKind::Layer:
        o += "\"Layer\",\"v\":" + (v.layer == 0 ? "null}" : std::to_string(v.layer) + "}");
        break;
    case KeyframeModel::ValueKind::Path: {
        o += "\"Path\",\"v\":{";
        o += "\"closed\":" + std::string(v.path.closed ? "true" : "false") + ",";
        o += "\"vertices\":[";
        for (size_t i = 0; i < v.path.vertices.size(); ++i) {
            if (i > 0) o += ",";
            o += "[" + jsonDouble(v.path.vertices[i][0]) + "," + jsonDouble(v.path.vertices[i][1]) + "]";
        }
        o += "],\"in_tangents\":[";
        for (size_t i = 0; i < v.path.in_tangents.size(); ++i) {
            if (i > 0) o += ",";
            o += "[" + jsonDouble(v.path.in_tangents[i][0]) + "," + jsonDouble(v.path.in_tangents[i][1]) + "]";
        }
        o += "],\"out_tangents\":[";
        for (size_t i = 0; i < v.path.out_tangents.size(); ++i) {
            if (i > 0) o += ",";
            o += "[" + jsonDouble(v.path.out_tangents[i][0]) + "," + jsonDouble(v.path.out_tangents[i][1]) + "]";
        }
        o += "],\"feather\":[";
        for (size_t i = 0; i < v.path.feather.size(); ++i) {
            if (i > 0) o += ",";
            const auto& f = v.path.feather[i];
            o += "{\"segment\":" + std::to_string(f.segment) + ",\"t\":" + jsonDouble(f.t) + ",\"radius\":" + jsonDouble(f.radius) + ",\"tension\":" + jsonDouble(f.tension) + "}";
        }
        o += "]}}";
        break;
    }
    case KeyframeModel::ValueKind::Gradient: {
        o += "\"Gradient\",\"v\":{\"colors\":[";
        for (size_t i = 0; i < v.grad.colors.size(); ++i) {
            if (i > 0) o += ",";
            const auto& c = v.grad.colors[i];
            o += "[" + jsonDouble(c.first) + ",[" + jsonDouble(c.second[0]) + "," + jsonDouble(c.second[1]) + "," + jsonDouble(c.second[2]) + "," + jsonDouble(c.second[3]) + "]]";
        }
        o += "],\"opacities\":[";
        for (size_t i = 0; i < v.grad.opacities.size(); ++i) {
            if (i > 0) o += ",";
            const auto& op = v.grad.opacities[i];
            o += "[" + jsonDouble(op.first) + "," + jsonDouble(op.second) + "]";
        }
        o += "]}}";
        break;
    }
    }
    return o;
}

static std::string keyframeToJson(const KeyframeModel::Keyframe& k) {
    std::string o = "{";
    o += "\"time\":" + std::to_string(k.time.v) + ",";
    o += "\"value\":" + valueToJson(k.value) + ",";
    o += "\"in_interp\":" + jsonStr(KeyframeModel::interp_label(k.in_interp)) + ",";
    o += "\"out_interp\":" + jsonStr(KeyframeModel::interp_label(k.out_interp)) + ",";
    o += "\"continuous\":" + std::string(k.continuous ? "true" : "false") + ",";
    o += "\"auto_bezier\":" + std::string(k.auto_bezier ? "true" : "false") + ",";
    o += "\"spatial_in\":[" + jsonDouble(k.spatial_in[0]) + "," + jsonDouble(k.spatial_in[1]) + "," + jsonDouble(k.spatial_in[2]) + "],";
    o += "\"spatial_out\":[" + jsonDouble(k.spatial_out[0]) + "," + jsonDouble(k.spatial_out[1]) + "," + jsonDouble(k.spatial_out[2]) + "],";
    o += "\"spatial_auto\":" + std::string(k.spatial_auto ? "true" : "false") + ",";
    o += "\"spatial_continuous\":" + std::string(k.spatial_continuous ? "true" : "false") + ",";
    o += "\"roving\":" + std::string(k.roving ? "true" : "false") + ",";
    o += "\"label\":" + std::to_string(k.label) + ",";
    o += "\"muted\":" + std::string(k.muted ? "true" : "false") + ",";
    o += "\"in_ease\":[";
    for (size_t i = 0; i < k.in_ease.size(); ++i) {
        if (i > 0) o += ",";
        o += "{\"speed\":" + jsonDouble(k.in_ease[i].speed) + ",\"influence\":" + jsonDouble(k.in_ease[i].influence) + "}";
    }
    o += "],\"out_ease\":[";
    for (size_t i = 0; i < k.out_ease.size(); ++i) {
        if (i > 0) o += ",";
        o += "{\"speed\":" + jsonDouble(k.out_ease[i].speed) + ",\"influence\":" + jsonDouble(k.out_ease[i].influence) + "}";
    }
    o += "]}";
    return o;
}

static std::string propertyToJson(const Props::Property& p) {
    std::string o = "{";
    o += "\"uid\":" + std::to_string(p.uid) + ",";
    o += "\"match\":" + jsonStr(p.match_id) + ",";
    o += "\"name\":" + jsonStr(p.name) + ",";
    o += "\"value\":" + valueToJson(p.value) + ",";
    o += "\"spatial\":" + std::string(p.spatial ? "true" : "false") + ",";
    o += "\"hold_only\":" + std::string(p.hold_only ? "true" : "false") + ",";
    o += "\"three_d_only\":" + std::string(p.three_d_only ? "true" : "false") + ",";
    o += "\"two_d_only\":" + std::string(p.two_d_only ? "true" : "false") + ",";
    o += "\"shown_dims\":" + std::to_string((int)p.shown_dims) + ",";
    o += "\"static_only\":" + std::string(p.static_only ? "true" : "false") + ",";
    if (p.expr.has_value()) {
        o += "\"expr\":{\"text\":" + jsonStr(p.expr->text) + ",\"enabled\":" + (p.expr->enabled ? "true" : "false") + "},";
    }
    o += "\"keys\":[";
    for (size_t i = 0; i < p.keys.size(); ++i) {
        if (i > 0) o += ",";
        o += keyframeToJson(p.keys[i]);
    }
    o += "]}";
    return o;
}

static std::string propGroupToJson(const Props::PropGroup& g);

static std::string nodeToJson(const Props::Node& n) {
    if (const auto* p = n.as_prop()) {
        std::string o = "{\"node\":\"Prop\",";
        std::string pstr = propertyToJson(*p);
        o += pstr.substr(1);
        return o;
    } else if (const auto* g = n.as_group()) {
        std::string o = "{\"node\":\"Group\",";
        std::string gstr = propGroupToJson(*g);
        o += gstr.substr(1);
        return o;
    }
    return "{}";
}

static std::string propGroupToJson(const Props::PropGroup& g) {
    std::string o = "{";
    o += "\"uid\":" + std::to_string(g.uid) + ",";
    o += "\"match\":" + jsonStr(g.match_id) + ",";
    o += "\"name\":" + jsonStr(g.name) + ",";
    o += "\"enabled\":" + std::string(g.enabled ? "true" : "false") + ",";
    o += "\"label\":" + std::to_string((int)g.label) + ",";
    o += "\"kind\":{\"tag\":" + std::to_string((int)g.kind.tag);
    if (g.kind.tag == Props::GroupKindTag::Mask) {
        o += ",\"mask_mode\":" + std::to_string((int)g.kind.mask_mode);
        o += ",\"mask_inverted\":" + std::string(g.kind.mask_inverted ? "true" : "false");
        o += ",\"mask_color\":[" + std::to_string((int)g.kind.mask_color[0]) + "," +
             std::to_string((int)g.kind.mask_color[1]) + "," +
             std::to_string((int)g.kind.mask_color[2]) + "]";
    } else if (g.kind.tag == Props::GroupKindTag::Effect) {
        o += ",\"effect\":" + jsonStr(g.kind.effect_id);
    }
    o += "},";
    o += "\"children\":[";
    for (size_t i = 0; i < g.children.size(); ++i) {
        if (i > 0) o += ",";
        o += nodeToJson(g.children[i]);
    }
    o += "]}";
    return o;
}

// =====================================================================
// Layer helpers  (after EffectCraft crates/project/src/layer.rs, build.rs)
// =====================================================================

void Layer::initProps(uint64_t* counter, double anchorX, double anchorY, double posX, double posY) {
    Props::Ids ids(counter);
    props = ids.group("layer", name);
    props.with(Props::Build::masks(ids));
    props.with(Props::Build::effects(ids));
    props.with(Props::Build::transform(ids, {anchorX, anchorY}, {posX, posY}));
    syncPropsToTransform();
}

void Layer::syncPropsToTransform() {
    if (const auto* a = props.prop("transform/anchor")) {
        auto v = a->value.as_vec3();
        xform.anchorX = (float)v[0];
        xform.anchorY = (float)v[1];
    }
    if (const auto* p = props.prop("transform/position")) {
        auto v = p->value.as_vec3();
        xform.posX = (float)v[0];
        xform.posY = (float)v[1];
    }
    if (const auto* s = props.prop("transform/scale")) {
        auto v = s->value.as_vec3();
        xform.scaleX = (float)v[0];
        xform.scaleY = (float)v[1];
    }
    if (const auto* r = props.prop("transform/rotation")) {
        xform.rotation = (float)r->value.as_f64();
    }
    if (const auto* op = props.prop("transform/opacity")) {
        xform.opacity = (float)op->value.as_f64();
    }
}

void Layer::syncTransformToProps() {
    if (auto* a = props.prop_mut("transform/anchor")) {
        if (!a->is_animated()) a->value = KeyframeModel::Value::Vec3(xform.anchorX, xform.anchorY, 0.0);
    }
    if (auto* p = props.prop_mut("transform/position")) {
        if (!p->is_animated()) p->value = KeyframeModel::Value::Vec3(xform.posX, xform.posY, 0.0);
    }
    if (auto* s = props.prop_mut("transform/scale")) {
        if (!s->is_animated()) s->value = KeyframeModel::Value::Vec3(xform.scaleX, xform.scaleY, 100.0);
    }
    if (auto* r = props.prop_mut("transform/rotation")) {
        if (!r->is_animated()) r->value = KeyframeModel::Value::Scalar(xform.rotation);
    }
    if (auto* op = props.prop_mut("transform/opacity")) {
        if (!op->is_animated()) op->value = KeyframeModel::Value::Scalar(xform.opacity);
    }
}

bool Layer::operator==(const Layer& o) const {
    return id == o.id && name == o.name && kind == o.kind && label == o.label &&
           comment == o.comment && sourceItem == o.sourceItem &&
           lightKind == o.lightKind && primitiveKind == o.primitiveKind &&
           inPt == o.inPt && outPt == o.outPt && startTime == o.startTime &&
           stretch == o.stretch && solidR == o.solidR && solidG == o.solidG &&
           solidB == o.solidB && solidA == o.solidA &&
           width == o.width && height == o.height && blendMode == o.blendMode &&
           preserveTransparency == o.preserveTransparency &&
           switches.video == o.switches.video && switches.audio == o.switches.audio &&
           switches.solo == o.switches.solo && switches.locked == o.switches.locked &&
           switches.shy == o.switches.shy && switches.collapse == o.switches.collapse &&
           switches.effects == o.switches.effects && switches.frameBlend == o.switches.frameBlend &&
           switches.motionBlur == o.switches.motionBlur && switches.adjustment == o.switches.adjustment &&
           switches.threeD == o.switches.threeD && switches.guide == o.switches.guide &&
           props == o.props;
}

// =====================================================================
// Composition & Name Stem Helpers (after EffectCraft crates/project/src/comp.rs, item.rs)
// =====================================================================

std::string name_stem(const std::string& base) {
    size_t i = base.size();
    while (i > 0 && std::isdigit((unsigned char)base[i - 1])) --i;
    std::string head = base.substr(0, i);
    if (i < base.size() && !head.empty() && head.back() == ' ') {
        std::string stem = head.substr(0, head.size() - 1);
        while (!stem.empty() && stem.back() == ' ') stem.pop_back();
        if (!stem.empty()) return stem;
    }
    std::string trimmed = base;
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    return trimmed;
}

std::string Composition::uniqueLayerName(const std::string& base) const {
    bool exists = false;
    for (const auto& l : layers) {
        if (l.name == base) { exists = true; break; }
    }
    if (!exists) return base;

    std::string stem = name_stem(base);
    std::vector<uint64_t> taken;
    for (const auto& l : layers) {
        if (l.name.size() > stem.size() + 1 && l.name.compare(0, stem.size(), stem) == 0 && l.name[stem.size()] == ' ') {
            std::string num_str = l.name.substr(stem.size() + 1);
            bool all_digits = !num_str.empty();
            for (char c : num_str) {
                if (!std::isdigit((unsigned char)c)) { all_digits = false; break; }
            }
            if (all_digits) {
                try {
                    uint64_t n = std::stoull(num_str);
                    if (std::to_string(n) == num_str) taken.push_back(n);
                } catch (...) {}
            }
        }
    }
    for (uint64_t cand = 2; ; ++cand) {
        if (std::find(taken.begin(), taken.end(), cand) == taken.end()) {
            return stem + " " + std::to_string(cand);
        }
    }
}

bool Composition::operator==(const Composition& o) const {
    return id == o.id && name == o.name && width == o.width && height == o.height &&
           pixelAspect == o.pixelAspect && frameRate == o.frameRate &&
           duration == o.duration && displayStart == o.displayStart &&
           workAreaStart == o.workAreaStart && workAreaDur == o.workAreaDur &&
           resolution == o.resolution && layers == o.layers;
}

std::string ProjectData::numberedItemName(const std::string& base, std::function<bool(const std::string&)> in_use) const {
    std::string stem = name_stem(base);
    for (uint64_t cand = 2; ; ++cand) {
        std::string cand_name = stem + " " + std::to_string(cand);
        if (!in_use(cand_name)) return cand_name;
    }
}

bool ProjectData::operator==(const ProjectData& o) const {
    return name == o.name && nextId == o.nextId && compositions == o.compositions;
}

// =====================================================================
// Project save (after EffectCraft Project::to_file_json)
// =====================================================================
// Schema: {"schema":1,"name":"...","compositions":[...],"nextId":N}
// Composition: {"name":"...","width":W,"height":H,"frameRate":{"num":N,"den":D},
//               "durationTicks":T,"layers":[...]}
// Layer: {"id":N,"name":"...","kind":"Solid","label":N,"inTicks":T,"outTicks":T,
//          "transform":{...},"switches":{...},"solidRGBA":[r,g,b,a]}

static std::string layerToJson(const Layer& l) {
    std::string o = "{";
    o += "\"id\":" + std::to_string(l.id.v) + ",";
    o += "\"name\":" + jsonStr(l.name) + ",";
    o += "\"kind\":" + jsonStr(layerKindTypeName(l.kind)) + ",";
    o += "\"label\":" + std::to_string((int)l.label) + ",";
    o += "\"inTicks\":" + std::to_string(l.inPt.v) + ",";
    o += "\"outTicks\":" + std::to_string(l.outPt.v) + ",";
    o += "\"startTimeTicks\":" + std::to_string(l.startTime.v) + ",";
    o += "\"stretch\":" + jsonDouble(l.stretch) + ",";
    // Transform
    char xf[256];
    snprintf(xf, sizeof xf,
        "\"transform\":{\"anchorX\":%g,\"anchorY\":%g,\"posX\":%g,\"posY\":%g,"
        "\"scaleX\":%g,\"scaleY\":%g,\"rotation\":%g,\"opacity\":%g},",
        l.xform.anchorX, l.xform.anchorY,
        l.xform.posX, l.xform.posY,
        l.xform.scaleX, l.xform.scaleY,
        l.xform.rotation, l.xform.opacity);
    o += xf;
    // Switches
    char sw[256];
    snprintf(sw, sizeof sw,
        "\"switches\":{\"video\":%s,\"audio\":%s,\"solo\":%s,\"locked\":%s,"
        "\"shy\":%s,\"effects\":%s,\"motionBlur\":%s,\"adjustment\":%s,\"threeD\":%s,\"guide\":%s},",
        l.switches.video?"true":"false",
        l.switches.audio?"true":"false",
        l.switches.solo?"true":"false",
        l.switches.locked?"true":"false",
        l.switches.shy?"true":"false",
        l.switches.effects?"true":"false",
        l.switches.motionBlur?"true":"false",
        l.switches.adjustment?"true":"false",
        l.switches.threeD?"true":"false",
        l.switches.guide?"true":"false");
    o += sw;
    // Solid colour
    char sc[128];
    snprintf(sc, sizeof sc, "\"solidRGBA\":[%g,%g,%g,%g],",
             l.solidR, l.solidG, l.solidB, l.solidA);
    o += sc;
    if (l.width > 0) o += "\"width\":" + std::to_string(l.width) + ",";
    if (l.height > 0) o += "\"height\":" + std::to_string(l.height) + ",";
    o += "\"blendMode\":" + jsonStr(Raster::blend_mode_label(l.blendMode)) + ",";
    if (l.preserveTransparency) o += "\"preserveTransparency\":true,";
    o += "\"props\":" + propGroupToJson(l.props);
    o += "}";
    return o;
}

static std::string compToJson(const Composition& c) {
    std::string o = "{";
    if (c.id.v > 0) o += "\"id\":" + std::to_string(c.id.v) + ",";
    o += "\"name\":" + jsonStr(c.name) + ",";
    o += "\"width\":" + std::to_string(c.width) + ",";
    o += "\"height\":" + std::to_string(c.height) + ",";
    o += "\"pixelAspect\":" + jsonDouble(c.pixelAspect) + ",";
    char fr[64]; snprintf(fr, sizeof fr,
        "\"frameRate\":{\"num\":%lld,\"den\":%lld},",
        (long long)c.frameRate.num, (long long)c.frameRate.den);
    o += fr;
    o += "\"durationTicks\":" + std::to_string(c.duration.v) + ",";
    o += "\"displayStartTicks\":" + std::to_string(c.displayStart.v) + ",";
    o += "\"workAreaStartTicks\":" + std::to_string(c.workAreaStart.v) + ",";
    o += "\"workAreaDurTicks\":" + std::to_string(c.workAreaDur.v) + ",";
    char bg[64]; snprintf(bg, sizeof bg, "\"background\":[%g,%g,%g],",
                          c.background[0], c.background[1], c.background[2]);
    o += bg;
    o += "\"shutterAngle\":" + jsonDouble(c.shutterAngle) + ",";
    o += "\"shutterPhase\":" + jsonDouble(c.shutterPhase) + ",";
    o += "\"motionBlurSamples\":" + std::to_string(c.motionBlurSamples) + ",";
    o += "\"motionBlurAdaptiveLimit\":" + std::to_string(c.motionBlurAdaptiveLimit) + ",";
    o += "\"renderer\":" + std::to_string((int)c.renderer) + ",";
    o += "\"hideShy\":" + std::string(c.hideShy?"true":"false") + ",";
    o += "\"enableMotionBlur\":" + std::string(c.enableMotionBlur?"true":"false") + ",";
    o += "\"enableFrameBlending\":" + std::string(c.enableFrameBlending?"true":"false") + ",";
    o += "\"preserveFrameRate\":" + std::string(c.preserveFrameRate?"true":"false") + ",";
    o += "\"preserveResolution\":" + std::string(c.preserveResolution?"true":"false") + ",";
    o += "\"resolution\":" + std::to_string((int)c.resolution) + ",";
    o += "\"layers\":[";
    for (size_t i = 0; i < c.layers.size(); ++i) {
        if (i > 0) o += ",";
        o += layerToJson(c.layers[i]);
    }
    o += "]}";
    return o;
}

std::string ProjectData::toJson() const {
    std::string o = "{";
    o += "\"schema\":1,";
    o += "\"name\":" + jsonStr(name) + ",";
    o += "\"nextId\":" + std::to_string(nextId) + ",";
    o += "\"compositions\":[";
    for (size_t i = 0; i < compositions.size(); ++i) {
        if (i > 0) o += ",";
        o += compToJson(compositions[i]);
    }
    o += "]}";
    return o;
}

bool save(Project& proj, const std::string& path) {
    std::string o = proj.toJson();
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(o.data(), 1, o.size(), f);
    fclose(f);
    proj.path  = path;
    proj.dirty = false;
    return true;
}

// =====================================================================
// Minimal JSON parser  (recursive descent)
// =====================================================================

struct Parser {
    const char* p;
    const char* end;

    explicit Parser(const char* src, size_t len) : p(src), end(src+len) {}

    void skip() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    }

    bool eof() const { return p >= end; }

    bool peek(char c) {
        skip();
        return !eof() && *p == c;
    }

    bool eat(char c) {
        skip();
        if (!eof() && *p == c) { ++p; return true; }
        return false;
    }

    // Parse a JSON string into out.  Returns false on error.
    bool parseString(std::string& out) {
        skip();
        if (!eat('"')) return false;
        out.clear();
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) return false;
                switch (*p) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (p+4 >= end) return false;
                    char hex[5] = {p[1],p[2],p[3],p[4],0};
                    unsigned cp = (unsigned)strtoul(hex, nullptr, 16);
                    p += 4;
                    // Simple ASCII output
                    if (cp < 128) out += (char)cp;
                    else out += '?';
                    break;
                }
                default: out += *p; break;
                }
            } else {
                out += *p;
            }
            ++p;
        }
        if (p >= end) return false;
        ++p; // closing "
        return true;
    }

    // Parse a JSON number into out.
    bool parseDouble(double& out) {
        skip();
        char* endp;
        out = strtod(p, &endp);
        if (endp == p) return false;
        p = endp;
        return true;
    }

    // Parse a JSON integer.
    bool parseInt64(int64_t& out) {
        skip();
        char* endp;
        out = (int64_t)strtoll(p, &endp, 10);
        if (endp == p) return false;
        p = endp;
        return true;
    }

    bool parseUint64(uint64_t& out) {
        skip();
        char* endp;
        out = (uint64_t)strtoull(p, &endp, 10);
        if (endp == p) return false;
        p = endp;
        return true;
    }

    bool parseInt(int& out) {
        int64_t v = 0;
        if (!parseInt64(v)) return false;
        out = (int)v;
        return true;
    }

    bool parseU64(uint64_t& out) {
        return parseUint64(out);
    }

    bool parseBool(bool& out) {
        skip();
        if (p+4 <= end && strncmp(p,"true",4)==0)  { out=true;  p+=4; return true; }
        if (p+5 <= end && strncmp(p,"false",5)==0) { out=false; p+=5; return true; }
        return false;
    }

    // Skip any JSON value (for unknown keys).
    bool skipValue() {
        skip();
        if (eof()) return false;
        if (*p == '"') {
            std::string dummy; return parseString(dummy);
        }
        if (*p == '{') {
            ++p;
            skip();
            if (eat('}')) return true;
            do {
                std::string key; if (!parseString(key)) return false;
                if (!eat(':')) return false;
                if (!skipValue()) return false;
            } while (eat(','));
            return eat('}');
        }
        if (*p == '[') {
            ++p;
            skip();
            if (eat(']')) return true;
            do { if (!skipValue()) return false; } while (eat(','));
            return eat(']');
        }
        if (strncmp(p,"null",4)==0)  { p+=4; return true; }
        if (strncmp(p,"true",4)==0)  { p+=4; return true; }
        if (strncmp(p,"false",5)==0) { p+=5; return true; }
        // number
        while (p < end && (*p=='-'||*p=='+'||(*p>='0'&&*p<='9')||*p=='.'||*p=='e'||*p=='E')) ++p;
        return true;
    }

    // Parse "key": value pairs in an object, calling cb for each key.
    // cb returns false to skip.
    template<typename Fn>
    bool parseObject(Fn cb) {
        if (!eat('{')) return false;
        skip();
        if (eat('}')) return true;
        do {
            std::string key;
            if (!parseString(key)) return false;
            if (!eat(':')) return false;
            if (!cb(key)) {
                if (!skipValue()) return false;
            }
        } while (eat(','));
        return eat('}');
    }

    // Parse a JSON array calling cb for each element.
    template<typename Fn>
    bool parseArray(Fn cb) {
        if (!eat('[')) return false;
        skip();
        if (eat(']')) return true;
        do { if (!cb()) return false; } while (eat(','));
        return eat(']');
    }
};

// =====================================================================
// Property Tree JSON Deserialization (after EffectCraft crates/project/src/props.rs)
// =====================================================================

static bool parseValue(Parser& jp, KeyframeModel::Value& v) {
    std::string tag;
    return jp.parseObject([&](const std::string& key) -> bool {
        if (key == "t") return jp.parseString(tag);
        if (key == "v") {
            if (tag == "Scalar") {
                double val = 0.0;
                if (!jp.parseDouble(val)) return false;
                v = KeyframeModel::Value::Scalar(val);
                return true;
            }
            if (tag == "Vec2") {
                std::vector<double> vals;
                bool ok = jp.parseArray([&]() -> bool {
                    double d; if (!jp.parseDouble(d)) return false;
                    vals.push_back(d); return true;
                });
                if (!ok) return false;
                v = KeyframeModel::Value::Vec2(vals.size() > 0 ? vals[0] : 0.0, vals.size() > 1 ? vals[1] : 0.0);
                return true;
            }
            if (tag == "Vec3") {
                std::vector<double> vals;
                bool ok = jp.parseArray([&]() -> bool {
                    double d; if (!jp.parseDouble(d)) return false;
                    vals.push_back(d); return true;
                });
                if (!ok) return false;
                v = KeyframeModel::Value::Vec3(vals.size() > 0 ? vals[0] : 0.0, vals.size() > 1 ? vals[1] : 0.0, vals.size() > 2 ? vals[2] : 0.0);
                return true;
            }
            if (tag == "Color") {
                std::vector<double> vals;
                bool ok = jp.parseArray([&]() -> bool {
                    double d; if (!jp.parseDouble(d)) return false;
                    vals.push_back(d); return true;
                });
                if (!ok) return false;
                v = KeyframeModel::Value::Color(vals.size() > 0 ? vals[0] : 0.0, vals.size() > 1 ? vals[1] : 0.0, vals.size() > 2 ? vals[2] : 0.0, vals.size() > 3 ? vals[3] : 1.0);
                return true;
            }
            if (tag == "Bool") {
                bool b = false;
                if (!jp.parseBool(b)) return false;
                v = KeyframeModel::Value::Bool(b);
                return true;
            }
            if (tag == "Enum") {
                int64_t e = 0;
                if (!jp.parseInt64(e)) return false;
                v = KeyframeModel::Value::Enum((uint32_t)e);
                return true;
            }
            if (tag == "Str") {
                std::string s;
                if (!jp.parseString(s)) return false;
                v = KeyframeModel::Value::Str(s);
                return true;
            }
            if (tag == "Layer") {
                uint64_t lay = 0;
                if (!jp.parseUint64(lay)) lay = 0;
                v = KeyframeModel::Value::Layer(lay);
                return true;
            }
            if (tag == "Path") {
                KeyframeModel::ShapePath sp;
                bool ok = jp.parseObject([&](const std::string& pk) -> bool {
                    if (pk == "closed") return jp.parseBool(sp.closed);
                    if (pk == "vertices") {
                        return jp.parseArray([&]() -> bool {
                            std::vector<double> pt;
                            if (!jp.parseArray([&]() -> bool {
                                double d; if (!jp.parseDouble(d)) return false;
                                pt.push_back(d); return true;
                            })) return false;
                            sp.vertices.push_back({pt.size() > 0 ? pt[0] : 0.0, pt.size() > 1 ? pt[1] : 0.0});
                            return true;
                        });
                    }
                    if (pk == "in_tangents") {
                        return jp.parseArray([&]() -> bool {
                            std::vector<double> pt;
                            if (!jp.parseArray([&]() -> bool {
                                double d; if (!jp.parseDouble(d)) return false;
                                pt.push_back(d); return true;
                            })) return false;
                            sp.in_tangents.push_back({pt.size() > 0 ? pt[0] : 0.0, pt.size() > 1 ? pt[1] : 0.0});
                            return true;
                        });
                    }
                    if (pk == "out_tangents") {
                        return jp.parseArray([&]() -> bool {
                            std::vector<double> pt;
                            if (!jp.parseArray([&]() -> bool {
                                double d; if (!jp.parseDouble(d)) return false;
                                pt.push_back(d); return true;
                            })) return false;
                            sp.out_tangents.push_back({pt.size() > 0 ? pt[0] : 0.0, pt.size() > 1 ? pt[1] : 0.0});
                            return true;
                        });
                    }
                    if (pk == "feather") {
                        return jp.parseArray([&]() -> bool {
                            KeyframeModel::FeatherPoint fp;
                            bool fok = jp.parseObject([&](const std::string& fk) -> bool {
                                if (fk == "segment") { int64_t seg; if (!jp.parseInt64(seg)) return false; fp.segment = (size_t)seg; return true; }
                                if (fk == "t") return jp.parseDouble(fp.t);
                                if (fk == "radius") return jp.parseDouble(fp.radius);
                                if (fk == "tension") return jp.parseDouble(fp.tension);
                                return false;
                            });
                            if (!fok) return false;
                            sp.feather.push_back(fp);
                            return true;
                        });
                    }
                    return false;
                });
                if (!ok) return false;
                v = KeyframeModel::Value::Path(sp);
                return true;
            }
            if (tag == "Gradient") {
                KeyframeModel::Gradient g;
                bool ok = jp.parseObject([&](const std::string& gk) -> bool {
                    if (gk == "colors") {
                        return jp.parseArray([&]() -> bool {
                            double pos = 0.0;
                            std::vector<double> rgba;
                            int elem = 0;
                            bool aok = jp.parseArray([&]() -> bool {
                                if (elem == 0) {
                                    if (!jp.parseDouble(pos)) return false;
                                } else {
                                    if (!jp.parseArray([&]() -> bool {
                                        double d; if (!jp.parseDouble(d)) return false;
                                        rgba.push_back(d); return true;
                                    })) return false;
                                }
                                ++elem; return true;
                            });
                            if (!aok) return false;
                            g.colors.push_back({pos, {
                                (float)(rgba.size() > 0 ? rgba[0] : 0.0),
                                (float)(rgba.size() > 1 ? rgba[1] : 0.0),
                                (float)(rgba.size() > 2 ? rgba[2] : 0.0),
                                (float)(rgba.size() > 3 ? rgba[3] : 1.0)
                            }});
                            return true;
                        });
                    }
                    if (gk == "opacities") {
                        return jp.parseArray([&]() -> bool {
                            double pos = 0.0, alpha = 1.0;
                            int elem = 0;
                            bool aok = jp.parseArray([&]() -> bool {
                                if (elem == 0) {
                                    if (!jp.parseDouble(pos)) return false;
                                } else {
                                    if (!jp.parseDouble(alpha)) return false;
                                }
                                ++elem; return true;
                            });
                            if (!aok) return false;
                            g.opacities.push_back({pos, (float)alpha});
                            return true;
                        });
                    }
                    return false;
                });
                if (!ok) return false;
                v = KeyframeModel::Value::Grad(g);
                return true;
            }
        }
        return false;
    });
}

static bool parseKeyframe(Parser& jp, KeyframeModel::Keyframe& k) {
    return jp.parseObject([&](const std::string& key) -> bool {
        if (key == "time") {
            int64_t t; if (!jp.parseInt64(t)) return false;
            k.time = Tick(t); return true;
        }
        if (key == "value") {
            return parseValue(jp, k.value);
        }
        if (key == "in_interp") {
            std::string s; if (!jp.parseString(s)) return false;
            if (s == "Bezier") k.in_interp = KeyframeModel::Interp::Bezier;
            else if (s == "Hold") k.in_interp = KeyframeModel::Interp::Hold;
            else k.in_interp = KeyframeModel::Interp::Linear;
            return true;
        }
        if (key == "out_interp") {
            std::string s; if (!jp.parseString(s)) return false;
            if (s == "Bezier") k.out_interp = KeyframeModel::Interp::Bezier;
            else if (s == "Hold") k.out_interp = KeyframeModel::Interp::Hold;
            else k.out_interp = KeyframeModel::Interp::Linear;
            return true;
        }
        if (key == "continuous") return jp.parseBool(k.continuous);
        if (key == "auto_bezier") return jp.parseBool(k.auto_bezier);
        if (key == "spatial_in") {
            int i = 0;
            return jp.parseArray([&]() -> bool {
                double d; if (!jp.parseDouble(d)) return false;
                if (i < 3) k.spatial_in[i] = d;
                ++i; return true;
            });
        }
        if (key == "spatial_out") {
            int i = 0;
            return jp.parseArray([&]() -> bool {
                double d; if (!jp.parseDouble(d)) return false;
                if (i < 3) k.spatial_out[i] = d;
                ++i; return true;
            });
        }
        if (key == "spatial_auto") return jp.parseBool(k.spatial_auto);
        if (key == "spatial_continuous") return jp.parseBool(k.spatial_continuous);
        if (key == "roving") return jp.parseBool(k.roving);
        if (key == "label") {
            int64_t l; if (!jp.parseInt64(l)) return false;
            k.label = (uint8_t)l; return true;
        }
        if (key == "muted") return jp.parseBool(k.muted);
        if (key == "in_ease") {
            return jp.parseArray([&]() -> bool {
                KeyframeModel::Ease e;
                bool ok = jp.parseObject([&](const std::string& ek) -> bool {
                    if (ek == "speed") return jp.parseDouble(e.speed);
                    if (ek == "influence") return jp.parseDouble(e.influence);
                    return false;
                });
                if (!ok) return false;
                k.in_ease.push_back(e); return true;
            });
        }
        if (key == "out_ease") {
            return jp.parseArray([&]() -> bool {
                KeyframeModel::Ease e;
                bool ok = jp.parseObject([&](const std::string& ek) -> bool {
                    if (ek == "speed") return jp.parseDouble(e.speed);
                    if (ek == "influence") return jp.parseDouble(e.influence);
                    return false;
                });
                if (!ok) return false;
                k.out_ease.push_back(e); return true;
            });
        }
        return false;
    });
}

static bool parseProperty(Parser& jp, Props::Property& p) {
    return jp.parseObject([&](const std::string& key) -> bool {
        if (key == "uid") return jp.parseUint64(p.uid);
        if (key == "match") return jp.parseString(p.match_id);
        if (key == "name") return jp.parseString(p.name);
        if (key == "value") return parseValue(jp, p.value);
        if (key == "spatial") return jp.parseBool(p.spatial);
        if (key == "hold_only") return jp.parseBool(p.hold_only);
        if (key == "three_d_only") return jp.parseBool(p.three_d_only);
        if (key == "two_d_only") return jp.parseBool(p.two_d_only);
        if (key == "shown_dims") {
            int64_t d; if (!jp.parseInt64(d)) return false;
            p.shown_dims = (uint8_t)d; return true;
        }
        if (key == "static_only") return jp.parseBool(p.static_only);
        if (key == "expr") {
            Props::Expression ex;
            bool ok = jp.parseObject([&](const std::string& ek) -> bool {
                if (ek == "text") return jp.parseString(ex.text);
                if (ek == "enabled") return jp.parseBool(ex.enabled);
                return false;
            });
            if (ok) p.expr = ex;
            return ok;
        }
        if (key == "keys") {
            return jp.parseArray([&]() -> bool {
                KeyframeModel::Keyframe k;
                if (!parseKeyframe(jp, k)) return false;
                p.keys.push_back(k); return true;
            });
        }
        return false;
    });
}

static bool parseNode(Parser& jp, Props::Node& n);

static bool parsePropGroup(Parser& jp, Props::PropGroup& g) {
    return jp.parseObject([&](const std::string& key) -> bool {
        if (key == "uid") return jp.parseUint64(g.uid);
        if (key == "match") return jp.parseString(g.match_id);
        if (key == "name") return jp.parseString(g.name);
        if (key == "enabled") return jp.parseBool(g.enabled);
        if (key == "label") {
            int64_t l; if (!jp.parseInt64(l)) return false;
            g.label = (uint8_t)l; return true;
        }
        if (key == "kind") {
            return jp.parseObject([&](const std::string& kk) -> bool {
                if (kk == "tag") {
                    int64_t t; if (!jp.parseInt64(t)) return false;
                    g.kind.tag = (Props::GroupKindTag)t; return true;
                }
                if (kk == "mask_mode") {
                    int64_t m; if (!jp.parseInt64(m)) return false;
                    g.kind.mask_mode = (Props::MaskMode)m; return true;
                }
                if (kk == "mask_inverted") return jp.parseBool(g.kind.mask_inverted);
                if (kk == "mask_color") {
                    int i = 0;
                    return jp.parseArray([&]() -> bool {
                        int64_t c; if (!jp.parseInt64(c)) return false;
                        if (i < 3) g.kind.mask_color[i] = (uint8_t)c;
                        ++i; return true;
                    });
                }
                if (kk == "effect") return jp.parseString(g.kind.effect_id);
                return false;
            });
        }
        if (key == "children") {
            return jp.parseArray([&]() -> bool {
                Props::Node n;
                if (!parseNode(jp, n)) return false;
                g.children.push_back(std::move(n));
                return true;
            });
        }
        return false;
    });
}

static bool parseNode(Parser& jp, Props::Node& n) {
    const char* start_pos = jp.p;
    std::string node_type;
    bool ok = jp.parseObject([&](const std::string& key) -> bool {
        if (key == "node") return jp.parseString(node_type);
        return false;
    });
    if (!ok) return false;
    jp.p = start_pos;
    if (node_type == "Group") {
        Props::PropGroup g;
        if (!parsePropGroup(jp, g)) return false;
        n = Props::Node(std::move(g));
        return true;
    } else {
        Props::Property p;
        if (!parseProperty(jp, p)) return false;
        n = Props::Node(std::move(p));
        return true;
    }
}

// =====================================================================
// Parse a Layer from JSON
// =====================================================================
static bool parseLayer(Parser& jp, Layer& l) {
    bool hasProps = false;
    bool ok = jp.parseObject([&](const std::string& key) -> bool {
        if (key == "id") {
            uint64_t v; if (!jp.parseUint64(v)) return false;
            l.id.v = v; return true;
        }
        if (key == "name") return jp.parseString(l.name);
        if (key == "kind") {
            std::string s; if (!jp.parseString(s)) return false;
            if      (s == "Solid")        l.kind = LayerKind::Solid;
            else if (s == "Shape")        l.kind = LayerKind::Shape;
            else if (s == "Text")         l.kind = LayerKind::Text;
            else if (s == "Image")        l.kind = LayerKind::Image;
            else if (s == "Footage")      l.kind = LayerKind::Footage;
            else if (s == "Composition")  l.kind = LayerKind::Composition;
            else if (s == "Null")         l.kind = LayerKind::Null;
            else if (s == "Camera")       l.kind = LayerKind::Camera;
            else if (s == "Light")        l.kind = LayerKind::Light;
            else if (s == "3D Model")     l.kind = LayerKind::Model;
            else if (s == "3D Primitive") l.kind = LayerKind::Primitive;
            return true;
        }
        if (key == "label") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            l.label = (Label)(v < 0 ? 0 : (v > 16 ? 16 : v)); return true;
        }
        if (key == "inTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            l.inPt = Tick(v); return true;
        }
        if (key == "outTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            l.outPt = Tick(v); return true;
        }
        if (key == "startTimeTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            l.startTime = Tick(v); return true;
        }
        if (key == "stretch") {
            double v; if (!jp.parseDouble(v)) return false;
            l.stretch = v; return true;
        }
        if (key == "transform") {
            return jp.parseObject([&](const std::string& k2) -> bool {
                double v;
                if (!jp.parseDouble(v)) return false;
                if      (k2=="anchorX")  l.xform.anchorX  = (float)v;
                else if (k2=="anchorY")  l.xform.anchorY  = (float)v;
                else if (k2=="posX")     l.xform.posX     = (float)v;
                else if (k2=="posY")     l.xform.posY     = (float)v;
                else if (k2=="scaleX")   l.xform.scaleX   = (float)v;
                else if (k2=="scaleY")   l.xform.scaleY   = (float)v;
                else if (k2=="rotation") l.xform.rotation = (float)v;
                else if (k2=="opacity")  l.xform.opacity  = (float)v;
                return true;
            });
        }
        if (key == "switches") {
            return jp.parseObject([&](const std::string& k2) -> bool {
                bool b;
                if (!jp.parseBool(b)) return false;
                if      (k2=="video")       l.switches.video       = b;
                else if (k2=="audio")       l.switches.audio       = b;
                else if (k2=="solo")        l.switches.solo        = b;
                else if (k2=="locked")      l.switches.locked      = b;
                else if (k2=="shy")         l.switches.shy         = b;
                else if (k2=="effects")     l.switches.effects     = b;
                else if (k2=="motionBlur")  l.switches.motionBlur  = b;
                else if (k2=="adjustment")  l.switches.adjustment  = b;
                else if (k2=="threeD")      l.switches.threeD      = b;
                else if (k2=="guide")       l.switches.guide       = b;
                return true;
            });
        }
        if (key == "solidRGBA") {
            int i = 0;
            return jp.parseArray([&]() -> bool {
                double v; if (!jp.parseDouble(v)) return false;
                if      (i==0) l.solidR = (float)v;
                else if (i==1) l.solidG = (float)v;
                else if (i==2) l.solidB = (float)v;
                else if (i==3) l.solidA = (float)v;
                ++i; return true;
            });
        }
        if (key == "width") { int v; if (!jp.parseInt(v)) return false; l.width = (uint32_t)v; return true; }
        if (key == "height") { int v; if (!jp.parseInt(v)) return false; l.height = (uint32_t)v; return true; }
        if (key == "blendMode") {
            std::string s; if (!jp.parseString(s)) return false;
            auto bm = Raster::blend_mode_from_name(s);
            if (bm.has_value()) l.blendMode = *bm;
            return true;
        }
        if (key == "preserveTransparency") return jp.parseBool(l.preserveTransparency);
        if (key == "props") {
            hasProps = true;
            return parsePropGroup(jp, l.props);
        }
        return false; // unknown key → skip
    });
    if (!ok) return false;
    if (hasProps) {
        l.syncPropsToTransform();
    } else {
        uint64_t dummy = 1000;
        l.initProps(&dummy, l.xform.anchorX, l.xform.anchorY, l.xform.posX, l.xform.posY);
    }
    return true;
}

// =====================================================================
// Parse a Composition from JSON
// =====================================================================
static bool parseComp(Parser& jp, Composition& c) {
    return jp.parseObject([&](const std::string& key) -> bool {
        if (key == "id") { uint64_t v; if (!jp.parseU64(v)) return false; c.id.v = v; return true; }
        if (key == "name") return jp.parseString(c.name);
        if (key == "width") {
            double v; if (!jp.parseDouble(v)) return false;
            c.width = (uint32_t)(v < 1 ? 1 : v > 65535 ? 65535 : v); return true;
        }
        if (key == "height") {
            double v; if (!jp.parseDouble(v)) return false;
            c.height = (uint32_t)(v < 1 ? 1 : v > 65535 ? 65535 : v); return true;
        }
        if (key == "pixelAspect") {
            double v; if (!jp.parseDouble(v)) return false;
            c.pixelAspect = (v > 0 && std::isfinite(v)) ? v : 1.0; return true;
        }
        if (key == "frameRate") {
            int64_t n = 30, d = 1;
            bool ok = jp.parseObject([&](const std::string& k2) -> bool {
                int64_t v; if (!jp.parseInt64(v)) return false;
                if (k2=="num") n = v;
                else if (k2=="den") d = v;
                return true;
            });
            if (!ok) return false;
            c.frameRate = {n, d};
            return true;
        }
        if (key == "durationTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.duration = Tick(v); return true;
        }
        if (key == "displayStartTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.displayStart = Tick(v); return true;
        }
        if (key == "workAreaStartTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.workAreaStart = Tick(v); return true;
        }
        if (key == "workAreaDurTicks") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.workAreaDur = Tick(v); return true;
        }
        if (key == "background") {
            int i = 0;
            return jp.parseArray([&]() -> bool {
                double v; if (!jp.parseDouble(v)) return false;
                if (i<3) c.background[i] = (float)v; ++i; return true;
            });
        }
        if (key == "shutterAngle") {
            double v; if (!jp.parseDouble(v)) return false;
            c.shutterAngle = v; return true;
        }
        if (key == "shutterPhase") {
            double v; if (!jp.parseDouble(v)) return false;
            c.shutterPhase = v; return true;
        }
        if (key == "motionBlurSamples") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.motionBlurSamples = (uint32_t)(v < 1 ? 1 : v > 64 ? 64 : v); return true;
        }
        if (key == "motionBlurAdaptiveLimit") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.motionBlurAdaptiveLimit = (uint32_t)(v < 1 ? 1 : v > 256 ? 256 : v); return true;
        }
        if (key == "renderer") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            c.renderer = (v == 1) ? Renderer::Advanced3D : Renderer::Classic3D; return true;
        }
        if (key == "hideShy")             { return jp.parseBool(c.hideShy); }
        if (key == "enableMotionBlur")    { return jp.parseBool(c.enableMotionBlur); }
        if (key == "enableFrameBlending") { return jp.parseBool(c.enableFrameBlending); }
        if (key == "preserveFrameRate")   { return jp.parseBool(c.preserveFrameRate); }
        if (key == "preserveResolution")  { return jp.parseBool(c.preserveResolution); }
        if (key == "resolution") {
            int64_t v; if (!jp.parseInt64(v)) return false;
            static const Resolution RS[] = {
                Resolution::Auto, Resolution::Full, Resolution::Half,
                Resolution::Third, Resolution::Quarter };
            c.resolution = (v >= 0 && v < 5) ? RS[v] : Resolution::Auto;
            return true;
        }
        if (key == "layers") {
            return jp.parseArray([&]() -> bool {
                Layer l; if (!parseLayer(jp, l)) return false;
                c.layers.push_back(l); return true;
            });
        }
        return false; // skip unknown
    });
}

// =====================================================================
// Project fromJson & load  (after EffectCraft Project::from_json_repaired)
// =====================================================================
bool ProjectData::fromJson(ProjectData& proj, const std::string& json) {
    Parser jp(json.c_str(), json.size());
    ProjectData tmp;
    bool ok = jp.parseObject([&](const std::string& key) -> bool {
        if (key == "name") return jp.parseString(tmp.name);
        if (key == "nextId") {
            uint64_t v; if (!jp.parseUint64(v)) return false;
            tmp.nextId = v; return true;
        }
        if (key == "compositions") {
            return jp.parseArray([&]() -> bool {
                Composition c;
                if (!parseComp(jp, c)) return false;
                tmp.compositions.push_back(c);
                return true;
            });
        }
        if (key == "schema" || key == "savedBy") {
            return false;
        }
        return false;
    });
    if (!ok) return false;

    // Fix nextId to be greater than all ids in the project
    // after EffectCraft Project::fix_next_id
    for (const auto& c : tmp.compositions) {
        for (const auto& l : c.layers) {
            if (l.id.v + 1 > tmp.nextId) tmp.nextId = l.id.v + 1;
            uint64_t max_p = l.props.max_uid();
            if (max_p + 1 > tmp.nextId) tmp.nextId = max_p + 1;
        }
    }

    // Sanitize comps: if duration is 0, set a default
    for (auto& c : tmp.compositions) {
        if (c.duration.v <= 0) c.duration = Tick::from_seconds_f64(10.0);
        if (c.workAreaDur.v <= 0) c.workAreaDur = c.duration;
        if (c.frameRate.num <= 0 || c.frameRate.den <= 0) c.frameRate = FrameRate::FPS_29_97;
        if (c.width  < 1 || c.width  > 65535) c.width  = 1920;
        if (c.height < 1 || c.height > 65535) c.height = 1080;
    }

    if (!tmp.compositions.empty()) tmp.activeComp = 0;
    proj = tmp;
    return true;
}

bool load(Project& proj, const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { fclose(f); return false; }
    std::string src((size_t)sz, '\0');
    if ((long)fread(src.data(), 1, (size_t)sz, f) != sz) { fclose(f); return false; }
    fclose(f);

    if (!ProjectData::fromJson(proj, src)) return false;
    proj.path  = path;
    proj.dirty = false;
    return true;
}

} // namespace Project
