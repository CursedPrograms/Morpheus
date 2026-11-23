// Timeline time mapping, zoom and playhead helpers.
// after EffectCraft (crates/ui-egui/src/panels/timeline.rs TMap, fit_pps, set_zoom)
#pragma once

#include "time_.h"
#include "props.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Project {
struct Composition;
}

namespace Timeline {

// Pixels per second that fit the composition in the time graph.
double fitPps(double width, double duration);

// Zoom around a time and its pixel offset, preserving the anchored point.
void setZoom(double& start, double& pixelsPerSecond, double requestedPps,
             double fit, double duration, double atTime, double atPixel);

// Keep the current view when the playhead is visible; otherwise page to it.
double pageToCti(double start, double span, double duration, double cti);

// Center the view on the current time without scrolling past the composition.
double centeredOn(double span, double duration, double cti);

// Horizontal auto-scroll speed when a drag extends beyond the visible timeline.
double edgeScrollSpeedX(double x0, double x1, double x);

// Snap to the closest candidate within the given time tolerance.
double snapTime(const double* candidates, size_t count, double time, double tolerance);

// Convert a horizontal position to the nearest frame in the composition.
Project::Tick timeAtPixel(double x, double x0, double start, double pps,
                          Project::FrameRate frameRate, Project::Tick duration);

// Restrict a Shift-drag to its dominant axis; without Shift both axes remain available.
std::pair<bool, bool> shiftAxes(bool shift, float dx, float dy);

// Convert a dragged Bezier handle endpoint into temporal speed and influence.
KeyframeModel::Ease easeAtHandle(KeyframeModel::Ease current, double keyTime, double keyValue,
                                 double desiredTime, double desiredValue, double duration,
                                 bool out, bool speedGraph);

enum class BarEdit { Move, TrimIn, TrimOut };

struct LayerTiming {
    Project::Tick in = Project::Tick::ZERO;
    Project::Tick out = Project::Tick::ZERO;
    Project::Tick start = Project::Tick::ZERO;
};

struct KeyMarker {
    uint64_t layer = 0;
    uint64_t property = 0;
    Project::Tick time = Project::Tick::ZERO;
    float x = 0.0f;
    float y = 0.0f;
    float glyphSize = 9.0f;
};

struct KeyRef {
    uint64_t layer = 0;
    uint64_t property = 0;
    Project::Tick time = Project::Tick::ZERO;
};

struct KeyMoveResult {
    std::vector<KeyRef> selected;
    std::optional<KeyRef> grabbed;
};

// Move selected keyframes by a composition-time delta; locked keys remain selected.
KeyMoveResult moveSelectedKeys(Project::Composition& comp,
                               const std::vector<KeyRef>& selected,
                               Project::Tick compDelta, KeyRef grabbed,
                               const std::optional<KeyframeModel::Value>& replacement = std::nullopt);

// Replace, add, or toggle a selected key reference; return whether the key remains selected.
bool selectKey(std::vector<KeyRef>& selected, KeyRef key, bool toggle);

// Return the visible key glyphs intersected by a marquee, in row/key draw order.
std::vector<KeyRef> keysInBox(const std::vector<KeyMarker>& markers,
                              float minX, float minY, float maxX, float maxY);

// Apply a frame-quantized move or trim to a layer's composition-time range.
LayerTiming editLayerTiming(LayerTiming original, BarEdit edit, Project::Tick offset,
                            Project::Tick frameDuration, Project::Tick compDuration);

// Add a key at layer time or remove the key there, preserving the last value.
bool toggleKeyframe(Props::Property& property, Project::Tick layerTime);

// Move selected layer IDs in stack order before a target, or to the stack end.
std::vector<uint64_t> reorderLayers(const std::vector<uint64_t>& order,
                                    const std::vector<uint64_t>& moving,
                                    std::optional<uint64_t> before);

} // namespace Timeline
