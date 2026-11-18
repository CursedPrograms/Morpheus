// Morpheus - project model: compositions, layers, transforms, in/out points.
// Ported function-by-function from EffectCraft crates/project.
//
// Covers (from REVIEW.md item 1):
//   - Tick / FrameRate time (from time_.h, re-exported here)
//   - ItemId, LayerId
//   - Project, Comp, Layer with all Switches fields
//   - Transform as the property tree base (phase 5 adds keyframes)
//   - LightKind, PrimitiveKind, LayerSource, Quality, Sampling, FrameBlend,
//     TrackMatte / MatteKind, AutoOrient, Label
//   - Save/load as .morpheus JSON
//
// Deferred to later phases: render_queue, render_templates, essential,
// tracking, styles, footage (struct fields present but not fully loaded).
//
// after EffectCraft (crates/project/src/item.rs, comp.rs, layer.rs,
//                    crates/time/src/lib.rs)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "time_.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Project {

// ---- ItemId / LayerId ------------------------------------------------
// after EffectCraft crates/project/src/item.rs ItemId, LayerId

struct ItemId {
    uint64_t v = 0;
    bool operator==(ItemId o) const { return v == o.v; }
    bool operator!=(ItemId o) const { return v != o.v; }
    bool operator< (ItemId o) const { return v <  o.v; }
};

struct LayerId {
    uint64_t v = 0;
    bool operator==(LayerId o) const { return v == o.v; }
    bool operator!=(LayerId o) const { return v != o.v; }
    bool operator< (LayerId o) const { return v <  o.v; }
};

// Explicit "none" in a layer parameter whose unset value means the owning layer.
// after EffectCraft crates/project/src/item.rs NO_LAYER
static constexpr uint64_t NO_LAYER = 0;

// ---- Label -----------------------------------------------------------
// after EffectCraft crates/engine/src/color.rs Label
enum class Label {
    None = 0, Red, Yellow, Aqua, Pink, Lavender, Peach, SeaFoam,
    Blue, Green, Purple, Orange, Brown, Fuchsia, Cyan, Sandstone, DarkGreen
};

// ---- LightKind -------------------------------------------------------
// after EffectCraft crates/project/src/layer.rs LightKind
enum class LightKind { Parallel, Spot, Point, Ambient, Environment };
static inline const char* lightKindLabel(LightKind k) {
    switch (k) {
    case LightKind::Parallel:   return "Parallel";
    case LightKind::Spot:       return "Spot";
    case LightKind::Point:      return "Point";
    case LightKind::Ambient:    return "Ambient";
    case LightKind::Environment:return "Environment";
    }
    return "Point";
}

// ---- PrimitiveKind ---------------------------------------------------
// after EffectCraft crates/project/src/layer.rs PrimitiveKind
enum class PrimitiveKind { Cube, Sphere, Plane, Torus, Cone, Cylinder };
static inline const char* primitiveKindLabel(PrimitiveKind k) {
    switch (k) {
    case PrimitiveKind::Cube:     return "Cube";
    case PrimitiveKind::Sphere:   return "Sphere";
    case PrimitiveKind::Plane:    return "Plane";
    case PrimitiveKind::Torus:    return "Torus";
    case PrimitiveKind::Cone:     return "Cone";
    case PrimitiveKind::Cylinder: return "Cylinder";
    }
    return "Cube";
}

// ---- LayerKind / LayerSource -----------------------------------------
// after EffectCraft crates/project/src/layer.rs LayerSource
// (Morpheus uses a simpler enum for the phase 2 panel display;
//  the full LayerSource with item refs is added in phase 3+)
enum class LayerKind {
    Solid,
    Shape,
    Text,
    Image,       // still-image footage
    Footage,     // video/audio footage
    Composition, // nested composition (precomp)
    Null,
    Camera,
    Light,
    Model,       // 3D model (Advanced 3D, phase 10)
    Primitive,   // 3D primitive (Advanced 3D, phase 10)
};

static inline const char* layerKindTypeName(LayerKind k) {
    switch (k) {
    case LayerKind::Solid:        return "Solid";
    case LayerKind::Shape:        return "Shape";
    case LayerKind::Text:         return "Text";
    case LayerKind::Image:        return "Image";
    case LayerKind::Footage:      return "Footage";
    case LayerKind::Composition:  return "Composition";
    case LayerKind::Null:         return "Null";
    case LayerKind::Camera:       return "Camera";
    case LayerKind::Light:        return "Light";
    case LayerKind::Model:        return "3D Model";
    case LayerKind::Primitive:    return "3D Primitive";
    }
    return "Solid";
}

// ---- Quality / Sampling / FrameBlend / MatteKind / AutoOrient --------
// after EffectCraft crates/project/src/layer.rs

enum class Quality    { Best, Draft, Wireframe };
enum class Sampling   { Bilinear, Bicubic };
enum class FrameBlend { Off, FrameMix, PixelMotion };

// after EffectCraft crates/project/src/layer.rs TrackMatte / MatteKind
enum class MatteKind  { Alpha, AlphaInverse, Luma, LumaInverse };

// after EffectCraft crates/project/src/layer.rs AutoOrient
enum class AutoOrient { None, AlongPath, Camera };

// ---- Switches --------------------------------------------------------
// after EffectCraft crates/project/src/layer.rs Switches
// Every field that layer.rs::Switches has.
struct Switches {
    bool video           = true;
    bool audio           = true;
    bool solo            = false;
    bool locked          = false;
    bool shy             = false;
    bool collapse        = false;  // collapse transformations
    bool effects         = true;
    bool frameBlend      = false;
    bool motionBlur      = false;
    bool adjustment      = false;
    bool threeD          = false;
    bool environmentLayer = false;
    // Quality / sampling (stored as switches in AE / EffectCraft)
    Quality    quality   = Quality::Best;
    Sampling   sampling  = Sampling::Bilinear;
    FrameBlend frameBlendKind = FrameBlend::Off;
};

// ---- Transform -------------------------------------------------------
// Every layer's animatable spatial properties.
// after EffectCraft crates/project/src/layer.rs transform group
// (keyframes added in phase 5; for now these are static values)
struct Transform {
    float anchorX  = 0, anchorY  = 0;
    float posX     = 0, posY     = 0;
    float scaleX   = 100, scaleY = 100;
    float rotation = 0;    // degrees
    float opacity  = 100;  // percent
};

// ---- Layer -----------------------------------------------------------
// after EffectCraft crates/project/src/layer.rs Layer
struct Layer {
    LayerId    id;
    std::string name;
    LayerKind  kind    = LayerKind::Solid;
    Label      label   = Label::None;
    std::string comment;

    // Source item reference (used when kind is Footage/Composition/Solid etc.)
    // after EffectCraft LayerSource::item()
    ItemId     sourceItem;   // zero = no item

    // For Light layers
    LightKind     lightKind   = LightKind::Point;
    PrimitiveKind primitiveKind = PrimitiveKind::Cube;

    // Transform
    Transform xform;

    // Timing (in Ticks, as EffectCraft uses)
    // after EffectCraft crates/project/src/layer.rs in_point, out_point, start_time, stretch
    Tick inPt;   // in-point within the composition
    Tick outPt;  // out-point within the composition
    Tick startTime; // layer offset from comp time 0 (default: same as inPt)
    double stretch = 1.0; // time stretch factor

    Switches switches;

    // Track matte
    // after EffectCraft crates/project/src/layer.rs track_matte
    bool     hasMatte   = false;
    MatteKind matteKind = MatteKind::Alpha;
    LayerId  matteLayer;  // zero = none

    // Parenting
    // after EffectCraft crates/project/src/layer.rs parent
    LayerId  parent;  // zero = none

    // Auto-orient
    AutoOrient autoOrient = AutoOrient::None;

    // Solid colour (RGBA 0..1) for Solid layers.
    float solidR = 0.5f, solidG = 0.5f, solidB = 0.5f, solidA = 1.0f;

    // Convenience: is this layer an AV layer (has pixels)?
    // after EffectCraft LayerSource::is_av
    bool isAV() const {
        return kind != LayerKind::Camera && kind != LayerKind::Light && kind != LayerKind::Null;
    }

    // Is the layer active at composition time t?
    // after EffectCraft crates/project/src/layer.rs Layer::is_active_at
    bool isActiveAt(Tick t) const {
        return switches.video && t >= inPt && t < outPt;
    }
};

// ---- Renderer / Resolution -------------------------------------------
// after EffectCraft crates/project/src/comp.rs Renderer, Resolution

enum class Renderer { Classic3D, Advanced3D };

enum class Resolution { Auto, Full, Half, Third, Quarter };
static inline const char* resolutionLabel(Resolution r) {
    switch (r) {
    case Resolution::Auto:    return "Auto";
    case Resolution::Full:    return "Full";
    case Resolution::Half:    return "Half";
    case Resolution::Third:   return "Third";
    case Resolution::Quarter: return "Quarter";
    }
    return "Auto";
}

// ---- Guide -----------------------------------------------------------
// after EffectCraft crates/project/src/comp.rs Guide
struct Guide {
    bool   vertical = true;
    double position = 0;
    bool   percent  = false;
};

// ---- Composition -----------------------------------------------------
// after EffectCraft crates/project/src/comp.rs Comp
struct Composition {
    std::string  name;
    uint32_t     width         = 1920;
    uint32_t     height        = 1080;
    double       pixelAspect   = 1.0;
    FrameRate    frameRate     = FrameRate::FPS_30;
    Tick         duration;         // comp duration in ticks
    Tick         displayStart;     // start timecode offset (default 0)
    float        background[3]  = {0.f, 0.f, 0.f};
    Tick         workAreaStart;
    Tick         workAreaDur;
    double       shutterAngle  = 180.0;
    double       shutterPhase  = -90.0;
    uint32_t     motionBlurSamples        = 16;
    uint32_t     motionBlurAdaptiveLimit  = 128;
    Renderer     renderer      = Renderer::Classic3D;
    bool         hideShy       = false;
    bool         enableMotionBlur    = true;
    bool         enableFrameBlending = true;
    bool         draft3D       = false;
    bool         preserveFrameRate   = false;
    bool         preserveResolution  = false;
    Resolution   resolution    = Resolution::Auto;
    std::vector<Layer>  layers;
    std::vector<Guide>  guides;

    // Convenience: frame duration
    // after EffectCraft Comp::frame_duration
    Tick frameDuration() const { return frameRate.frame_duration(); }
    // Duration in seconds (double), for display
    double durationSeconds() const { return duration.seconds(); }

    // Legacy accessor: fps as double
    double fps() const { return frameRate.as_f64(); }
};

// ---- ItemKind --------------------------------------------------------
// after EffectCraft crates/project/src/item.rs ItemKind
enum class ItemKind { Folder, Comp, Footage, Solid };
static inline const char* itemKindTypeName(ItemKind k) {
    switch (k) {
    case ItemKind::Folder:  return "Folder";
    case ItemKind::Comp:    return "Composition";
    case ItemKind::Footage: return "Footage";
    case ItemKind::Solid:   return "Solid";
    }
    return "Folder";
}

// ---- Project ---------------------------------------------------------
// after EffectCraft crates/project/src/item.rs / lib.rs Project
struct ProjectData {
    std::string              name;
    std::string              path;      // empty = not yet saved
    bool                     dirty = false;
    std::vector<Composition> compositions;
    int                      activeComp = -1;  // index into compositions, or -1
    uint64_t                 nextId     = 1;   // monotonic id counter

    // Create a default new project with one composition.
    // after EffectCraft Project::default
    static ProjectData makeNew();

    // Allocate a unique id.
    // after EffectCraft Project::alloc
    uint64_t alloc() { return nextId++; }
};
// Alias for backward compatibility
using Project = ProjectData;

// ---- File I/O --------------------------------------------------------
// Save project to path as JSON; returns true on success.
bool save(ProjectData& proj, const std::string& path);
// Load project from path; returns true on success.
bool load(ProjectData& proj, const std::string& path);

} // namespace Project
