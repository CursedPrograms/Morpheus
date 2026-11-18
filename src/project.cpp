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
        "\"shy\":%s,\"effects\":%s,\"motionBlur\":%s,\"adjustment\":%s,\"threeD\":%s},",
        l.switches.video?"true":"false",
        l.switches.audio?"true":"false",
        l.switches.solo?"true":"false",
        l.switches.locked?"true":"false",
        l.switches.shy?"true":"false",
        l.switches.effects?"true":"false",
        l.switches.motionBlur?"true":"false",
        l.switches.adjustment?"true":"false",
        l.switches.threeD?"true":"false");
    o += sw;
    // Solid colour
    char sc[128];
    snprintf(sc, sizeof sc, "\"solidRGBA\":[%g,%g,%g,%g]",
             l.solidR, l.solidG, l.solidB, l.solidA);
    o += sc;
    o += "}";
    return o;
}

static std::string compToJson(const Composition& c) {
    std::string o = "{";
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

bool save(Project& proj, const std::string& path) {
    std::string o = "{";
    o += "\"schema\":1,";
    o += "\"name\":" + jsonStr(proj.name) + ",";
    o += "\"nextId\":" + std::to_string(proj.nextId) + ",";
    o += "\"compositions\":[";
    for (size_t i = 0; i < proj.compositions.size(); ++i) {
        if (i > 0) o += ",";
        o += compToJson(proj.compositions[i]);
    }
    o += "]}";

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
// Parse a Layer from JSON
// =====================================================================
static bool parseLayer(Parser& jp, Layer& l) {
    return jp.parseObject([&](const std::string& key) -> bool {
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
        return false; // unknown key → skip
    });
}

// =====================================================================
// Parse a Composition from JSON
// =====================================================================
static bool parseComp(Parser& jp, Composition& c) {
    return jp.parseObject([&](const std::string& key) -> bool {
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
// Project load  (after EffectCraft Project::from_json_repaired)
// =====================================================================
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

    Parser jp(src.c_str(), src.size());
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
            // skip scalar
            return false;
        }
        return false;
    });
    if (!ok) return false;

    // Fix nextId to be greater than all ids in the project
    // after EffectCraft Project::fix_next_id
    for (const auto& c : tmp.compositions)
        for (const auto& l : c.layers)
            if (l.id.v + 1 > tmp.nextId) tmp.nextId = l.id.v + 1;

    // Sanitize comps: if duration is 0, set a default
    for (auto& c : tmp.compositions) {
        if (c.duration.v <= 0) c.duration = Tick::from_seconds_f64(10.0);
        if (c.workAreaDur.v <= 0) c.workAreaDur = c.duration;
        if (c.frameRate.num <= 0 || c.frameRate.den <= 0) c.frameRate = FrameRate::FPS_29_97;
        if (c.width  < 1 || c.width  > 65535) c.width  = 1920;
        if (c.height < 1 || c.height > 65535) c.height = 1080;
    }

    tmp.path  = path;
    tmp.dirty = false;
    if (!tmp.compositions.empty()) tmp.activeComp = 0;
    proj = tmp;
    return true;
}

} // namespace Project
