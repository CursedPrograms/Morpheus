// Morpheus - project model: compositions, layers, transforms, in/out points.
// After EffectCraft (crates/project, crates/time, crates/keyframe).
//
// Phase 2: full data model; saved/loaded as .morpheus JSON.
// Phase 1: just the structs so panels.h can forward-declare them.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Project {

// ----- Time -----
// Time in seconds (double) is used at this stage; a Tick integer (like EffectCraft's 254016000000/s) is deferred.
using TimeS = double;

// ----- Transform -----
// Every layer's animatable spatial properties (after EffectCraft crates/geom).
struct Transform {
    float anchorX  = 0, anchorY  = 0;    // anchor point (pixels from layer top-left)
    float posX     = 0, posY     = 0;    // position in composition space
    float scaleX   = 100, scaleY = 100;  // percent
    float rotation = 0;                  // degrees
    float opacity  = 100;                // percent
};

// ----- Layer kinds -----
enum class LayerKind {
    Solid,
    Shape,
    Text,
    Image,
    Footage,
    Composition,  // nested composition
    Null,
    Camera,
    Light,
};

// ----- Layer -----
struct Layer {
    std::string    name;
    LayerKind      kind   = LayerKind::Solid;
    Transform      xform;
    TimeS          inPt   = 0;    // in-point within the composition
    TimeS          outPt  = 0;    // out-point within the composition
    bool           visible = true;
    bool           solo    = false;
    bool           locked  = false;

    // Phase-specific extras (more added in later phases).
    // Solid colour (RGBA 0..255) used when kind == Solid.
    uint8_t        solidR = 128, solidG = 128, solidB = 128, solidA = 255;
};

// ----- Composition -----
struct Composition {
    std::string         name;
    int                 width    = 1920;
    int                 height   = 1080;
    double              fps      = 30.0;    // frames per second
    TimeS               duration = 10.0;    // seconds
    std::vector<Layer>  layers;
};

// ----- Project -----
struct Project {
    std::string               name;
    std::string               path;    // empty = not yet saved
    bool                      dirty = false;
    std::vector<Composition>  compositions;
    int                       activeComp = -1;  // index into compositions, or -1

    // Create a default new project with one composition.
    static Project makeNew();
};

// Save project to path as JSON; returns true on success.
bool save(Project& proj, const std::string& path);

// Load project from path; returns true on success.
bool load(Project& proj, const std::string& path);

}  // namespace Project
