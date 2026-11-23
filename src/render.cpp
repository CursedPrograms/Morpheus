// Morpheus - Composition frame renderer.
// Ported from EffectCraft:
//   crates/render/src/pipeline.rs
//   crates/render/src/layers.rs
//   crates/render/src/masks.rs
//   crates/render/src/frames.rs
// after EffectCraft (crates/render/src/pipeline.rs, crates/render/src/layers.rs, crates/render/src/masks.rs, crates/render/src/frames.rs)
#include "render.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <iterator>

namespace Render {

FrameCache::FrameCache(size_t budgetBytes)
    : budgetBytes_(budgetBytes) {}

const Raster::Image* FrameCache::find(uint64_t compId, int64_t frame, double scale) {
    for (auto it = frames_.begin(); it != frames_.end(); ++it) {
        if (it->compId == compId && it->frame == frame && it->scale == scale) {
            frames_.splice(frames_.begin(), frames_, it);
            return &frames_.front().image;
        }
    }
    return nullptr;
}

void FrameCache::insert(uint64_t compId, int64_t frame, double scale, Raster::Image image) {
    for (auto it = frames_.begin(); it != frames_.end();) {
        if (it->compId == compId && it->frame == frame && it->scale == scale) {
            usedBytes_ -= it->image.data.size() * sizeof(Raster::Px);
            it = frames_.erase(it);
        } else {
            ++it;
        }
    }
    const size_t bytes = image.data.size() * sizeof(Raster::Px);
    if (bytes == 0 || bytes > budgetBytes_)
        return;
    frames_.push_front({compId, frame, scale, std::move(image)});
    usedBytes_ += bytes;
    while (usedBytes_ > budgetBytes_ || frames_.size() > 120) {
        usedBytes_ -= frames_.back().image.data.size() * sizeof(Raster::Px);
        frames_.pop_back();
    }
}

void FrameCache::clear() {
    frames_.clear();
    usedBytes_ = 0;
}

// =====================================================================
// Transform & Opacity Evaluation
// =====================================================================

// after EffectCraft crates/render/src/layers.rs layer_matrix_at
Raster::Mat3 Renderer::layer_matrix_at(const Project::Composition& comp, const Project::Layer& layer, Project::Tick t, double scale) {
    const Project::Tick layerTime = layer.layerTime(t);
    Raster::Vec2 anchor{0.0, 0.0};
    Raster::Vec2 pos{0.0, 0.0};
    Raster::Vec2 sc{100.0, 100.0};
    double rot = 0.0;

    // Anchor
    if (const auto* p = layer.props.prop("transform/anchor")) {
        auto val = p->value_at(layerTime);
        auto a = val.as_vec2();
        anchor = {a[0], a[1]};
    } else {
        anchor = {(double)layer.xform.anchorX, (double)layer.xform.anchorY};
    }

    // Position
    if (const auto* p = layer.props.prop("transform/position")) {
        auto val = p->value_at(layerTime);
        auto a = val.as_vec2();
        pos = {a[0], a[1]};
    } else {
        pos = {(double)layer.xform.posX, (double)layer.xform.posY};
    }

    // Scale
    if (const auto* p = layer.props.prop("transform/scale")) {
        auto val = p->value_at(layerTime);
        auto a = val.as_vec2();
        sc = {a[0], a[1]};
    } else {
        sc = {(double)layer.xform.scaleX, (double)layer.xform.scaleY};
    }

    // Rotation
    if (const auto* p = layer.props.prop("transform/rotation")) {
        auto val = p->value_at(layerTime);
        rot = val.as_f64();
    } else {
        rot = (double)layer.xform.rotation;
    }

    // After Effects 2D layer transform:
    // M = Translate(Pos) * Rotate(Rot) * Scale(Scale/100) * Translate(-Anchor)
    Raster::Mat3 l2c = Raster::Mat3::layer_2d(anchor, pos, sc, rot);

    // Parent layer chain
    uint64_t currentParent = layer.parent.v;
    int guard = 0;
    while (currentParent != 0 && guard < 32) {
        const Project::Layer* parentLayer = nullptr;
        for (const auto& l : comp.layers) {
            if (l.id.v == currentParent) {
                parentLayer = &l;
                break;
            }
        }
        if (!parentLayer || parentLayer->id.v == layer.id.v) break;

        Raster::Vec2 p_anchor{0.0, 0.0};
        Raster::Vec2 p_pos{0.0, 0.0};
        Raster::Vec2 p_sc{100.0, 100.0};
        double p_rot = 0.0;
        const Project::Tick parentTime = parentLayer->layerTime(t);

        if (const auto* p = parentLayer->props.prop("transform/anchor")) {
            auto val = p->value_at(parentTime);
            auto a = val.as_vec2();
            p_anchor = {a[0], a[1]};
        } else {
            p_anchor = {(double)parentLayer->xform.anchorX, (double)parentLayer->xform.anchorY};
        }

        if (const auto* p = parentLayer->props.prop("transform/position")) {
            auto val = p->value_at(parentTime);
            auto a = val.as_vec2();
            p_pos = {a[0], a[1]};
        } else {
            p_pos = {(double)parentLayer->xform.posX, (double)parentLayer->xform.posY};
        }

        if (const auto* p = parentLayer->props.prop("transform/scale")) {
            auto val = p->value_at(parentTime);
            auto a = val.as_vec2();
            p_sc = {a[0], a[1]};
        } else {
            p_sc = {(double)parentLayer->xform.scaleX, (double)parentLayer->xform.scaleY};
        }

        if (const auto* p = parentLayer->props.prop("transform/rotation")) {
            auto val = p->value_at(parentTime);
            p_rot = val.as_f64();
        } else {
            p_rot = (double)parentLayer->xform.rotation;
        }

        Raster::Mat3 p_m = Raster::Mat3::layer_2d(p_anchor, p_pos, p_sc, p_rot);
        l2c = p_m * l2c;
        currentParent = parentLayer->parent.v;
        ++guard;
    }

    if (scale != 1.0) {
        return Raster::Mat3::scale(Raster::vec2(scale, scale)) * l2c;
    }
    return l2c;
}

// after EffectCraft crates/render/src/layers.rs opacity
float Renderer::layer_opacity_at(const Project::Layer& layer, Project::Tick t) {
    if (const auto* p = layer.props.prop("transform/opacity")) {
        auto val = p->value_at(layer.layerTime(t));
        return (float)std::clamp(val.as_f64() / 100.0, 0.0, 1.0);
    }
    return std::clamp(layer.xform.opacity / 100.0f, 0.0f, 1.0f);
}

// =====================================================================
// Layer Source Evaluation
// =====================================================================

// after EffectCraft crates/render/src/pipeline.rs source
Raster::Image Renderer::layer_source(const Project::Project& project, const Project::Composition& comp, const Project::Layer& layer, Project::Tick t, double scale) {
    uint32_t lw = (layer.width > 0) ? layer.width : comp.width;
    uint32_t lh = (layer.height > 0) ? layer.height : comp.height;

    switch (layer.kind) {
        case Project::LayerKind::Solid: {
            Raster::Px px = {
                layer.solidR * layer.solidA,
                layer.solidG * layer.solidA,
                layer.solidB * layer.solidA,
                layer.solidA
            };
            return Raster::Image::filled(lw, lh, px);
        }
        case Project::LayerKind::Composition: {
            if (layer.sourceItem.v != 0) {
                // Nested composition
                RenderOpts sub_opts;
                sub_opts.scale = 1.0;
                return render_frame_opts(project, layer.sourceItem.v, layer.layerTime(t), sub_opts);
            }
            return Raster::Image(lw, lh);
        }
        case Project::LayerKind::Null:
        case Project::LayerKind::Camera:
        case Project::LayerKind::Light:
            return Raster::Image(0, 0);
        default:
            return Raster::Image::filled(lw, lh, {0.0f, 0.0f, 0.0f, 0.0f});
    }
}

// =====================================================================
// Masks (after EffectCraft crates/render/src/masks.rs)
// =====================================================================

static void flatten_bezier(const std::array<double, 2>& p0, const std::array<double, 2>& p1,
                           const std::array<double, 2>& p2, const std::array<double, 2>& p3,
                           std::vector<Raster::Vec2>& out, int depth = 0) {
    if (depth > 8) {
        out.push_back(Raster::vec2(p3[0], p3[1]));
        return;
    }
    double dx = p3[0] - p0[0];
    double dy = p3[1] - p0[1];
    double d1 = std::abs((p1[0] - p3[0]) * dy - (p1[1] - p3[1]) * dx);
    double d2 = std::abs((p2[0] - p3[0]) * dy - (p2[1] - p3[1]) * dx);
    if ((d1 + d2) * (d1 + d2) < 0.25 * (dx * dx + dy * dy)) {
        out.push_back(Raster::vec2(p3[0], p3[1]));
        return;
    }
    std::array<double, 2> p01 = {(p0[0] + p1[0]) * 0.5, (p0[1] + p1[1]) * 0.5};
    std::array<double, 2> p12 = {(p1[0] + p2[0]) * 0.5, (p1[1] + p2[1]) * 0.5};
    std::array<double, 2> p23 = {(p2[0] + p3[0]) * 0.5, (p2[1] + p3[1]) * 0.5};
    std::array<double, 2> p012 = {(p01[0] + p12[0]) * 0.5, (p01[1] + p12[1]) * 0.5};
    std::array<double, 2> p123 = {(p12[0] + p23[0]) * 0.5, (p12[1] + p23[1]) * 0.5};
    std::array<double, 2> p0123 = {(p012[0] + p123[0]) * 0.5, (p012[1] + p123[1]) * 0.5};

    flatten_bezier(p0, p01, p012, p0123, out, depth + 1);
    flatten_bezier(p0123, p123, p23, p3, out, depth + 1);
}

static std::vector<Raster::Vec2> polygon_from_shape_path(const KeyframeModel::ShapePath& sp) {
    std::vector<Raster::Vec2> poly;
    if (sp.vertices.empty()) return poly;

    size_t count = sp.segment_count();
    for (size_t i = 0; i < count; ++i) {
        size_t next_i = (i + 1) % sp.vertices.size();
        const auto& v0 = sp.vertices[i];
        const auto& v1 = sp.vertices[next_i];
        std::array<double, 2> out_tan = {0.0, 0.0};
        std::array<double, 2> in_tan = {0.0, 0.0};
        if (i < sp.out_tangents.size()) out_tan = sp.out_tangents[i];
        if (next_i < sp.in_tangents.size()) in_tan = sp.in_tangents[next_i];

        bool has_curve = (std::abs(out_tan[0]) > 1e-4 || std::abs(out_tan[1]) > 1e-4 ||
                          std::abs(in_tan[0]) > 1e-4 || std::abs(in_tan[1]) > 1e-4);

        if (poly.empty()) poly.push_back(Raster::vec2(v0[0], v0[1]));

        if (has_curve) {
            std::array<double, 2> p1 = {v0[0] + out_tan[0], v0[1] + out_tan[1]};
            std::array<double, 2> p2 = {v1[0] + in_tan[0], v1[1] + in_tan[1]};
            flatten_bezier(v0, p1, p2, v1, poly);
        } else {
            poly.push_back(Raster::vec2(v1[0], v1[1]));
        }
    }
    return poly;
}

static inline bool point_in_polygon(double px, double py, const std::vector<Raster::Vec2>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly[i].x, yi = poly[i].y;
        double xj = poly[j].x, yj = poly[j].y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi + 1e-12) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// after EffectCraft crates/render/src/masks.rs apply
void Renderer::apply_masks(const Project::Layer& layer, Raster::Image& img, Project::Tick t, double /*scale*/) {
    const Project::Tick layerTime = layer.layerTime(t);
    const auto* masks_group = layer.props.sub("masks");
    if (!masks_group) masks_group = layer.props.group("masks");
    if (!masks_group) return;

    std::vector<const Props::PropGroup*> list;
    for (const auto& child : masks_group->children) {
        if (const auto* g = child.as_group()) {
            if (g->enabled && g->kind.tag == Props::GroupKindTag::Mask) {
                if (g->kind.mask_mode != Props::MaskMode::None) {
                    list.push_back(g);
                }
            }
        }
    }
    if (list.empty()) return;

    size_t n = (size_t)img.width * img.height;
    bool first_sub = (list[0]->kind.mask_mode == Props::MaskMode::Subtract);
    std::vector<float> acc(n, first_sub ? 1.0f : 0.0f);

    bool first = true;
    for (const auto* g : list) {
        Props::MaskMode mode = g->kind.mask_mode;
        bool inverted = g->kind.mask_inverted;
        const auto* path_prop = g->prop("path");
        if (!path_prop) path_prop = g->get("path");
        if (!path_prop) continue;
        auto val = path_prop->value_at(layerTime);
        const auto* sp = val.as_path();
        if (!sp || !sp->closed || sp->vertices.empty()) continue;

        if (first && (mode == Props::MaskMode::Intersect || mode == Props::MaskMode::Darken)) {
            mode = Props::MaskMode::Add;
        }
        first = false;

        float op = 1.0f;
        if (const auto* op_prop = g->prop("opacity")) {
            op = (float)(op_prop->value_at(layerTime).as_f64() / 100.0);
        } else if (const auto* op_prop2 = g->get("opacity")) {
            op = (float)(op_prop2->value_at(layerTime).as_f64() / 100.0);
        }

        std::vector<Raster::Vec2> poly = polygon_from_shape_path(*sp);
        std::vector<float> cov(n, 0.0f);
        for (uint32_t y = 0; y < img.height; ++y) {
            double py = (double)y + 0.5;
            for (uint32_t x = 0; x < img.width; ++x) {
                double px = (double)x + 0.5;
                if (point_in_polygon(px, py, poly)) {
                    cov[(size_t)y * img.width + x] = 1.0f;
                }
            }
        }

        for (size_t i = 0; i < n; ++i) {
            float c = cov[i];
            if (inverted) c = 1.0f - c;
            c *= op;
            float a = acc[i];
            switch (mode) {
                case Props::MaskMode::Add:        acc[i] = a + c - a * c; break;
                case Props::MaskMode::Subtract:   acc[i] = a * (1.0f - c); break;
                case Props::MaskMode::Intersect:  acc[i] = a * c; break;
                case Props::MaskMode::Lighten:    acc[i] = std::max(a, c); break;
                case Props::MaskMode::Darken:     acc[i] = std::min(a, c); break;
                case Props::MaskMode::Difference: acc[i] = std::abs(a - c); break;
                default: break;
            }
        }
    }

    // Multiply layer image by accumulated mask coverage
    for (size_t i = 0; i < n; ++i) {
        float m = acc[i];
        for (int c = 0; c < 4; ++c) img.data[i][c] *= m;
    }
}

// =====================================================================
// Composition Frame Renderer
// =====================================================================

// after EffectCraft crates/render/src/pipeline.rs comp_frame
Raster::Image Renderer::render_frame(const Project::Project& project, uint64_t comp_id, Project::Tick t, double scale) {
    RenderOpts opts;
    opts.scale = scale;
    return render_frame_opts(project, comp_id, t, opts);
}

// after EffectCraft crates/render/src/pipeline.rs comp_frame_cpu
Raster::Image Renderer::render_frame_opts(const Project::Project& project, uint64_t comp_id, Project::Tick t, const RenderOpts& opts) {
    const Project::Composition* comp = nullptr;
    for (const auto& c : project.compositions) {
        if (c.id.v == comp_id) {
            comp = &c;
            break;
        }
    }
    if (!comp) {
        if (!project.compositions.empty()) comp = &project.compositions[0];
        else return Raster::Image(1, 1);
    }

    double s = opts.scale;
    uint32_t w = std::max(1u, (uint32_t)std::ceil((double)comp->width * s));
    uint32_t h = std::max(1u, (uint32_t)std::ceil((double)comp->height * s));

    if (opts.depth > 16) return Raster::Image(w, h);

    Raster::Image canvas(w, h);

    // Check if any layer has solo active
    bool any_solo = false;
    for (const auto& l : comp->layers) {
        if (l.switches.solo) {
            any_solo = true;
            break;
        }
    }

    // Draw layers bottom to top (EffectCraft crates/render/src/accel.rs visible_layers: comp.layers.iter().rev())
    for (int i = (int)comp->layers.size() - 1; i >= 0; --i) {
        const auto& layer = comp->layers[(size_t)i];
        if (!layer.isActiveAt(t)) continue;
        if (!layer.switches.video) continue;
        if (any_solo && !layer.switches.solo) continue;
        if (!opts.guides && layer.switches.guide) continue;
        if (!layer.isAV()) continue;

        float opacity = layer_opacity_at(layer, t);
        if (opacity <= 0.0f && !layer.switches.adjustment) continue;

        Raster::Image src = layer_source(project, *comp, layer, t, s);
        if (src.is_empty() && !layer.switches.adjustment) continue;

        apply_masks(layer, src, t, s);

        Raster::Mat3 l2c = layer_matrix_at(*comp, layer, t, s);

        // Track Matte
        // after EffectCraft crates/render/src/layers.rs apply_track_matte
        bool has_matte = (layer.hasMatte && layer.matteLayer.v != 0 && layer.matteLayer.v != layer.id.v);
        const Project::Layer* matte_l = nullptr;
        if (has_matte) {
            for (const auto& l : comp->layers) {
                if (l.id.v == layer.matteLayer.v) {
                    matte_l = &l;
                    break;
                }
            }
            if (!matte_l) has_matte = false;
        }

        if (has_matte || layer.preserveTransparency) {
            Raster::Image iso(canvas.width, canvas.height);
            Raster::WarpOpts place_opts;
            place_opts.sampling = (layer.switches.sampling == Project::Sampling::Bicubic)
                                  ? Raster::Sampling::Bicubic : Raster::Sampling::Bilinear;
            place_opts.mode = Raster::BlendMode::Normal;
            place_opts.opacity = 1.0f;
            Raster::composite_warp(iso, src, l2c, place_opts);

            if (has_matte && matte_l) {
                Raster::Image mimg(canvas.width, canvas.height);
                if (matte_l->isActiveAt(t)) {
                    Raster::Image msrc = layer_source(project, *comp, *matte_l, t, s);
                    apply_masks(*matte_l, msrc, t, s);
                    Raster::Mat3 ml2c = layer_matrix_at(*comp, *matte_l, t, s);
                    Raster::WarpOpts mopts;
                    mopts.sampling = Raster::Sampling::Bilinear;
                    mopts.mode = Raster::BlendMode::Normal;
                    mopts.opacity = layer_opacity_at(*matte_l, t);
                    Raster::composite_warp(mimg, msrc, ml2c, mopts);
                }
                for (size_t p = 0; p < iso.data.size(); ++p) {
                    float k = 1.0f;
                    switch (layer.matteKind) {
                        case Project::MatteKind::Alpha:
                            k = mimg.data[p][3];
                            break;
                        case Project::MatteKind::AlphaInverse:
                            k = 1.0f - mimg.data[p][3];
                            break;
                        case Project::MatteKind::Luma:
                            k = Raster::luminance(mimg.data[p][0], mimg.data[p][1], mimg.data[p][2]);
                            break;
                        case Project::MatteKind::LumaInverse:
                            k = 1.0f - Raster::luminance(mimg.data[p][0], mimg.data[p][1], mimg.data[p][2]);
                            break;
                    }
                    k = std::clamp(k, 0.0f, 1.0f);
                    for (int c = 0; c < 4; ++c) iso.data[p][c] *= k;
                }
            }

            if (layer.preserveTransparency) {
                for (size_t p = 0; p < iso.data.size(); ++p) {
                    float da = canvas.data[p][3];
                    for (int c = 0; c < 4; ++c) iso.data[p][c] *= da;
                }
            }

            canvas.blend_from(iso, layer.blendMode, opacity, (uint32_t)layer.id.v);
        } else {
            Raster::WarpOpts wopts;
            wopts.sampling = (layer.switches.sampling == Project::Sampling::Bicubic)
                             ? Raster::Sampling::Bicubic : Raster::Sampling::Bilinear;
            wopts.opacity = opacity;
            wopts.mode = layer.blendMode;
            wopts.seed = (uint32_t)layer.id.v;
            Raster::composite_warp(canvas, src, l2c, wopts);
        }
    }

    return canvas;
}

} // namespace Render
