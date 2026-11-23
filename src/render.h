// Morpheus - Composition frame renderer.
// Ported from EffectCraft:
//   crates/render/src/pipeline.rs
//   crates/render/src/layers.rs
//   crates/render/src/masks.rs
//   crates/render/src/frames.rs
// after EffectCraft (crates/render/src/pipeline.rs, crates/render/src/layers.rs, crates/render/src/masks.rs, crates/render/src/frames.rs)
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "project.h"
#include "raster.h"
#include "time_.h"

#include <vector>
#include <optional>
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>

namespace Render {

struct RenderOpts {
    double scale = 1.0;
    std::optional<std::array<double, 4>> roi = std::nullopt; // [x, y, w, h] in comp space
    int depth = 0; // precomp nesting recursion limit (max 16)
    bool guides = false;
};

class Renderer {
public:
    static Raster::Image render_frame(const Project::Project& project, uint64_t comp_id, Project::Tick t, double scale = 1.0);
    static Raster::Image render_frame_opts(const Project::Project& project, uint64_t comp_id, Project::Tick t, const RenderOpts& opts);

    // Source image of a layer in layer space (before masks/transforms)
    static Raster::Image layer_source(const Project::Project& project, const Project::Composition& comp, const Project::Layer& layer, Project::Tick t, double scale);

    // Evaluated 2D transform matrix from layer space to composition space
    static Raster::Mat3 layer_matrix_at(const Project::Composition& comp, const Project::Layer& layer, Project::Tick t, double scale = 1.0);

    // Evaluated layer opacity (0.0 .. 1.0)
    static float layer_opacity_at(const Project::Layer& layer, Project::Tick t);

    // Apply masks in layer space
    static void apply_masks(const Project::Layer& layer, Raster::Image& img, Project::Tick t, double scale);
};

// A byte-bounded LRU cache of rendered composition frames for timeline playback.
class FrameCache {
public:
    explicit FrameCache(size_t budgetBytes = 128u * 1024u * 1024u);

    const Raster::Image* find(uint64_t compId, int64_t frame, double scale);
    void insert(uint64_t compId, int64_t frame, double scale, Raster::Image image);
    void clear();
    size_t size() const { return frames_.size(); }

private:
    struct Entry {
        uint64_t compId;
        int64_t frame;
        double scale;
        Raster::Image image;
    };

    size_t budgetBytes_;
    size_t usedBytes_ = 0;
    std::list<Entry> frames_;
};

// Top-level convenience function matching EffectCraft unit tests:
// render_frame(&p, cid, Tick::ZERO, 1.0)
inline Raster::Image render_frame(const Project::Project* project, uint64_t comp_id, Project::Tick t, double scale = 1.0) {
    if (!project) return Raster::Image(1, 1);
    return Renderer::render_frame(*project, comp_id, t, scale);
}

} // namespace Render
