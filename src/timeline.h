// Timeline time mapping, zoom and playhead helpers.
// after EffectCraft (crates/ui-egui/src/panels/timeline.rs TMap, fit_pps, set_zoom)
#pragma once

#include "time_.h"
#include <cstddef>

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

// Snap to the closest candidate within the given time tolerance.
double snapTime(const double* candidates, size_t count, double time, double tolerance);

// Convert a horizontal position to the nearest frame in the composition.
Project::Tick timeAtPixel(double x, double x0, double start, double pps,
                          Project::FrameRate frameRate, Project::Tick duration);

enum class BarEdit { Move, TrimIn, TrimOut };

struct LayerTiming {
    Project::Tick in = Project::Tick::ZERO;
    Project::Tick out = Project::Tick::ZERO;
    Project::Tick start = Project::Tick::ZERO;
};

// Apply a frame-quantized move or trim to a layer's composition-time range.
LayerTiming editLayerTiming(LayerTiming original, BarEdit edit, Project::Tick offset,
                            Project::Tick frameDuration, Project::Tick compDuration);

} // namespace Timeline
