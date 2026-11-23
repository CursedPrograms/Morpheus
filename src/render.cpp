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
#include <unordered_map>

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

    if (layer.kind == Project::LayerKind::Shape) {
        const double sourceWidth = static_cast<double>(layer.width > 0 ? layer.width : comp.width);
        const double sourceHeight = static_cast<double>(layer.height > 0 ? layer.height : comp.height);
        l2c = l2c * Raster::Mat3::translate(
            Raster::vec2(-sourceWidth * 0.5, -sourceHeight * 0.5));
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

namespace {

struct ShapeContour {
    std::vector<Raster::Vec2> points;
    std::vector<size_t> vertices;
    std::vector<size_t> corners;
    bool closed = false;
};

struct ShapeSlot {
    std::vector<ShapeContour> paths;
};

struct ShapeDraw {
    std::vector<size_t> slots;
    Raster::Px color{0.0f, 0.0f, 0.0f, 1.0f};
    float opacity = 1.0f;
    bool stroke = false;
    bool evenOdd = false;
    bool local = true;
    double width = 1.0;
    uint32_t cap = 0;
    uint32_t join = 0;
    double miter = 4.0;
    std::vector<double> dash;
    double dashOffset = 0.0;
};

static double shapeScalar(const Props::PropGroup& group, const char* name,
                          Project::Tick time, double fallback) {
    const Props::Property* property = group.prop(name);
    if (!property) return fallback;
    const double value = property->value_at(time).as_f64();
    return std::isfinite(value) ? value : fallback;
}

static std::array<double, 2> shapeVec2(const Props::PropGroup& group, const char* name,
                                      Project::Tick time, std::array<double, 2> fallback) {
    const Props::Property* property = group.prop(name);
    if (!property) return fallback;
    auto value = property->value_at(time).as_vec2();
    for (size_t i = 0; i < value.size(); ++i)
        if (!std::isfinite(value[i])) value[i] = fallback[i];
    return value;
}

static Raster::Mat3 shapeTransform(const Props::PropGroup* transform, Project::Tick time) {
    if (!transform) return Raster::Mat3::IDENTITY;
    const auto anchor = shapeVec2(*transform, "anchor", time, {0.0, 0.0});
    const auto position = shapeVec2(*transform, "position", time, {0.0, 0.0});
    const auto scale = shapeVec2(*transform, "scale", time, {100.0, 100.0});
    const double rotation = shapeScalar(*transform, "rotation", time, 0.0);
    const double skew = shapeScalar(*transform, "skew", time, 0.0);
    const double skewAxis = shapeScalar(*transform, "skewAxis", time, 0.0);
    return Raster::Mat3::translate(Raster::vec2(position[0], position[1])) *
        Raster::Mat3::rotate_deg(rotation) *
        Raster::Mat3::skew_deg(-skew, skewAxis) *
        Raster::Mat3::scale(Raster::vec2(scale[0] / 100.0, scale[1] / 100.0)) *
        Raster::Mat3::translate(Raster::vec2(-anchor[0], -anchor[1]));
}

static void flattenShapeCubic(const Raster::Vec2& p0, const Raster::Vec2& p1,
                              const Raster::Vec2& p2, const Raster::Vec2& p3,
                              std::vector<Raster::Vec2>& out, int depth = 0) {
    if (depth >= 12) {
        out.push_back(p3);
        return;
    }
    const double dx = p3.x - p0.x;
    const double dy = p3.y - p0.y;
    const double d1 = std::abs((p1.x - p3.x) * dy - (p1.y - p3.y) * dx);
    const double d2 = std::abs((p2.x - p3.x) * dy - (p2.y - p3.y) * dx);
    if ((d1 + d2) * (d1 + d2) < 0.25 * (dx * dx + dy * dy)) {
        out.push_back(p3);
        return;
    }
    const Raster::Vec2 p01 = (p0 + p1) * 0.5;
    const Raster::Vec2 p12 = (p1 + p2) * 0.5;
    const Raster::Vec2 p23 = (p2 + p3) * 0.5;
    const Raster::Vec2 p012 = (p01 + p12) * 0.5;
    const Raster::Vec2 p123 = (p12 + p23) * 0.5;
    const Raster::Vec2 p0123 = (p012 + p123) * 0.5;
    flattenShapeCubic(p0, p01, p012, p0123, out, depth + 1);
    flattenShapeCubic(p0123, p123, p23, p3, out, depth + 1);
}

static ShapeContour flattenShapePath(const KeyframeModel::ShapePath& path,
                                     const Raster::Mat3& transform) {
    ShapeContour contour;
    contour.closed = path.closed;
    const size_t count = path.vertices.size();
    if (count == 0) return contour;
    const auto point = [&](size_t i) {
        return transform.apply(Raster::vec2(path.vertices[i][0], path.vertices[i][1]));
    };
    const auto tangent = [&](const std::vector<std::array<double, 2>>& tangents, size_t i) {
        const auto value = i < tangents.size() ? tangents[i] : std::array<double, 2>{0.0, 0.0};
        return transform.apply_vec(Raster::vec2(value[0], value[1]));
    };
    contour.points.push_back(point(0));
    std::vector<size_t> pointAtVertex(count, 0);
    const size_t segmentCount = path.closed ? count : count - 1;
    for (size_t i = 0; i < segmentCount; ++i) {
        const size_t next = (i + 1) % count;
        const Raster::Vec2 start = point(i);
        const Raster::Vec2 end = point(next);
        const Raster::Vec2 out = tangent(path.out_tangents, i);
        const Raster::Vec2 in = tangent(path.in_tangents, next);
        if (out.x != 0.0 || out.y != 0.0 || in.x != 0.0 || in.y != 0.0)
            flattenShapeCubic(start, start + out, end + in, end, contour.points);
        else
            contour.points.push_back(end);
        if (next != 0) pointAtVertex[next] = contour.points.size() - 1;
    }

    std::vector<size_t> canonical;
    for (size_t i = 0; i < count; ++i) {
        if (canonical.empty() ||
            (point(i) - point(canonical.back())).length() > 1e-9) {
            canonical.push_back(i);
        } else {
            canonical.back() = i;
        }
    }
    if (path.closed && canonical.size() > 1 &&
        (point(canonical.front()) - point(canonical.back())).length() <= 1e-9)
        canonical.pop_back();
    for (size_t i : canonical)
        contour.vertices.push_back(i == 0 ? 0 : pointAtVertex[i]);

    for (size_t i = 0; i < canonical.size(); ++i) {
        if (!path.closed && (i == 0 || i + 1 == canonical.size())) continue;
        const size_t vertex = canonical[i];
        const size_t previous = canonical[(i + canonical.size() - 1) % canonical.size()];
        const size_t next = canonical[(i + 1) % canonical.size()];
        const Raster::Vec2 currentPoint = point(vertex);
        Raster::Vec2 incoming = vertex < path.in_tangents.size()
            ? tangent(path.in_tangents, vertex) * -1.0
            : Raster::Vec2::ZERO;
        Raster::Vec2 outgoing = vertex < path.out_tangents.size()
            ? tangent(path.out_tangents, vertex)
            : Raster::Vec2::ZERO;
        if (incoming.length() <= 1e-9) incoming = currentPoint - point(previous);
        if (outgoing.length() <= 1e-9) outgoing = point(next) - currentPoint;
        const double turn = std::abs(std::atan2(
            incoming.x * outgoing.y - incoming.y * outgoing.x,
            incoming.x * outgoing.x + incoming.y * outgoing.y));
        if (turn > 1e-3 && turn < 3.14159265358979323846 - 1e-3)
            contour.corners.push_back(vertex == 0 ? 0 : pointAtVertex[vertex]);
    }
    if (std::any_of(contour.points.begin(), contour.points.end(), [](const Raster::Vec2& point) {
            return !std::isfinite(point.x) || !std::isfinite(point.y);
        })) {
        contour.points.clear();
        contour.vertices.clear();
        contour.corners.clear();
    }
    return contour;
}

static KeyframeModel::ShapePath shapeRectangle(
    std::array<double, 2> size, std::array<double, 2> position, double roundness,
    bool perCorner, const Props::PropGroup& group, Project::Tick time) {
    const double width = std::isfinite(size[0]) ? std::abs(size[0]) : 0.0;
    const double height = std::isfinite(size[1]) ? std::abs(size[1]) : 0.0;
    if (!std::isfinite(position[0]) || !std::isfinite(position[1])) return {};
    const double x0 = position[0] - width * 0.5;
    const double y0 = position[1] - height * 0.5;
    const double x1 = position[0] + width * 0.5;
    const double y1 = position[1] + height * 0.5;
    std::array<double, 4> radii{};
    if (perCorner) {
        radii = {
            shapeScalar(group, "topLeftRoundness", time, 0.0),
            shapeScalar(group, "topRightRoundness", time, 0.0),
            shapeScalar(group, "bottomRightRoundness", time, 0.0),
            shapeScalar(group, "bottomLeftRoundness", time, 0.0)
        };
        for (double& radius : radii)
            radius = std::isfinite(radius) ? std::max(0.0, radius) : 0.0;
        const auto ratio = [](double a, double b, double edge) {
            return a + b > edge && a + b > 0.0 ? std::min(edge / (a + b), 1.0) : 1.0;
        };
        const double factor = std::min({
            ratio(radii[0], radii[1], width), ratio(radii[3], radii[2], width),
            ratio(radii[0], radii[3], height), ratio(radii[1], radii[2], height)
        });
        for (double& radius : radii) radius *= factor;
    } else {
        const double radius = std::isfinite(roundness)
            ? std::clamp(roundness, 0.0, std::min(width, height) * 0.5) : 0.0;
        radii.fill(radius);
    }

    constexpr double k = 0.5522847498307936;
    const double tl = radii[0], tr = radii[1], br = radii[2], bl = radii[3];
    KeyframeModel::ShapePath path;
    path.vertices = {
        {x1, y0 + tr}, {x1, y1 - br}, {x1 - br, y1}, {x0 + bl, y1},
        {x0, y1 - bl}, {x0, y0 + tl}, {x0 + tl, y0}, {x1 - tr, y0}
    };
    path.in_tangents.assign(8, {0.0, 0.0});
    path.out_tangents.assign(8, {0.0, 0.0});
    path.out_tangents[1] = {0.0, k * br};
    path.in_tangents[2] = {k * br, 0.0};
    path.out_tangents[3] = {-k * bl, 0.0};
    path.in_tangents[4] = {0.0, k * bl};
    path.out_tangents[5] = {0.0, -k * tl};
    path.in_tangents[6] = {-k * tl, 0.0};
    path.out_tangents[7] = {k * tr, 0.0};
    path.in_tangents[0] = {0.0, -k * tr};
    path.closed = true;
    return path;
}

static KeyframeModel::ShapePath reverseShapePath(KeyframeModel::ShapePath path) {
    std::reverse(path.vertices.begin(), path.vertices.end());
    std::reverse(path.in_tangents.begin(), path.in_tangents.end());
    std::reverse(path.out_tangents.begin(), path.out_tangents.end());
    std::swap(path.in_tangents, path.out_tangents);
    return path;
}

static float shapeFloat(const Props::PropGroup& group, const char* name,
                        Project::Tick time, float fallback) {
    return static_cast<float>(shapeScalar(group, name, time, fallback));
}

static double shapeContourLength(const ShapeContour& path) {
    double length = 0.0;
    for (size_t i = 1; i < path.points.size(); ++i)
        length += (path.points[i] - path.points[i - 1]).length();
    return length;
}

static double shapeSlotLength(const ShapeSlot& slot) {
    double length = 0.0;
    for (const ShapeContour& path : slot.paths) length += shapeContourLength(path);
    return length;
}

static std::optional<std::vector<std::pair<double, double>>> shapeTrimIntervals(
    double start, double end, double offsetDegrees) {
    double s = start / 100.0;
    double e = end / 100.0;
    if (s > e) std::swap(s, e);
    s = std::clamp(s, 0.0, 1.0);
    e = std::clamp(e, 0.0, 1.0);
    if (e - s >= 1.0 - 1e-9) return std::nullopt;
    if (e - s <= 1e-9) return std::vector<std::pair<double, double>>{};
    double offset = std::fmod(offsetDegrees / 360.0, 1.0);
    if (offset < 0.0) offset += 1.0;
    s += offset;
    e += offset;
    if (e <= 1.0) return std::vector<std::pair<double, double>>{{s, e}};
    if (s >= 1.0) return std::vector<std::pair<double, double>>{{s - 1.0, e - 1.0}};
    return std::vector<std::pair<double, double>>{{s, 1.0}, {0.0, e - 1.0}};
}

static std::vector<ShapeContour> trimShapePaths(
    const std::vector<ShapeContour>& paths,
    const std::vector<std::pair<double, double>>& ranges) {
    std::vector<ShapeContour> result;
    const auto contains = [](const std::vector<size_t>& indices, size_t index) {
        return std::find(indices.begin(), indices.end(), index) != indices.end();
    };
    const auto markVertex = [](ShapeContour& contour, bool vertex, bool corner) {
        if (contour.points.empty()) return;
        const size_t index = contour.points.size() - 1;
        if (vertex && (contour.vertices.empty() || contour.vertices.back() != index))
            contour.vertices.push_back(index);
        if (corner && (contour.corners.empty() || contour.corners.back() != index))
            contour.corners.push_back(index);
    };
    for (const auto& range : ranges) {
        double pathBase = 0.0;
        for (const ShapeContour& path : paths) {
            double segmentBase = pathBase;
            for (size_t i = 1; i < path.points.size(); ++i) {
                const Raster::Vec2 a = path.points[i - 1];
                const Raster::Vec2 b = path.points[i];
                const double segmentLength = (b - a).length();
                if (segmentLength <= 1e-12) continue;
                const double segmentEnd = segmentBase + segmentLength;
                const double from = std::max(segmentBase, range.first);
                const double to = std::min(segmentEnd, range.second);
                if (to > from) {
                    const Raster::Vec2 p0 = a + (b - a) * ((from - segmentBase) / segmentLength);
                    const Raster::Vec2 p1 = a + (b - a) * ((to - segmentBase) / segmentLength);
                    const bool startsAtSourceVertex =
                        std::abs(from - segmentBase) <= 1e-8 &&
                        contains(path.vertices, i - 1);
                    const bool endsAtSourceVertex = std::abs(to - segmentEnd) <= 1e-8 &&
                        (contains(path.vertices, i) ||
                         (path.closed && i + 1 == path.points.size() &&
                          contains(path.vertices, 0)));
                    const bool startsAtVertex = startsAtSourceVertex ||
                        std::abs(from - range.first) <= 1e-8;
                    const bool endsAtVertex = endsAtSourceVertex ||
                        std::abs(to - range.second) <= 1e-8;
                    const bool startsAtCorner = startsAtSourceVertex &&
                        contains(path.corners, i - 1);
                    const bool endsAtCorner = endsAtSourceVertex &&
                        (contains(path.corners, i) ||
                         (path.closed && i + 1 == path.points.size() &&
                          contains(path.corners, 0)));
                    if (result.empty() || result.back().points.empty() ||
                        (result.back().points.back() - p0).length() > 1e-6)
                        result.push_back(ShapeContour{{p0}, {}, {}, false});
                    markVertex(result.back(), startsAtVertex, startsAtCorner);
                    if ((result.back().points.back() - p1).length() > 1e-9)
                        result.back().points.push_back(p1);
                    markVertex(result.back(), endsAtVertex, endsAtCorner);
                }
                segmentBase = segmentEnd;
            }
            pathBase += shapeContourLength(path);
        }
    }
    return result;
}

static double shapePolylineLength(const ShapeContour& path, size_t start, size_t end) {
    double length = 0.0;
    for (size_t i = start + 1; i <= end; ++i)
        length += (path.points[i] - path.points[i - 1]).length();
    return length;
}

static Raster::Vec2 shapePolylinePointAt(
    const ShapeContour& path, size_t start, size_t end, double distance) {
    double traversed = 0.0;
    for (size_t i = start + 1; i <= end; ++i) {
        const Raster::Vec2 a = path.points[i - 1];
        const Raster::Vec2 b = path.points[i];
        const double segment = (b - a).length();
        if (segment > 1e-12 && traversed + segment >= distance)
            return a + (b - a) * std::clamp((distance - traversed) / segment, 0.0, 1.0);
        traversed += segment;
    }
    return path.points[end];
}

static std::vector<Raster::Vec2> shapePolylineSlice(
    const ShapeContour& path, size_t start, size_t end, double from, double to) {
    std::vector<Raster::Vec2> points{
        shapePolylinePointAt(path, start, end, from)
    };
    double traversed = 0.0;
    for (size_t i = start + 1; i <= end; ++i) {
        traversed += (path.points[i] - path.points[i - 1]).length();
        if (traversed > from + 1e-9 && traversed < to - 1e-9)
            points.push_back(path.points[i]);
    }
    const Raster::Vec2 last = shapePolylinePointAt(path, start, end, to);
    if ((points.back() - last).length() > 1e-9) points.push_back(last);
    return points;
}

static void appendShapePoints(std::vector<Raster::Vec2>& target,
                              const std::vector<Raster::Vec2>& points) {
    for (const Raster::Vec2& point : points)
        if (target.empty() || (target.back() - point).length() > 1e-9)
            target.push_back(point);
}

// after EffectCraft crates/path/src/ops.rs round_corners
static ShapeContour roundShapeContour(const ShapeContour& path, double radius) {
    const size_t vertexCount = path.vertices.size();
    const size_t segmentCount = path.closed ? vertexCount :
        (vertexCount == 0 ? 0 : vertexCount - 1);
    if (radius <= 0.0 || !std::isfinite(radius) || segmentCount == 0 ||
        path.corners.empty())
        return path;

    std::vector<double> cuts(vertexCount, 0.0);
    std::vector<Raster::Vec2> incoming(vertexCount, Raster::Vec2::ZERO);
    std::vector<Raster::Vec2> outgoing(vertexCount, Raster::Vec2::ZERO);
    std::vector<double> turns(vertexCount, 0.0);
    const auto segmentStart = [&](size_t i) { return path.vertices[i]; };
    const auto segmentEnd = [&](size_t i) {
        return i + 1 < vertexCount ? path.vertices[i + 1] : path.points.size() - 1;
    };
    const auto endTangent = [&](size_t i) {
        const size_t end = segmentEnd(i);
        for (size_t p = end; p > segmentStart(i); --p) {
            const Raster::Vec2 tangent = path.points[p] - path.points[p - 1];
            if (tangent.length() > 1e-9) return tangent * (1.0 / tangent.length());
        }
        return Raster::Vec2::ZERO;
    };
    const auto startTangent = [&](size_t i) {
        const size_t start = segmentStart(i);
        const size_t end = segmentEnd(i);
        for (size_t p = start + 1; p <= end; ++p) {
            const Raster::Vec2 tangent = path.points[p] - path.points[p - 1];
            if (tangent.length() > 1e-9) return tangent * (1.0 / tangent.length());
        }
        return Raster::Vec2::ZERO;
    };

    for (size_t i = 0; i < vertexCount; ++i) {
        if (!path.closed && (i == 0 || i + 1 == vertexCount)) continue;
        if (std::find(path.corners.begin(), path.corners.end(), path.vertices[i]) ==
            path.corners.end())
            continue;
        const size_t previous = (i + vertexCount - 1) % vertexCount;
        const size_t next = i;
        incoming[i] = endTangent(previous);
        outgoing[i] = startTangent(next);
        const double dot = incoming[i].x * outgoing[i].x +
            incoming[i].y * outgoing[i].y;
        const double cross = incoming[i].x * outgoing[i].y -
            incoming[i].y * outgoing[i].x;
        turns[i] = std::abs(std::atan2(cross, dot));
        if (turns[i] <= 1e-3 || turns[i] >= 3.14159265358979323846 - 1e-3)
            continue;
        const double previousLength = shapePolylineLength(
            path, segmentStart(previous), segmentEnd(previous));
        const double nextLength = shapePolylineLength(
            path, segmentStart(next), segmentEnd(next));
        cuts[i] = std::min({radius, previousLength * 0.5, nextLength * 0.5});
    }

    ShapeContour rounded;
    rounded.closed = path.closed;
    for (size_t i = 0; i < segmentCount; ++i) {
        const size_t next = (i + 1) % vertexCount;
        const size_t start = segmentStart(i);
        const size_t end = segmentEnd(i);
        const double length = shapePolylineLength(path, start, end);
        const double from = cuts[i];
        const double to = std::max(from, length - cuts[next]);
        appendShapePoints(rounded.points, shapePolylineSlice(path, start, end, from, to));
        if (cuts[next] <= 0.0) continue;

        const Raster::Vec2 corner = path.points[path.vertices[next]];
        const Raster::Vec2 p = corner - incoming[next] * cuts[next];
        const Raster::Vec2 q = corner + outgoing[next] * cuts[next];
        const double angle = turns[next];
        const double handle = (4.0 / 3.0) * std::tan(angle * 0.25) /
            std::tan(angle * 0.5);
        const Raster::Vec2 c1 = p + incoming[next] * (cuts[next] * handle);
        const Raster::Vec2 c2 = q - outgoing[next] * (cuts[next] * handle);
        std::vector<Raster::Vec2> arc{p};
        flattenShapeCubic(p, c1, c2, q, arc);
        appendShapePoints(rounded.points, arc);
    }
    return rounded.points.size() > 1 ? rounded : path;
}

// after EffectCraft crates/path/src/ops.rs trim and trim_individually
static void trimShapeSlots(std::vector<ShapeSlot>& arena, const std::vector<size_t>& live,
                          double start, double end, double offset, bool individually) {
    const auto intervals = shapeTrimIntervals(start, end, offset);
    if (!intervals) return;
    if (intervals->empty()) {
        for (size_t slot : live)
            if (slot < arena.size()) arena[slot].paths.clear();
        return;
    }
    if (!individually) {
        for (size_t slot : live) {
            if (slot >= arena.size()) continue;
            const double total = shapeSlotLength(arena[slot]);
            std::vector<std::pair<double, double>> ranges;
            ranges.reserve(intervals->size());
            for (const auto& interval : *intervals)
                ranges.emplace_back(interval.first * total, interval.second * total);
            arena[slot].paths = trimShapePaths(arena[slot].paths, ranges);
        }
        return;
    }

    double total = 0.0;
    for (size_t slot : live)
        if (slot < arena.size()) total += shapeSlotLength(arena[slot]);
    double base = 0.0;
    for (size_t slot : live) {
        if (slot >= arena.size()) continue;
        const double slotLength = shapeSlotLength(arena[slot]);
        std::vector<std::pair<double, double>> ranges;
        for (const auto& interval : *intervals) {
            const double from = std::max(0.0, interval.first * total - base);
            const double to = std::min(slotLength, interval.second * total - base);
            if (to > from) ranges.emplace_back(from, to);
        }
        arena[slot].paths = trimShapePaths(arena[slot].paths, ranges);
        base += slotLength;
    }
}

static std::vector<size_t> usedShapeSlots(const std::vector<size_t>& live,
                                         const std::vector<ShapeDraw>& draws) {
    std::vector<size_t> slots = live;
    for (const ShapeDraw& draw : draws)
        slots.insert(slots.end(), draw.slots.begin(), draw.slots.end());
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    return slots;
}

static void transformShapeSlot(ShapeSlot& slot, const Raster::Mat3& transform) {
    for (ShapeContour& path : slot.paths)
        for (Raster::Vec2& point : path.points)
            point = transform.apply(point);
}

// after EffectCraft crates/path/src/ops.rs twist
static void twistShapeSlots(std::vector<ShapeSlot>& arena, const std::vector<size_t>& live,
                            double angleDegrees, std::array<double, 2> center,
                            const Raster::Mat3& transform) {
    const double angle = angleDegrees * (3.14159265358979323846 / 180.0);
    const auto inverse = transform.inverse();
    const Raster::Vec2 localCenter = inverse
        ? Raster::vec2(center[0], center[1])
        : transform.apply(Raster::vec2(center[0], center[1]));
    const auto toLocal = [&](const Raster::Vec2& point) {
        return inverse ? inverse->apply(point) : point;
    };
    const auto toWorld = [&](const Raster::Vec2& point) {
        return inverse ? transform.apply(point) : point;
    };
    double maxRadius = 0.0;
    for (size_t slot : live) {
        if (slot >= arena.size()) continue;
        for (const ShapeContour& path : arena[slot].paths) {
            const auto update = [&](size_t i) {
                if (i < path.points.size())
                    maxRadius = std::max(maxRadius,
                        (toLocal(path.points[i]) - localCenter).length());
            };
            if (path.vertices.empty()) {
                for (size_t i = 0; i < path.points.size(); ++i) update(i);
            } else {
                for (size_t i : path.vertices) update(i);
            }
        }
    }
    if (angle == 0.0 || !std::isfinite(angle) || maxRadius <= 1e-9) return;
    const double radiansPerUnit = angle / maxRadius;
    const auto mapPoint = [&](const Raster::Vec2& point) {
        const Raster::Vec2 vector = point - localCenter;
        const double radius = vector.length();
        const double theta = radiansPerUnit * radius;
        const double cosine = std::cos(theta);
        const double sine = std::sin(theta);
        const Raster::Vec2 mapped = localCenter + Raster::vec2(
            vector.x * cosine - vector.y * sine,
            vector.x * sine + vector.y * cosine);
        return toWorld(mapped);
    };

    for (size_t slot : live) {
        if (slot >= arena.size()) continue;
        for (ShapeContour& path : arena[slot].paths) {
            const std::vector<Raster::Vec2> source = path.points;
            const std::vector<size_t> oldVertices = path.vertices;
            const std::vector<size_t> oldCorners = path.corners;
            std::vector<Raster::Vec2> local;
            local.reserve(source.size());
            for (const Raster::Vec2& point : source) local.push_back(toLocal(point));
            std::vector<size_t> pointMap(source.size(), 0);
            path.points.clear();
            if (source.empty()) continue;
            path.points.push_back(mapPoint(local.front()));
            for (size_t i = 1; i < source.size(); ++i) {
                const double length = (local[i] - local[i - 1]).length();
                const double subdivisionCount = std::ceil(
                    (std::abs(angle) * length / maxRadius) / 0.15 + length / 50.0);
                const size_t subdivisions = static_cast<size_t>(
                    std::clamp(subdivisionCount, 1.0, 64.0));
                for (size_t j = 1; j <= subdivisions; ++j) {
                    const double t = static_cast<double>(j) / subdivisions;
                    path.points.push_back(mapPoint(local[i - 1] +
                        (local[i] - local[i - 1]) * t));
                }
                pointMap[i] = path.points.size() - 1;
            }
            path.vertices.clear();
            path.corners.clear();
            for (size_t i : oldVertices)
                if (i < pointMap.size() &&
                    (path.vertices.empty() || path.vertices.back() != pointMap[i]))
                    path.vertices.push_back(pointMap[i]);
            for (size_t i : oldCorners)
                if (i < pointMap.size() &&
                    (path.corners.empty() || path.corners.back() != pointMap[i]))
                    path.corners.push_back(pointMap[i]);
            if (std::any_of(path.points.begin(), path.points.end(), [](const Raster::Vec2& point) {
                    return !std::isfinite(point.x) || !std::isfinite(point.y);
                })) {
                path.points.clear();
                path.vertices.clear();
                path.corners.clear();
            }
        }
    }
}

static void collectShapeDraws(const Props::PropGroup& contents, Project::Tick time,
                              const Raster::Mat3& transform, float parentOpacity,
                              std::vector<ShapeSlot>& arena, std::vector<size_t>& live,
                              std::vector<ShapeDraw>& draws) {
    const size_t scopeDrawStart = draws.size();
    for (const Props::Node& node : contents.children) {
        const Props::PropGroup* group = node.as_group();
        if (!group || !group->enabled) continue;
        if (group->match_id == "group") {
            std::vector<size_t> nested;
            const size_t nestedDrawStart = draws.size();
            const float opacity = std::clamp(
                shapeFloat(*group, "transform/opacity", time, 100.0f) / 100.0f, 0.0f, 1.0f);
            const Raster::Mat3 matrix = transform * shapeTransform(group->group("transform"), time);
            if (const Props::PropGroup* child = group->group("contents"))
                collectShapeDraws(*child, time, matrix, parentOpacity * opacity,
                                  arena, nested, draws);
            live.insert(live.end(), nested.begin(), nested.end());
            for (size_t i = nestedDrawStart; i < draws.size(); ++i)
                draws[i].local = false;
            continue;
        }

        if (group->match_id == "trim") {
            trimShapeSlots(arena, live,
                shapeScalar(*group, "start", time, 0.0),
                shapeScalar(*group, "end", time, 100.0),
                shapeScalar(*group, "offset", time, 0.0),
                shapeScalar(*group, "mode", time, 0.0) == 1.0);
            continue;
        }

        if (group->match_id == "round") {
            const double radius = shapeScalar(*group, "radius", time, 10.0);
            for (size_t slot : live) {
                if (slot >= arena.size()) continue;
                for (ShapeContour& path : arena[slot].paths)
                    path = roundShapeContour(path, radius);
            }
            continue;
        }

        if (group->match_id == "twist") {
            twistShapeSlots(arena, live,
                shapeScalar(*group, "angle", time, 0.0),
                shapeVec2(*group, "center", time, {0.0, 0.0}), transform);
            continue;
        }

        if (group->match_id == "repeater") {
            // Bound project-provided counts to prevent unbounded slot/draw allocation.
            const double copies = std::clamp(
                shapeScalar(*group, "copies", time, 3.0), 0.0, 4096.0);
            const double offset = shapeScalar(*group, "offset", time, 0.0);
            const size_t count = static_cast<size_t>(std::ceil(copies));
            const Props::PropGroup* tr = group->group("transform");
            if (!tr) continue;
            const auto anchor = shapeVec2(*tr, "anchor", time, {0.0, 0.0});
            const auto position = shapeVec2(*tr, "position", time, {100.0, 0.0});
            const auto scale = shapeVec2(*tr, "scale", time, {100.0, 100.0});
            const double rotation = shapeScalar(*tr, "rotation", time, 0.0);
            const float startOpacity = static_cast<float>(
                shapeScalar(*tr, "startOpacity", time, 100.0) / 100.0);
            const float endOpacity = static_cast<float>(
                shapeScalar(*tr, "endOpacity", time, 100.0) / 100.0);
            const auto inverseTransform = transform.inverse();
            const auto step = [&](double k) {
                const Raster::Mat3 local =
                    Raster::Mat3::translate(Raster::vec2(position[0] * k, position[1] * k)) *
                    Raster::Mat3::translate(Raster::vec2(anchor[0], anchor[1])) *
                    Raster::Mat3::rotate_deg(rotation * k) *
                    Raster::Mat3::scale(Raster::vec2(
                        std::pow(scale[0] / 100.0, k), std::pow(scale[1] / 100.0, k))) *
                    Raster::Mat3::translate(Raster::vec2(-anchor[0], -anchor[1]));
                return inverseTransform ? transform * local * *inverseTransform : local;
            };
            std::vector<ShapeDraw> baseDraws;
            baseDraws.reserve(draws.size() - scopeDrawStart);
            for (size_t i = scopeDrawStart; i < draws.size(); ++i)
                baseDraws.push_back(std::move(draws[i]));
            draws.erase(draws.begin() + static_cast<std::ptrdiff_t>(scopeDrawStart), draws.end());
            const std::vector<size_t> baseLive = std::move(live);
            const std::vector<size_t> baseSlots = usedShapeSlots(baseLive, baseDraws);
            std::vector<std::unordered_map<size_t, size_t>> copyMaps(count);
            for (size_t i = 0; i < count; ++i) {
                const Raster::Mat3 matrix = step(static_cast<double>(i) + offset);
                for (size_t slot : baseSlots) {
                    if (slot >= arena.size()) continue;
                    ShapeSlot copy = arena[slot];
                    transformShapeSlot(copy, matrix);
                    arena.push_back(std::move(copy));
                    copyMaps[i].emplace(slot, arena.size() - 1);
                }
                for (size_t slot : baseLive) {
                    const auto found = copyMaps[i].find(slot);
                    if (found != copyMaps[i].end()) live.push_back(found->second);
                }
            }
            std::vector<size_t> order(count);
            for (size_t i = 0; i < count; ++i) order[i] = i;
            if (shapeScalar(*group, "composite", time, 0.0) != 1.0)
                std::reverse(order.begin(), order.end());
            for (size_t i : order) {
                const Raster::Mat3 matrix = step(static_cast<double>(i) + offset);
                const float t = copies > 0.0
                    ? static_cast<float>(static_cast<double>(i) / copies) : 0.0f;
                const float opacity = startOpacity + (endOpacity - startOpacity) * t;
                const float partial = i + 1 == count && copies - std::floor(copies) > 0.0
                    ? static_cast<float>(copies - std::floor(copies)) : 1.0f;
                for (const ShapeDraw& base : baseDraws) {
                    ShapeDraw copy = base;
                    bool valid = true;
                    for (size_t& slot : copy.slots) {
                        const auto found = copyMaps[i].find(slot);
                        if (found == copyMaps[i].end()) {
                            valid = false;
                            break;
                        }
                        slot = found->second;
                    }
                    if (!valid) continue;
                    copy.opacity *= opacity * partial;
                    draws.push_back(std::move(copy));
                }
            }
            continue;
        }

        if (group->match_id == "rect" || group->match_id == "ellipse" || group->match_id == "path") {
            KeyframeModel::ShapePath path;
            if (group->match_id == "rect") {
                path = shapeRectangle(shapeVec2(*group, "size", time, {100.0, 100.0}),
                    shapeVec2(*group, "position", time, {0.0, 0.0}),
                    shapeScalar(*group, "roundness", time, 0.0),
                    group->prop("perCorner") && group->prop("perCorner")->value_at(time).as_bool(),
                    *group, time);
            } else if (group->match_id == "ellipse") {
                const auto size = shapeVec2(*group, "size", time, {100.0, 100.0});
                path = KeyframeModel::ShapePath::ellipse(
                    shapeVec2(*group, "position", time, {0.0, 0.0}), size[0], size[1]);
            } else if (const Props::Property* property = group->prop("path")) {
                path = property->value_at(time).path;
            }
            if (shapeScalar(*group, "direction", time, 0.0) == 1.0)
                path = reverseShapePath(std::move(path));
            ShapeContour contour = flattenShapePath(path, transform);
            if (contour.points.size() > 1) {
                arena.push_back({{std::move(contour)}});
                live.push_back(arena.size() - 1);
            }
            continue;
        }

        if (group->match_id != "fill" && group->match_id != "stroke") continue;
        ShapeDraw draw;
        draw.slots = live;
        draw.local = true;
        draw.stroke = group->match_id == "stroke";
        draw.opacity = parentOpacity * std::clamp(shapeFloat(*group, "opacity", time, 100.0f) / 100.0f, 0.0f, 1.0f);
        if (const Props::Property* color = group->prop("color")) {
            const auto rgba = color->value_at(time).as_color();
            draw.color = {
                std::isfinite(rgba[0]) ? rgba[0] : 0.0f,
                std::isfinite(rgba[1]) ? rgba[1] : 0.0f,
                std::isfinite(rgba[2]) ? rgba[2] : 0.0f,
                1.0f
            };
        }
        draw.evenOdd = shapeScalar(*group, "rule", time, 0.0) == 1.0;
        if (draw.stroke) {
            draw.width = shapeScalar(*group, "width", time, 2.0);
            draw.cap = static_cast<uint32_t>(std::clamp(shapeScalar(*group, "cap", time, 0.0), 0.0, 2.0));
            draw.join = static_cast<uint32_t>(std::clamp(shapeScalar(*group, "join", time, 0.0), 0.0, 2.0));
            draw.miter = std::max(1.0, shapeScalar(*group, "miter", time, 4.0));
            if (const Props::PropGroup* dashes = group->group("dashes")) {
                const double dash = shapeScalar(*dashes, "dash", time, 0.0);
                const double gap = shapeScalar(*dashes, "gap", time, 0.0);
                if (std::isfinite(dash) && dash > 0.0) {
                    draw.dash = {dash, std::isfinite(gap) && gap > 0.0 ? gap : dash};
                    draw.dashOffset = shapeScalar(*dashes, "offset", time, 0.0);
                }
            }
        }
        draws.push_back(std::move(draw));
    }
}

static bool shapePointInFill(double x, double y, const std::vector<ShapeContour>& paths,
                             bool evenOdd) {
    int winding = 0;
    bool parity = false;
    for (const ShapeContour& path : paths) {
        if (path.points.size() < 3) continue;
        for (size_t i = 0; i < path.points.size(); ++i) {
            const Raster::Vec2& a = path.points[i];
            const Raster::Vec2& b = path.points[(i + 1) % path.points.size()];
            if ((a.y > y) == (b.y > y)) continue;
            const double cross = (b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y);
            if (evenOdd) {
                if ((b.y > a.y && cross > 0.0) || (b.y < a.y && cross < 0.0))
                    parity = !parity;
            } else if (b.y > a.y && cross > 0.0) {
                ++winding;
            } else if (b.y < a.y && cross < 0.0) {
                --winding;
            }
        }
    }
    return evenOdd ? parity : winding != 0;
}

static bool shapePointInTriangle(const Raster::Vec2& p, const Raster::Vec2& a,
                                 const Raster::Vec2& b, const Raster::Vec2& c) {
    const double c1 = (b - a).cross(p - a);
    const double c2 = (c - b).cross(p - b);
    const double c3 = (a - c).cross(p - c);
    return (c1 >= 0.0 && c2 >= 0.0 && c3 >= 0.0) ||
        (c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0);
}

static bool shapeSegmentHit(const Raster::Vec2& point, const Raster::Vec2& a,
                            const Raster::Vec2& b, double radius,
                            uint32_t cap, bool capStart, bool capEnd) {
    const Raster::Vec2 delta = b - a;
    const double length = delta.length();
    if (length <= 1e-12) {
        const Raster::Vec2 d = point - a;
        return cap == 1 && d.dot(d) <= radius * radius;
    }
    const double t = (point - a).dot(delta) / (length * length);
    double lo = 0.0, hi = 1.0;
    if (cap == 2) {
        if (capStart) lo -= radius / length;
        if (capEnd) hi += radius / length;
    }
    const double clamped = std::clamp(t, lo, hi);
    const Raster::Vec2 difference = point - (a + delta * clamped);
    if (difference.dot(difference) <= radius * radius) return true;
    if (cap != 1) return false;
    if (capStart && (point - a).dot(point - a) <= radius * radius && t < lo) return true;
    return capEnd && (point - b).dot(point - b) <= radius * radius && t > hi;
}

static bool shapePointInStroke(const Raster::Vec2& point,
                               const std::vector<ShapeContour>& paths,
                               const ShapeDraw& draw) {
    if (!std::isfinite(draw.width) || draw.width <= 0.0) return false;
    const double radius = draw.width * 0.5;
    for (const ShapeContour& path : paths) {
        if (path.points.size() < 2) continue;
        const size_t segmentCount = path.points.size() - 1;
        double distanceAlong = 0.0;
        for (size_t i = 0; i < segmentCount; ++i) {
            const Raster::Vec2 a = path.points[i];
            const Raster::Vec2 b = path.points[i + 1];
            const double length = (b - a).length();
            if (draw.dash.empty()) {
                if (shapeSegmentHit(point, a, b, radius, 0,
                                    !path.closed && i == 0,
                                    !path.closed && i + 1 == segmentCount))
                    return true;
            } else if (length > 1e-12) {
                double patternTotal = 0.0;
                for (double value : draw.dash) patternTotal += value;
                if (patternTotal > 0.0 && std::isfinite(patternTotal)) {
                    double offset = std::isfinite(draw.dashOffset)
                        ? std::fmod(draw.dashOffset, patternTotal) : 0.0;
                    if (offset < 0.0) offset += patternTotal;
                    double local = 0.0;
                    while (local < length) {
                        double phase = std::fmod(distanceAlong + local + offset, patternTotal);
                        if (phase < 0.0) phase += patternTotal;
                        size_t dashIndex = 0;
                        while (dashIndex + 1 < draw.dash.size() && phase >= draw.dash[dashIndex]) {
                            phase -= draw.dash[dashIndex++];
                        }
                        const double dashLength = std::max(1e-9, draw.dash[dashIndex]);
                        const double step = std::min(length - local, dashLength - phase);
                        if ((dashIndex & 1u) == 0u) {
                            const Raster::Vec2 from = a + (b - a) * (local / length);
                            const Raster::Vec2 to = a + (b - a) * ((local + step) / length);
                            if (shapeSegmentHit(point, from, to, radius, draw.cap, true, true))
                                return true;
                        }
                        local += step;
                    }
                }
            }
            distanceAlong += length;
        }
        if (!draw.dash.empty()) continue;
        if (!path.closed) {
            const Raster::Vec2 first = path.points.front();
            const Raster::Vec2 last = path.points.back();
            if (draw.cap == 1) {
                if ((point - first).dot(point - first) <= radius * radius ||
                    (point - last).dot(point - last) <= radius * radius)
                    return true;
            } else if (draw.cap == 2) {
                const Raster::Vec2 firstDir = (path.points[1] - first).normalize();
                const Raster::Vec2 lastDir = (last - path.points[path.points.size() - 2]).normalize();
                const Raster::Vec2 fromFirst = point - first;
                const Raster::Vec2 fromLast = point - last;
                if (fromFirst.dot(firstDir) >= -radius && fromFirst.dot(firstDir) <= 0.0 &&
                    std::abs(fromFirst.cross(firstDir)) <= radius) return true;
                if (fromLast.dot(lastDir) >= 0.0 && fromLast.dot(lastDir) <= radius &&
                    std::abs(fromLast.cross(lastDir)) <= radius) return true;
            }
        }
        const size_t vertexCount = path.closed ? segmentCount : segmentCount - 1;
        for (size_t i = 0; i < vertexCount; ++i) {
            const size_t current = path.closed ? i : i + 1;
            const size_t previous = current == 0 ? segmentCount - 1 : current - 1;
            const size_t next = current % segmentCount;
            const Raster::Vec2 vertex = path.points[current];
            const Raster::Vec2 incoming = (vertex - path.points[previous]).normalize();
            const Raster::Vec2 outgoing = (path.points[next + 1] - vertex).normalize();
            const double turn = incoming.cross(outgoing);
            if (std::abs(turn) <= 1e-12) continue;
            if (draw.join == 1) {
                const Raster::Vec2 delta = point - vertex;
                if (delta.dot(delta) <= radius * radius) return true;
                continue;
            }
            const double side = turn > 0.0 ? -1.0 : 1.0;
            const Raster::Vec2 n1{-incoming.y * side, incoming.x * side};
            const Raster::Vec2 n2{-outgoing.y * side, outgoing.x * side};
            const Raster::Vec2 q1 = vertex + n1 * radius;
            const Raster::Vec2 q2 = vertex + n2 * radius;
            if (draw.join == 0) {
                const double denominator = incoming.cross(outgoing);
                const double along = (q2 - q1).cross(outgoing) / denominator;
                const Raster::Vec2 miter = q1 + incoming * along;
                if ((miter - vertex).length() <= draw.miter * radius) {
                    if (shapePointInTriangle(point, q1, miter, q2)) return true;
                    continue;
                }
            }
            if (shapePointInTriangle(point, vertex, q1, q2)) return true;
        }
    }
    return false;
}

static Raster::Image renderShapeSource(const Project::Layer& layer,
                                      uint32_t width, uint32_t height,
                                      Project::Tick compTime) {
    Raster::Image image(width, height);
    const Props::PropGroup* contents = layer.props.group("contents");
    if (!contents || width == 0 || height == 0) return image;
    const Project::Tick time = layer.layerTime(compTime);
    const Raster::Mat3 origin = Raster::Mat3::translate(
        Raster::vec2(static_cast<double>(width) * 0.5, static_cast<double>(height) * 0.5));
    std::vector<ShapeSlot> arena;
    std::vector<size_t> live;
    std::vector<ShapeDraw> draws;
    collectShapeDraws(*contents, time, origin, 1.0f, arena, live, draws);

    constexpr int samples = 4;
    for (auto draw = draws.rbegin(); draw != draws.rend(); ++draw) {
        if (draw->opacity <= 0.0f || draw->slots.empty()) continue;
        std::vector<ShapeContour> paths;
        for (size_t slot : draw->slots)
            if (slot < arena.size())
                paths.insert(paths.end(), arena[slot].paths.begin(), arena[slot].paths.end());
        if (paths.empty()) continue;
        double minX = static_cast<double>(width), minY = static_cast<double>(height);
        double maxX = 0.0, maxY = 0.0;
        for (const ShapeContour& path : paths) {
            for (const Raster::Vec2& point : path.points) {
                minX = std::min(minX, point.x);
                minY = std::min(minY, point.y);
                maxX = std::max(maxX, point.x);
                maxY = std::max(maxY, point.y);
            }
        }
        const double pad = draw->stroke ? std::max(0.0, draw->width * 0.5 + 2.0) : 1.0;
        const int x0 = static_cast<int>(std::clamp(std::floor(minX - pad), 0.0, static_cast<double>(width)));
        const int y0 = static_cast<int>(std::clamp(std::floor(minY - pad), 0.0, static_cast<double>(height)));
        const int x1 = static_cast<int>(std::clamp(std::ceil(maxX + pad), 0.0, static_cast<double>(width)));
        const int y1 = static_cast<int>(std::clamp(std::ceil(maxY + pad), 0.0, static_cast<double>(height)));
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                int covered = 0;
                for (int sy = 0; sy < samples; ++sy) {
                    for (int sx = 0; sx < samples; ++sx) {
                        const Raster::Vec2 point{x + (sx + 0.5) / samples,
                                                 y + (sy + 0.5) / samples};
                        const bool hit = draw->stroke
                            ? shapePointInStroke(point, paths, *draw)
                            : shapePointInFill(point.x, point.y, paths, draw->evenOdd);
                        if (hit) ++covered;
                    }
                }
                if (covered == 0) continue;
                const float alpha = draw->opacity * static_cast<float>(covered) /
                    static_cast<float>(samples * samples);
                Raster::Px source{
                    draw->color[0] * alpha, draw->color[1] * alpha,
                    draw->color[2] * alpha, alpha
                };
                const size_t index = image.idx(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                image.data[index] = Raster::blend_pixel(Raster::BlendMode::Normal, image.data[index], source);
            }
        }
    }
    return image;
}

} // namespace

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
        case Project::LayerKind::Shape:
            return renderShapeSource(layer, lw, lh, t);
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
