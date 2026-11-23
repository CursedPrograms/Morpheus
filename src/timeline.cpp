// Timeline time mapping and interaction math.
// after EffectCraft (crates/ui-egui/src/panels/timeline.rs TMap, fit_pps, set_zoom)
#include "timeline.h"
#include "project.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>

namespace Timeline {

double fitPps(double width, double duration) {
    if (!std::isfinite(width) || !std::isfinite(duration) || duration <= 0.0)
        return 1.0;
    return std::max(width - 10.0, 1.0) / std::max(duration, 0.01);
}

void setZoom(double& start, double& pixelsPerSecond, double requestedPps,
             double fit, double duration, double atTime, double atPixel) {
    if (!std::isfinite(fit) || fit <= 0.0 || !std::isfinite(duration) || duration <= 0.0) {
        start = 0.0;
        pixelsPerSecond = 0.0;
        return;
    }
    constexpr double MAX_PPS = 4000.0;
    const double safeFit = std::max(1.0, fit);
    const double pps = std::clamp(std::isfinite(requestedPps) ? requestedPps : safeFit,
                                  safeFit, std::max(MAX_PPS, safeFit));
    if (pps <= safeFit) {
        start = 0.0;
        pixelsPerSecond = 0.0;
        return;
    }
    pixelsPerSecond = pps;
    const double candidate = std::isfinite(atTime) && std::isfinite(atPixel)
        ? atTime - atPixel / pps : 0.0;
    start = std::clamp(candidate, 0.0, duration);
}

double pageToCti(double start, double span, double duration, double cti) {
    if (!std::isfinite(start) || !std::isfinite(span) ||
        !std::isfinite(duration) || !std::isfinite(cti) || span <= 0.0)
        return std::isfinite(start) ? start : 0.0;
    if (span >= duration)
        return 0.0;
    if (cti < start)
        return std::max(0.0, cti);
    if (cti > start + span)
        return std::min(std::max(0.0, duration - span), cti);
    return start;
}

double centeredOn(double span, double duration, double cti) {
    if (!std::isfinite(span) || !std::isfinite(duration) || !std::isfinite(cti) ||
        span <= 0.0 || duration <= 0.0)
        return 0.0;
    return std::clamp(cti - span * 0.5, 0.0, std::max(0.0, duration - span));
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs edge_scroll_speed_x
double edgeScrollSpeedX(double x0, double x1, double x) {
    if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(x) || x0 > x1)
        return 0.0;
    const double depth = x < x0 ? x - x0 : x > x1 ? x - x1 : 0.0;
    if (depth == 0.0) return 0.0;
    return std::clamp(depth / 40.0, -3.0, 3.0) * 400.0 + std::copysign(100.0, depth);
}

double snapTime(const double* candidates, size_t count, double time, double tolerance) {
    if (!std::isfinite(time) || !std::isfinite(tolerance) || tolerance < 0.0)
        return time;
    double best = time;
    double distance = tolerance;
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
        const double candidate = candidates[i];
        if (!std::isfinite(candidate))
            continue;
        const double d = std::abs(candidate - time);
        if (d <= tolerance && (!found || d < distance)) {
            best = candidate;
            distance = d;
            found = true;
        }
    }
    return best;
}

Project::Tick timeAtPixel(double x, double x0, double start, double pps,
                          Project::FrameRate frameRate, Project::Tick duration) {
    if (!std::isfinite(x) || !std::isfinite(x0) || !std::isfinite(start) ||
        !std::isfinite(pps) || pps <= 0.0)
        return Project::Tick::ZERO;
    const double seconds = std::clamp(start + (x - x0) / pps, 0.0, duration.seconds());
    return frameRate.snap_nearest(Project::Tick::from_seconds_f64(seconds));
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs shift_axes
std::pair<bool, bool> shiftAxes(bool shift, float dx, float dy) {
    if (shift) return {std::abs(dx) >= std::abs(dy), std::abs(dy) > std::abs(dx)};
    return {true, true};
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs HandleGrab::ease
KeyframeModel::Ease easeAtHandle(KeyframeModel::Ease current, double keyTime, double keyValue,
                                 double desiredTime, double desiredValue, double duration,
                                 bool out, bool speedGraph) {
    const double direction = out ? 1.0 : -1.0;
    const double safeDuration = std::max(duration, 1e-9);
    const double offset = std::max((desiredTime - keyTime) * direction, safeDuration * 0.001);
    const double influence = std::clamp(offset / safeDuration, 0.001, 1.0);
    const double speed = speedGraph
        ? std::copysign(std::max(0.0, desiredValue), current.speed)
        : (desiredValue - keyValue) / offset * direction;
    return {speed, influence};
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs keys_in_box
std::vector<KeyRef> keysInBox(const std::vector<KeyMarker>& markers,
                              float minX, float minY, float maxX, float maxY) {
    std::vector<KeyRef> selected;
    if (minX > maxX || minY > maxY) return selected;
    for (const KeyMarker& marker : markers) {
        const float half = std::max(0.0f, marker.glyphSize) * 0.5f;
        if (marker.x + half >= minX && marker.x - half <= maxX &&
            marker.y + half >= minY && marker.y - half <= maxY)
            selected.push_back({marker.layer, marker.property, marker.time});
    }
    return selected;
}

// after EffectCraft crates/engine/src/cmd/prop.rs move_key_refs
KeyMoveResult moveSelectedKeys(Project::Composition& comp,
                               const std::vector<KeyRef>& selected,
                               Project::Tick compDelta, KeyRef grabbed,
                               const std::optional<KeyframeModel::Value>& replacement) {
    struct Move {
        KeyRef source;
        Project::Tick target;
        KeyframeModel::Keyframe key;
        size_t selectionIndex = 0;
    };
    using Group = std::pair<uint64_t, Props::Uid>;
    std::map<Group, std::vector<Move>> grouped;
    std::unordered_map<size_t, KeyRef> remapped;
    std::vector<KeyRef> unique;
    for (const KeyRef& ref : selected) {
        if (std::find_if(unique.begin(), unique.end(), [&](const KeyRef& old) {
                return old.layer == ref.layer && old.property == ref.property && old.time == ref.time;
            }) != unique.end())
            continue;
        unique.push_back(ref);

        auto layer = std::find_if(comp.layers.begin(), comp.layers.end(),
            [&](const Project::Layer& candidate) { return candidate.id.v == ref.layer; });
        if (layer == comp.layers.end()) continue;
        Props::Property* property = layer->props.find_mut(ref.property);
        if (!property) continue;
        auto key = std::find_if(property->keys.begin(), property->keys.end(),
            [&](const KeyframeModel::Keyframe& candidate) { return candidate.time == ref.time; });
        if (key == property->keys.end()) continue;
        if (layer->switches.locked) {
            remapped.emplace(unique.size() - 1, ref);
            continue;
        }
        const Project::Tick target = layer->layerTime(layer->compTime(ref.time) + compDelta);
        grouped[{ref.layer, ref.property}].push_back({ref, target, *key, unique.size() - 1});
    }

    for (auto& [group, moves] : grouped) {
        auto layer = std::find_if(comp.layers.begin(), comp.layers.end(),
            [&](const Project::Layer& candidate) { return candidate.id.v == group.first; });
        if (layer == comp.layers.end()) continue;
        Props::Property* property = layer->props.find_mut(group.second);
        if (!property) continue;

        for (const Move& move : moves) {
            property->keys.erase(std::remove_if(property->keys.begin(), property->keys.end(),
                [&](const KeyframeModel::Keyframe& key) { return key.time == move.source.time; }),
                property->keys.end());
        }

        std::unordered_map<int64_t, size_t> targetOwners;
        for (size_t moveIndex = 0; moveIndex < moves.size(); ++moveIndex) {
            Move& move = moves[moveIndex];
            move.key.time = move.target;
            if (replacement && move.source.layer == grabbed.layer &&
                move.source.property == grabbed.property && move.source.time == grabbed.time)
                move.key.value = *replacement;
            KeyframeModel::set_key(property->keys, std::move(move.key));
            targetOwners[move.target.v] = moveIndex;
        }

        std::unordered_map<size_t, size_t> sourceAtIndex;
        for (const auto& [targetTicks, owner] : targetOwners) {
            auto key = std::find_if(property->keys.begin(), property->keys.end(),
                [&](const KeyframeModel::Keyframe& candidate) { return candidate.time.v == targetTicks; });
            if (key != property->keys.end())
                sourceAtIndex.emplace(owner, static_cast<size_t>(key - property->keys.begin()));
        }
        KeyframeModel::retime_roving(property->keys, property->spatial);
        for (const auto& [owner, index] : sourceAtIndex) {
            if (index >= property->keys.size()) continue;
            const Move& move = moves[owner];
            remapped.emplace(move.selectionIndex,
                KeyRef{move.source.layer, move.source.property, property->keys[index].time});
        }
    }

    KeyMoveResult result;
    for (size_t i = 0; i < unique.size(); ++i) {
        const auto mapped = remapped.find(i);
        if (mapped == remapped.end()) continue;
        const KeyRef& ref = mapped->second;
        if (unique[i].layer == grabbed.layer && unique[i].property == grabbed.property &&
            unique[i].time == grabbed.time)
            result.grabbed = ref;
        if (std::none_of(result.selected.begin(), result.selected.end(), [&](const KeyRef& existing) {
                return existing.layer == ref.layer && existing.property == ref.property &&
                    existing.time == ref.time;
            }))
            result.selected.push_back(ref);
    }
    return result;
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs key_select
bool selectKey(std::vector<KeyRef>& selected, KeyRef key, bool toggle) {
    auto found = std::find_if(selected.begin(), selected.end(), [&](const KeyRef& item) {
        return item.layer == key.layer && item.property == key.property && item.time == key.time;
    });
    if (toggle && found != selected.end()) {
        selected.erase(found);
        return false;
    }
    if (found == selected.end()) {
        if (!toggle) selected.clear();
        selected.push_back(key);
    }
    return true;
}

LayerTiming editLayerTiming(LayerTiming original, BarEdit edit, Project::Tick offset,
                            Project::Tick frameDuration, Project::Tick compDuration) {
    const Project::Tick oneFrame = frameDuration.v > 0 ? frameDuration : Project::Tick(1);
    if (edit == BarEdit::TrimIn) {
        original.in = (original.in + offset).max_t(Project::Tick::ZERO).min_t(original.out - oneFrame);
    } else if (edit == BarEdit::TrimOut) {
        original.out = (original.out + offset).min_t(compDuration).max_t(original.in + oneFrame);
    } else {
        const Project::Tick newIn = (original.in + offset).max_t(Project::Tick::ZERO);
        const Project::Tick move = newIn - original.in;
        original.in = newIn;
        original.out += move;
        original.start += move;
    }
    return original;
}

// after EffectCraft crates/engine/src/cmd/prop.rs toggle_key_at
bool toggleKeyframe(Props::Property& property, Project::Tick layerTime) {
    const auto key = std::lower_bound(property.keys.begin(), property.keys.end(), layerTime,
        [](const KeyframeModel::Keyframe& candidate, Project::Tick time) {
            return candidate.time < time;
        });
    if (key != property.keys.end() && key->time == layerTime) {
        if (property.keys.size() == 1)
            property.value = property.keys.front().value;
        property.keys.erase(key);
        return false;
    }

    KeyframeModel::Keyframe added(layerTime, property.value_at(layerTime));
    if (property.hold_only)
        added = added.hold();
    KeyframeModel::set_key(property.keys, added);
    return true;
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs layer_reorder_drag
std::vector<uint64_t> reorderLayers(const std::vector<uint64_t>& order,
                                    const std::vector<uint64_t>& moving,
                                    std::optional<uint64_t> before) {
    if (before && std::find(moving.begin(), moving.end(), *before) != moving.end())
        return order;
    std::vector<uint64_t> block;
    std::vector<uint64_t> result;
    for (uint64_t id : order) {
        if (std::find(moving.begin(), moving.end(), id) != moving.end())
            block.push_back(id);
        else
            result.push_back(id);
    }
    if (block.empty()) return order;
    const auto target = before ? std::find(result.begin(), result.end(), *before) : result.end();
    result.insert(target, block.begin(), block.end());
    return result;
}

} // namespace Timeline
