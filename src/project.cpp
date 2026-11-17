// Morpheus - project model: compositions, layers, transforms.
// After EffectCraft (crates/project, crates/time, crates/keyframe).
// Saved/loaded as .morpheus JSON (hand-written serialiser; no external library needed).
#include "project.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace Project {

// ----- Project::makeNew -----
// Returns a new project with one default composition (1920x1080, 30 fps, 10 s).
Project Project::makeNew() {
    Project p;
    p.name  = "Untitled";
    p.dirty = true;

    Composition comp;
    comp.name     = "Composition 1";
    comp.width    = 1920;
    comp.height   = 1080;
    comp.fps      = 30.0;
    comp.duration = 10.0;
    p.compositions.push_back(std::move(comp));
    p.activeComp = 0;
    return p;
}

// ---------- minimal JSON helpers ----------
// We write JSON by hand to keep dependencies small; a proper parser comes in a later phase.

// Escape a string for JSON output.
static std::string jsonStr(const std::string& s) {
    std::string out;
    out += '"';
    for (unsigned char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += (char)c;
    }
    out += '"';
    return out;
}

// Advance past whitespace and the given character (returns false if not found).
static bool skipWS(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return true;
}

static bool eat(const char*& p, char c) {
    skipWS(p);
    if (*p != c) return false;
    ++p;
    return true;
}

// Parse a JSON string into dst; returns false on parse error.
static bool parseStr(const char*& p, std::string& dst) {
    skipWS(p);
    if (*p != '"') return false;
    ++p;
    dst.clear();
    while (*p && *p != '"') {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case '"':  dst += '"';  break;
                case '\\': dst += '\\'; break;
                case 'n':  dst += '\n'; break;
                case 'r':  dst += '\r'; break;
                case 't':  dst += '\t'; break;
                default:   dst += *p;   break;
            }
        } else {
            dst += *p;
        }
        ++p;
    }
    if (*p == '"') ++p;
    return true;
}

// Parse a JSON number (double) into dst.
static bool parseNum(const char*& p, double& dst) {
    skipWS(p);
    char* end = nullptr;
    dst = strtod(p, &end);
    if (end == p) return false;
    p = end;
    return true;
}

// Parse a JSON integer into dst.
static bool parseInt(const char*& p, int& dst) {
    double d = 0;
    if (!parseNum(p, d)) return false;
    dst = (int)d;
    return true;
}

// Parse a JSON bool into dst.
static bool parseBool(const char*& p, bool& dst) {
    skipWS(p);
    if (strncmp(p, "true", 4) == 0)  { dst = true;  p += 4; return true; }
    if (strncmp(p, "false", 5) == 0) { dst = false; p += 5; return true; }
    return false;
}

// Skip a JSON value (string, number, bool, null, object, array) without parsing it.
static void skipValue(const char*& p);

static void skipObject(const char*& p) {
    // p is at '{'
    ++p;
    while (*p) {
        skipWS(p);
        if (*p == '}') { ++p; return; }
        if (*p == '"') { std::string k; parseStr(p, k); }
        eat(p, ':');
        skipValue(p);
        skipWS(p);
        if (*p == ',') ++p;
    }
}

static void skipArray(const char*& p) {
    ++p;
    while (*p) {
        skipWS(p);
        if (*p == ']') { ++p; return; }
        skipValue(p);
        skipWS(p);
        if (*p == ',') ++p;
    }
}

static void skipValue(const char*& p) {
    skipWS(p);
    if (*p == '"') { std::string s; parseStr(p, s); }
    else if (*p == '{') skipObject(p);
    else if (*p == '[') skipArray(p);
    else if (strncmp(p, "true", 4) == 0)  p += 4;
    else if (strncmp(p, "false", 5) == 0) p += 5;
    else if (strncmp(p, "null", 4) == 0)  p += 4;
    else { char* e = nullptr; strtod(p, &e); if (e && e != p) p = e; else ++p; }
}

// ----- Serialisation -----

static std::string serTransform(const Transform& t) {
    char buf[256];
    snprintf(buf, sizeof buf,
        "{\"ax\":%.3f,\"ay\":%.3f,\"px\":%.3f,\"py\":%.3f,"
        "\"sx\":%.3f,\"sy\":%.3f,\"rot\":%.3f,\"op\":%.3f}",
        t.anchorX, t.anchorY, t.posX, t.posY,
        t.scaleX, t.scaleY, t.rotation, t.opacity);
    return buf;
}

static std::string kindToStr(LayerKind k) {
    switch (k) {
        case LayerKind::Solid:       return "solid";
        case LayerKind::Shape:       return "shape";
        case LayerKind::Text:        return "text";
        case LayerKind::Image:       return "image";
        case LayerKind::Footage:     return "footage";
        case LayerKind::Composition: return "composition";
        case LayerKind::Null:        return "null";
        case LayerKind::Camera:      return "camera";
        case LayerKind::Light:       return "light";
        default:                     return "solid";
    }
}

static LayerKind strToKind(const std::string& s) {
    if (s == "shape")       return LayerKind::Shape;
    if (s == "text")        return LayerKind::Text;
    if (s == "image")       return LayerKind::Image;
    if (s == "footage")     return LayerKind::Footage;
    if (s == "composition") return LayerKind::Composition;
    if (s == "null")        return LayerKind::Null;
    if (s == "camera")      return LayerKind::Camera;
    if (s == "light")       return LayerKind::Light;
    return LayerKind::Solid;
}

static std::string serLayer(const Layer& l) {
    std::ostringstream o;
    o << "{"
      << "\"name\":" << jsonStr(l.name) << ","
      << "\"kind\":" << jsonStr(kindToStr(l.kind)) << ","
      << "\"in\":"   << l.inPt  << ","
      << "\"out\":"  << l.outPt << ","
      << "\"visible\":" << (l.visible ? "true" : "false") << ","
      << "\"solo\":"    << (l.solo    ? "true" : "false") << ","
      << "\"locked\":"  << (l.locked  ? "true" : "false") << ","
      << "\"solidR\":" << (int)l.solidR << ","
      << "\"solidG\":" << (int)l.solidG << ","
      << "\"solidB\":" << (int)l.solidB << ","
      << "\"solidA\":" << (int)l.solidA << ","
      << "\"xform\":"  << serTransform(l.xform)
      << "}";
    return o.str();
}

static std::string serComp(const Composition& c) {
    std::ostringstream o;
    o << "{"
      << "\"name\":"     << jsonStr(c.name)   << ","
      << "\"width\":"    << c.width           << ","
      << "\"height\":"   << c.height          << ","
      << "\"fps\":"      << c.fps             << ","
      << "\"duration\":" << c.duration        << ","
      << "\"layers\":[";
    for (size_t i = 0; i < c.layers.size(); ++i) {
        if (i > 0) o << ",";
        o << serLayer(c.layers[i]);
    }
    o << "]}";
    return o.str();
}

bool save(Project& proj, const std::string& path) {
    std::ostringstream o;
    o << "{\n"
      << "  \"morpheus\":\"1\",\n"
      << "  \"name\":"       << jsonStr(proj.name) << ",\n"
      << "  \"activeComp\":" << proj.activeComp    << ",\n"
      << "  \"compositions\":[\n";
    for (size_t i = 0; i < proj.compositions.size(); ++i) {
        if (i > 0) o << ",\n";
        o << "    " << serComp(proj.compositions[i]);
    }
    o << "\n  ]\n}\n";

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::string s = o.str();
    const bool ok = fwrite(s.data(), 1, s.size(), f) == s.size();
    fclose(f);
    return ok;
}

// ----- Deserialisation -----

static bool parseTransform(const char*& p, Transform& t) {
    if (!eat(p, '{')) return false;
    while (*p) {
        skipWS(p);
        if (*p == '}') { ++p; break; }
        std::string key;
        if (!parseStr(p, key)) return false;
        if (!eat(p, ':')) return false;
        double v = 0;
        parseNum(p, v);
        if      (key == "ax")  t.anchorX  = (float)v;
        else if (key == "ay")  t.anchorY  = (float)v;
        else if (key == "px")  t.posX     = (float)v;
        else if (key == "py")  t.posY     = (float)v;
        else if (key == "sx")  t.scaleX   = (float)v;
        else if (key == "sy")  t.scaleY   = (float)v;
        else if (key == "rot") t.rotation = (float)v;
        else if (key == "op")  t.opacity  = (float)v;
        skipWS(p);
        if (*p == ',') ++p;
    }
    return true;
}

static bool parseLayer(const char*& p, Layer& l) {
    if (!eat(p, '{')) return false;
    while (*p) {
        skipWS(p);
        if (*p == '}') { ++p; break; }
        std::string key;
        if (!parseStr(p, key)) return false;
        if (!eat(p, ':')) return false;
        if (key == "name")    { parseStr(p, l.name); }
        else if (key == "kind")  { std::string s; parseStr(p, s); l.kind = strToKind(s); }
        else if (key == "in")    { double v = 0; parseNum(p, v); l.inPt  = v; }
        else if (key == "out")   { double v = 0; parseNum(p, v); l.outPt = v; }
        else if (key == "visible") parseBool(p, l.visible);
        else if (key == "solo")    parseBool(p, l.solo);
        else if (key == "locked")  parseBool(p, l.locked);
        else if (key == "solidR") { int v = 0; parseInt(p, v); l.solidR = (uint8_t)v; }
        else if (key == "solidG") { int v = 0; parseInt(p, v); l.solidG = (uint8_t)v; }
        else if (key == "solidB") { int v = 0; parseInt(p, v); l.solidB = (uint8_t)v; }
        else if (key == "solidA") { int v = 0; parseInt(p, v); l.solidA = (uint8_t)v; }
        else if (key == "xform")  parseTransform(p, l.xform);
        else skipValue(p);
        skipWS(p);
        if (*p == ',') ++p;
    }
    return true;
}

static bool parseComp(const char*& p, Composition& c) {
    if (!eat(p, '{')) return false;
    while (*p) {
        skipWS(p);
        if (*p == '}') { ++p; break; }
        std::string key;
        if (!parseStr(p, key)) return false;
        if (!eat(p, ':')) return false;
        if (key == "name")     parseStr(p, c.name);
        else if (key == "width")    { double v = 0; parseNum(p, v); c.width  = (int)v; }
        else if (key == "height")   { double v = 0; parseNum(p, v); c.height = (int)v; }
        else if (key == "fps")      { parseNum(p, c.fps); }
        else if (key == "duration") { parseNum(p, c.duration); }
        else if (key == "layers") {
            eat(p, '[');
            while (*p) {
                skipWS(p);
                if (*p == ']') { ++p; break; }
                Layer l;
                parseLayer(p, l);
                c.layers.push_back(std::move(l));
                skipWS(p);
                if (*p == ',') ++p;
            }
        } else skipValue(p);
        skipWS(p);
        if (*p == ',') ++p;
    }
    return true;
}

bool load(Project& proj, const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { fclose(f); return false; } // reject huge files
    std::string buf(sz, '\0');
    const bool ok = fread(&buf[0], 1, sz, f) == (size_t)sz;
    fclose(f);
    if (!ok) return false;

    proj = Project();
    proj.path = path;
    const char* p = buf.c_str();
    if (!eat(p, '{')) return false;
    while (*p) {
        skipWS(p);
        if (*p == '}') break;
        std::string key;
        if (!parseStr(p, key)) return false;
        if (!eat(p, ':')) return false;
        if (key == "name")       parseStr(p, proj.name);
        else if (key == "activeComp") parseInt(p, proj.activeComp);
        else if (key == "compositions") {
            eat(p, '[');
            while (*p) {
                skipWS(p);
                if (*p == ']') { ++p; break; }
                Composition c;
                parseComp(p, c);
                proj.compositions.push_back(std::move(c));
                skipWS(p);
                if (*p == ',') ++p;
            }
        } else skipValue(p);
        skipWS(p);
        if (*p == ',') ++p;
    }
    proj.dirty = false;
    return true;
}

}  // namespace Project
