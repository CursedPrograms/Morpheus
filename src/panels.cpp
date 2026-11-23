// Morpheus - panel layout matching the EffectCraft Default workspace.
// Ported from crates/ui-egui/src/dock.rs (workspace_root "Default"),
// crates/ui-egui/src/header.rs (tool bar), and each panel in crates/ui-egui/src/panels/.
//
// Default layout (after EffectCraft crates/ui-egui/src/dock.rs workspace_root):
//   [Header / Tools bar - full width, ~38 px]
//   [Left 300px | Centre flex | Right 300px]
//   Left:   Project / Effect Controls tabs (full height)
//   Centre: Composition / Layer tabs (top ~56%) over Timeline / Render Queue tabs
//   Right:  Preview tab (top ~PREVIEW_FRAME) over Properties / Align / Audio tabs
//
// Label colours from EffectCraft (after crates/engine/src/color.rs Label / theme.rs):
//   None=grey, Red, Yellow, Aqua, Pink, Lavender, Peach, Sea Foam, Blue, Green,
//   Purple, Orange, Brown, Fuchsia, Cyan, Sandstone, Dark Green
//
// Atelier colours: dark greys, #007bb7 accent, solid, no gradients.
//
// after EffectCraft (crates/ui-egui/src/dock.rs, crates/ui-egui/src/header.rs,
//                    crates/ui-egui/src/icons.rs,
//                    crates/ui-egui/src/panels/project.rs,
//                    crates/ui-egui/src/panels/viewer.rs,
//                    crates/ui-egui/src/panels/timeline.rs,
//                    crates/ui-egui/src/panels/live_preview.rs,
//                    crates/ui-egui/src/panels/properties.rs,
//                    crates/ui-egui/src/panels/effect_controls.rs,
//                    crates/ui-egui/src/panels/info.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "panels.h"
#include "project.h"
#include "time_.h"
#include "lang.h"
#include "icons.h"
#include "render.h"
#include "timeline.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <SDL_opengl.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <optional>

// Suppress min/max macro conflicts.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panels {

// ---------- Atelier colour palette ----------
// after EffectCraft crates/ui-egui/src/theme.rs
static const ImU32
    C_APP_BG   = IM_COL32(0x14, 0x14, 0x14, 255),   // app background / window bg
    C_HEADER   = IM_COL32(0x1c, 0x1c, 0x1c, 255),   // header / tool bar background
    C_PANEL    = IM_COL32(0x22, 0x22, 0x22, 255),   // panel background
    C_TAB_BG   = IM_COL32(0x1a, 0x1a, 0x1a, 255),   // inactive tab
    C_TAB_ACT  = IM_COL32(0x28, 0x28, 0x28, 255),   // active tab background
    C_LINE     = IM_COL32(0x38, 0x38, 0x38, 255),   // panel dividers / borders
    C_TEXT     = IM_COL32(0xe0, 0xe0, 0xe0, 255),   // primary text
    C_DIM      = IM_COL32(0x80, 0x80, 0x80, 255),   // secondary / dim text
    C_FAINT    = IM_COL32(0x50, 0x50, 0x50, 255),   // very dim text / hint
    C_BLUE     = IM_COL32(0x00, 0x7b, 0xb7, 255),   // Atelier blue accent
    C_DEEP     = IM_COL32(0x00, 0x51, 0x84, 255),   // darker blue (hover)
    C_WELL     = IM_COL32(0x14, 0x14, 0x14, 255);   // input field background

// Label colours matching EffectCraft's Label enum (after crates/engine/src/color.rs Label).
// These 17 entries correspond to Label::None..Label::DarkGreen in order.
static const ImU32 LABEL_COLORS[] = {
    IM_COL32(0x60,0x60,0x60,255), // None   – grey
    IM_COL32(0xc8,0x44,0x44,255), // Red
    IM_COL32(0xc8,0xa0,0x44,255), // Yellow
    IM_COL32(0x44,0xb0,0xb8,255), // Aqua
    IM_COL32(0xb8,0x44,0x88,255), // Pink
    IM_COL32(0x88,0x68,0xc0,255), // Lavender
    IM_COL32(0xc8,0x78,0x44,255), // Peach
    IM_COL32(0x44,0xa0,0x88,255), // Sea Foam
    IM_COL32(0x44,0x6c,0xb8,255), // Blue
    IM_COL32(0x44,0xa0,0x44,255), // Green
    IM_COL32(0x68,0x44,0xb8,255), // Purple
    IM_COL32(0xc8,0x68,0x44,255), // Orange
    IM_COL32(0x78,0x50,0x38,255), // Brown
    IM_COL32(0xb8,0x44,0xb0,255), // Fuchsia
    IM_COL32(0x44,0xb8,0xb8,255), // Cyan
    IM_COL32(0xb8,0xa0,0x68,255), // Sandstone
    IM_COL32(0x38,0x68,0x38,255), // Dark Green
};
static ImU32 labelColor(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(LABEL_COLORS)/sizeof(LABEL_COLORS[0])))
        return LABEL_COLORS[0];
    return LABEL_COLORS[idx];
}

static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// ----- panel/tab strip heights -----
static const float HEADER_H  = 38.0f;   // Tools bar height
static const float TAB_H     = 25.0f;   // panel tab strip height
static const float ROW_H     = 19.0f;   // project panel row height
static const float LEFT_W    = 300.0f;  // left column width (Project / FX)
static const float RIGHT_W   = 300.0f;  // right column width (Preview / Properties)
static const float PREVIEW_H = TAB_H + 180.0f; // Preview frame height (tab + controls)
static const float SPLIT_Y   = 0.56f;   // centre split (comp over timeline)
static const float DIVIDER   = 1.0f;    // pixel divider width

// ----- per-panel active-tab state -----
static int s_leftTab   = 0;  // 0=Project 1=EffectControls
static int s_centreTop = 0;  // 0=Composition 1=Layer
static int s_centreBot = 0;  // 0=Timeline 1=RenderQueue
static int s_rightBot  = 0;  // 0=Properties 1=Align 2=Audio 3=EffectsPresets
static const Actions* g_actions = nullptr;
static Project::Tick s_currentTime = Project::Tick::ZERO;
static bool s_playing = false;
static bool s_loop = true;
static double s_timelineStart = 0.0;
static double s_timelinePps = 0.0;
static float s_timelineZoom = 0.0f;
static float s_timelineScroll = 0.0f;
static uint64_t s_selectedLayer = 0;
static uint64_t s_selectedShapeLayer = 0;
static uint64_t s_selectedShapeGroup = 0;
static std::unordered_set<uint64_t> s_selectedLayers;
static uint64_t s_selectionAnchor = 0;
static bool s_graphEditor = false;
static std::unordered_set<uint64_t> s_openLayers;
static std::unordered_set<uint64_t> s_openGroups;
static Render::FrameCache s_frameCache;
static uint64_t s_cacheGeneration = 0;

struct SelectedKey {
    uint64_t layer = 0;
    Props::Uid property = 0;
    Project::Tick time = Project::Tick::ZERO;
    size_t dimension = 0;
};

struct KeyDrag {
    uint64_t layer = 0;
    Props::Uid property = 0;
    Project::Tick startTime = Project::Tick::ZERO;
    Project::Tick currentTime = Project::Tick::ZERO;
    KeyframeModel::Value value;
    size_t dimension = 0;
    float startX = 0.0f;
    float startY = 0.0f;
    double graphMin = 0.0;
    double graphMax = 1.0;
    float graphHeight = 1.0f;
    bool graph = false;
};
struct GraphEaseDrag {
    uint64_t layer = 0;
    Props::Uid property = 0;
    Project::Tick time = Project::Tick::ZERO;
    size_t dimension = 0;
    bool out = false;
    ImVec2 press{};
    double handleTime = 0.0;
    double handleValue = 0.0;
    double keyValue = 0.0;
    double duration = 0.0;
    KeyframeModel::Ease ease;
};

static SelectedKey s_selectedKey;
static std::vector<Timeline::KeyRef> s_selectedKeys;
static KeyDrag s_keyDrag;
static GraphEaseDrag s_graphEaseDrag;
static bool s_graphSpeed = false;
struct LayerRowHit { ImVec2 min; ImVec2 max; uint64_t id; size_t index; };
struct PropertyRowHit { float y; size_t layer; Props::Uid property; };
static std::vector<LayerRowHit> s_layerRowHits;
static std::vector<PropertyRowHit> s_propertyRowHits;
static bool s_layerReorderActive = false;
static std::vector<uint64_t> s_layerReorderIds;
static bool s_layerMarqueeActive = false;
static ImVec2 s_layerMarqueeStart{};
static bool s_keyMarqueeActive = false;
static ImVec2 s_keyMarqueeStart{};
static bool s_keyMarkerClicked = false;

static bool sameKeyRef(const Timeline::KeyRef& a, const SelectedKey& b) {
    return a.layer == b.layer && a.property == b.property && a.time == b.time;
}

static bool keyIsSelected(uint64_t layer, Props::Uid property, Project::Tick time,
                          size_t /*dimension*/, bool /*graph*/) {
    return std::any_of(s_selectedKeys.begin(), s_selectedKeys.end(), [&](const Timeline::KeyRef& key) {
        return key.layer == layer && key.property == property && key.time == time;
    });
}

static void selectKey(SelectedKey key, bool toggle) {
    const Timeline::KeyRef ref{key.layer, key.property, key.time};
    const bool remains = Timeline::selectKey(s_selectedKeys, ref, toggle);
    if (remains) s_selectedKey = key;
    else if (sameKeyRef(ref, s_selectedKey)) {
        s_selectedKey = s_selectedKeys.empty()
            ? SelectedKey{}
            : SelectedKey{s_selectedKeys.back().layer, s_selectedKeys.back().property,
                          s_selectedKeys.back().time, 0};
    }
}

void invalidateFrameCache() {
    s_frameCache.clear();
    ++s_cacheGeneration;
}

void prepareTimelineCapture(uint64_t layerId) {
    s_selectedLayer = layerId;
    s_selectedShapeLayer = 0;
    s_selectedShapeGroup = 0;
    s_selectedLayers.clear();
    s_selectedLayers.insert(layerId);
    s_selectionAnchor = layerId;
    s_openLayers.clear();
    s_openLayers.insert(layerId);
    s_openGroups.clear();
    s_timelineStart = 0.0;
    s_timelinePps = 0.0;
    s_timelineZoom = 0.0f;
    s_currentTime = Project::Tick::ZERO;
    s_playing = false;
    s_graphEditor = false;
    s_selectedKey = {};
    s_selectedKeys.clear();
    s_keyDrag = {};
    s_graphEaseDrag = {};
    s_graphSpeed = false;
    s_layerRowHits.clear();
    s_layerReorderActive = false;
    s_layerReorderIds.clear();
    s_layerMarqueeActive = false;
    s_propertyRowHits.clear();
    s_keyMarqueeActive = false;
    invalidateFrameCache();
}

void prepareShapeCapture(uint64_t layerId, uint64_t contentsId,
                         const std::vector<uint64_t>& groupIds) {
    prepareTimelineCapture(layerId);
    s_selectedShapeLayer = layerId;
    s_selectedShapeGroup = groupIds.empty() ? 0 : groupIds.front();
    s_openGroups.insert(contentsId);
    for (uint64_t groupId : groupIds)
        if (groupId != 0) s_openGroups.insert(groupId);
}

void prepareKeyframeCapture(uint64_t layerId, bool graphEditor, bool speedGraph,
                            uint64_t propertyId, int64_t keyTimeTicks, size_t dimension) {
    prepareTimelineCapture(layerId);
    s_graphEditor = graphEditor;
    s_graphSpeed = speedGraph;
    if (propertyId != 0) {
        s_selectedKey = {layerId, propertyId, Project::Tick(keyTimeTicks), dimension};
        s_selectedKeys = {{layerId, propertyId, Project::Tick(keyTimeTicks)}};
    }
}

static void setCurrentTime(Project::Tick time) {
    s_currentTime = time;
}

static const Raster::Image* cachedFrame(const Project::Project* proj, const Project::Composition& comp) {
    const int64_t frame = comp.frameRate.frame_at(s_currentTime);
    if (const Raster::Image* cached = s_frameCache.find(comp.id.v, frame, 0.5))
        return cached;
    Raster::Image rendered = Render::Renderer::render_frame(*proj, comp.id.v, comp.frameRate.tick_of(frame), 0.5);
    s_frameCache.insert(comp.id.v, frame, 0.5, std::move(rendered));
    return s_frameCache.find(comp.id.v, frame, 0.5);
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs numeric graph components
static size_t numericDimensions(const KeyframeModel::Value& value) {
    switch (value.kind) {
    case KeyframeModel::ValueKind::Scalar: return 1;
    case KeyframeModel::ValueKind::Vec2: return 2;
    case KeyframeModel::ValueKind::Vec3: return 3;
    default: return 0;
    }
}

// after EffectCraft crates/keyframe/src/value.rs Value::components
static double numericComponent(const KeyframeModel::Value& value, size_t dimension) {
    switch (value.kind) {
    case KeyframeModel::ValueKind::Scalar: return value.scalar;
    case KeyframeModel::ValueKind::Vec2: return dimension < 2 ? value.v2[dimension] : 0.0;
    case KeyframeModel::ValueKind::Vec3: return dimension < 3 ? value.v3[dimension] : 0.0;
    default: return 0.0;
    }
}

// Replace one numeric component while preserving the key's other dimensions.
static void setNumericComponent(KeyframeModel::Value& value, size_t dimension, double component) {
    switch (value.kind) {
    case KeyframeModel::ValueKind::Scalar:
        if (dimension == 0) value.scalar = component;
        break;
    case KeyframeModel::ValueKind::Vec2:
        if (dimension < 2) value.v2[dimension] = component;
        break;
    case KeyframeModel::ValueKind::Vec3:
        if (dimension < 3) value.v3[dimension] = component;
        break;
    default:
        break;
    }
}

// after EffectCraft crates/ui-egui/src/panels/graph.rs speed_at and curve sampling
static double graphComponentAt(const Project::Layer& layer, const Props::Property& property,
                               Project::Tick compTime, size_t dimension) {
    const Project::Tick layerTime = layer.layerTime(compTime);
    if (s_graphSpeed) {
        if (property.spatial &&
            (property.value.kind == KeyframeModel::ValueKind::Vec2 ||
             property.value.kind == KeyframeModel::ValueKind::Vec3))
            return KeyframeModel::speed(property.keys, layerTime, true);
        const auto velocity = KeyframeModel::velocity(property.keys, layerTime, false);
        return dimension < velocity.size() ? std::abs(velocity[dimension]) : 0.0;
    }
    return numericComponent(property.value_at(layerTime), dimension);
}

// Draw and edit a selected key's temporal Bezier influence/speed handle.
static void graphEaseHandle(Project::Project* project, Project::Layer& layer,
                            Props::Property& property, size_t keyIndex, size_t dimension,
                            bool out, ImDrawList* drawList, ImVec2 graphMin, ImVec2 graphMax,
                            double valueMin, double valueMax, double pixelsPerSecond) {
    if (layer.switches.locked || keyIndex >= property.keys.size()) return;
    if (property.spatial && !s_graphSpeed) return;
    const bool selected = s_selectedKey.layer == layer.id.v &&
        s_selectedKey.property == property.uid && s_selectedKey.time == property.keys[keyIndex].time &&
        s_selectedKey.dimension == dimension;
    if (!selected) return;

    const auto ease = KeyframeModel::side_ease(property.keys, keyIndex, dimension, property.spatial, out);
    if (!ease) return;
    const size_t neighborIndex = out ? keyIndex + 1 : keyIndex - 1;
    if (neighborIndex >= property.keys.size()) return;
    const double duration = std::abs(property.keys[neighborIndex].time.seconds() -
                                      property.keys[keyIndex].time.seconds());
    if (!(duration > 0.0) || !std::isfinite(duration)) return;
    const Project::Tick compTime = layer.compTime(property.keys[keyIndex].time);
    const double keyTime = compTime.seconds();
    const double signedOffset = ease->influence * duration * (out ? 1.0 : -1.0);
    double keyValue = numericComponent(property.keys[keyIndex].value, dimension);
    if (s_graphSpeed) keyValue = graphComponentAt(layer, property, compTime, dimension);
    const double handleValue = s_graphSpeed ? std::abs(ease->speed) :
        keyValue + ease->speed * signedOffset;
    const double graphDuration = std::max(1e-12, valueMax - valueMin);
    const float handleY = graphMax.y - static_cast<float>((handleValue - valueMin) / graphDuration) *
        (graphMax.y - graphMin.y);
    const ImVec2 keyPoint{graphMin.x + static_cast<float>((keyTime - s_timelineStart) * pixelsPerSecond),
        graphMax.y - static_cast<float>((keyValue - valueMin) / graphDuration) * (graphMax.y - graphMin.y)};
    const ImVec2 handlePoint{graphMin.x + static_cast<float>((keyTime + signedOffset - s_timelineStart) *
        pixelsPerSecond), handleY};
    const ImU32 handleColor = IM_COL32(216, 216, 96, 255);
    drawList->AddLine(keyPoint, handlePoint, handleColor, 1.0f);
    drawList->AddCircleFilled(handlePoint, 3.5f, handleColor);

    char widgetId[128];
    snprintf(widgetId, sizeof widgetId, "##ease_%llu_%llu_%zu_%d",
        static_cast<unsigned long long>(layer.id.v),
        static_cast<unsigned long long>(property.uid), keyIndex, out ? 1 : 0);
    ImGui::SetCursorScreenPos({handlePoint.x - 6.0f, handlePoint.y - 6.0f});
    ImGui::InvisibleButton(widgetId, {12.0f, 12.0f});
    if (ImGui::IsItemActivated()) {
        s_graphEaseDrag = {layer.id.v, property.uid, property.keys[keyIndex].time,
            dimension, out, ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left],
            keyTime + signedOffset, handleValue, keyValue, duration, *ease};
    }
    const bool ownsDrag = s_graphEaseDrag.layer == layer.id.v &&
        s_graphEaseDrag.property == property.uid &&
        s_graphEaseDrag.time == property.keys[keyIndex].time &&
        s_graphEaseDrag.out == out && s_graphEaseDrag.dimension == dimension;
    if (ownsDrag && ImGui::IsMouseDown(ImGuiMouseButton_Left) && pixelsPerSecond > 0.0) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float dx = mouse.x - s_graphEaseDrag.press.x;
        const float dy = mouse.y - s_graphEaseDrag.press.y;
        const auto [timeAxis, valueAxis] =
            Timeline::shiftAxes(ImGui::GetIO().KeyShift, dx, dy);
        const double desiredTime = s_graphEaseDrag.handleTime +
            (timeAxis ? dx / pixelsPerSecond : 0.0);
        const double desiredValue = s_graphEaseDrag.handleValue -
            (valueAxis ? dy * graphDuration / (graphMax.y - graphMin.y) : 0.0);
        const KeyframeModel::Ease newEase = Timeline::easeAtHandle(s_graphEaseDrag.ease,
            keyTime, s_graphEaseDrag.keyValue, desiredTime, desiredValue, duration, out, s_graphSpeed);

        KeyframeModel::Keyframe& key = property.keys[keyIndex];
        std::vector<KeyframeModel::Ease>& side = out ? key.out_ease : key.in_ease;
        const size_t dimensions = property.spatial &&
            (key.value.kind == KeyframeModel::ValueKind::Vec2 ||
             key.value.kind == KeyframeModel::ValueKind::Vec3)
            ? 1 : std::max<size_t>(1, key.value.dims());
        if (side.size() != dimensions) {
            std::vector<KeyframeModel::Ease> initial;
            initial.reserve(dimensions);
            for (size_t d = 0; d < dimensions; ++d)
                initial.push_back(KeyframeModel::side_ease(property.keys, keyIndex, d, property.spatial, out)
                    .value_or(KeyframeModel::Ease{}));
            side = std::move(initial);
        }
        const size_t easeDimension = property.spatial ? 0 : dimension;
        if (easeDimension < side.size()) {
            side[easeDimension] = newEase;
            if (out) key.out_interp = KeyframeModel::Interp::Bezier;
            else key.in_interp = KeyframeModel::Interp::Bezier;
            key.auto_bezier = false;
            if (key.continuous) {
                std::vector<KeyframeModel::Ease>& other = out ? key.in_ease : key.out_ease;
                if (other.size() != side.size()) other = side;
                for (size_t d = 0; d < other.size(); ++d)
                    other[d].speed = side[d].speed;
                key.in_interp = KeyframeModel::Interp::Bezier;
                key.out_interp = KeyframeModel::Interp::Bezier;
            }
            project->dirty = true;
            invalidateFrameCache();
        }
    }
    if (ownsDrag && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        s_graphEaseDrag = {};
}

// Draw a keyframe and start a frame-quantized drag without changing its selection identity.
static bool keyframeMarker(Project::Project* project, Project::Composition& comp,
                           Project::Layer& layer, Props::Property& property,
                           KeyframeModel::Keyframe& key, size_t keyIndex,
                           size_t dimension, ImDrawList* drawList, ImVec2 center,
                           bool graph, bool graphValueEdit,
                           double graphMin, double graphMax, float graphHeight) {
    const bool selected = keyIsSelected(layer.id.v, property.uid, key.time, dimension, graph);
    Icons::paintCentered(drawList, center, graph ? 11.0f : 9.0f,
                         Icons::Icon::Keyframe, selected ? C_BLUE : C_TEXT);
    char id[128];
    snprintf(id, sizeof id, "##key_%llu_%llu_%zu_%zu",
             static_cast<unsigned long long>(layer.id.v),
             static_cast<unsigned long long>(property.uid), keyIndex, dimension);
    ImGui::SetCursorScreenPos({center.x - 6.0f, center.y - 7.0f});
    ImGui::InvisibleButton(id, {12.0f, 14.0f});
    if (ImGui::IsItemClicked()) {
        s_keyMarkerClicked = true;
        s_keyMarqueeActive = false;
        s_selectedLayer = layer.id.v;
        const SelectedKey clicked{layer.id.v, property.uid, key.time, dimension};
        const bool toggle = ImGui::GetIO().KeyShift;
        selectKey(clicked, toggle);
        setCurrentTime(layer.compTime(key.time));
        if (!layer.switches.locked && keyIsSelected(layer.id.v, property.uid, key.time, dimension, graph)) {
            s_keyDrag = {};
            s_keyDrag.layer = layer.id.v;
            s_keyDrag.property = property.uid;
            s_keyDrag.startTime = key.time;
            s_keyDrag.currentTime = key.time;
            s_keyDrag.value = key.value;
            s_keyDrag.dimension = dimension;
            s_keyDrag.startX = ImGui::GetIO().MousePos.x;
            s_keyDrag.startY = ImGui::GetIO().MousePos.y;
            s_keyDrag.graphMin = graphMin;
            s_keyDrag.graphMax = graphMax;
            s_keyDrag.graphHeight = std::max(1.0f, graphHeight);
            s_keyDrag.graph = graphValueEdit;
        }
    }

    if (dimension == 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup(id);
    bool changed = false;
    bool remove = false;
    if (dimension == 0 && ImGui::BeginPopup(id)) {
        ImGui::BeginDisabled(layer.switches.locked);
        if (ImGui::MenuItem(L("Linear"))) {
            key.in_interp = KeyframeModel::Interp::Linear;
            key.out_interp = KeyframeModel::Interp::Linear;
            changed = true;
        }
        if (ImGui::MenuItem(L("Hold"))) {
            key = key.hold();
            changed = true;
        }
        if (ImGui::MenuItem(L("Bezier"))) {
            key.in_interp = KeyframeModel::Interp::Bezier;
            key.out_interp = KeyframeModel::Interp::Bezier;
            changed = true;
        }
        if (ImGui::MenuItem(L("Easy Ease"))) {
            key = key.eased();
            changed = true;
        }
        if (ImGui::MenuItem(L("Delete"))) remove = true;
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    if (remove) {
        Timeline::toggleKeyframe(property, key.time);
        if (!property.is_animated()) layer.syncPropsToTransform();
        s_selectedKeys.erase(std::remove_if(s_selectedKeys.begin(), s_selectedKeys.end(),
            [&](const Timeline::KeyRef& selected) {
                return selected.layer == layer.id.v && selected.property == property.uid &&
                    selected.time == key.time;
            }), s_selectedKeys.end());
        s_selectedKey = s_selectedKeys.empty() ? SelectedKey{} :
            SelectedKey{s_selectedKeys.back().layer, s_selectedKeys.back().property,
                        s_selectedKeys.back().time, 0};
        changed = true;
    }
    if (changed) {
        project->dirty = true;
        invalidateFrameCache();
    }
    return remove;
}

// Apply a dragged key's layer time and, in the value graph, its selected component.
static bool updateKeyDrag(Project::Project* project, Project::Composition& comp,
                          double pixelsPerSecond) {
    if (s_keyDrag.layer == 0) return false;
    if (!ImGui::IsMouseDown(0)) {
        s_keyDrag = {};
        return false;
    }
    if (pixelsPerSecond <= 0.0 || !std::isfinite(pixelsPerSecond)) return false;

    auto layerIt = std::find_if(comp.layers.begin(), comp.layers.end(), [](const Project::Layer& layer) {
        return layer.id.v == s_keyDrag.layer;
    });
    if (layerIt == comp.layers.end()) {
        s_keyDrag = {};
        return false;
    }
    Project::Layer& layer = *layerIt;
    if (layer.switches.locked) {
        s_keyDrag = {};
        return false;
    }
    Props::Property* property = layer.props.find_mut(s_keyDrag.property);
    if (!property) {
        s_keyDrag = {};
        return false;
    }
    auto keyIt = std::find_if(property->keys.begin(), property->keys.end(), [](const KeyframeModel::Keyframe& key) {
        return key.time == s_keyDrag.currentTime;
    });
    if (keyIt == property->keys.end()) {
        s_keyDrag = {};
        return false;
    }

    const float mouseX = ImGui::GetIO().MousePos.x;
    const float mouseY = ImGui::GetIO().MousePos.y;
    const Project::Tick compOffset = comp.frameRate.snap_nearest(
        Project::Tick::from_seconds_f64((mouseX - s_keyDrag.startX) / pixelsPerSecond));
    const Project::Tick targetCompTime = layer.compTime(s_keyDrag.startTime) + compOffset;
    KeyframeModel::Value value = s_keyDrag.value;
    if (s_keyDrag.graph && s_keyDrag.dimension < numericDimensions(value)) {
        const double perPixel = (s_keyDrag.graphMax - s_keyDrag.graphMin) / s_keyDrag.graphHeight;
        const double delta = (s_keyDrag.startY - mouseY) * perPixel;
        setNumericComponent(value, s_keyDrag.dimension,
            numericComponent(s_keyDrag.value, s_keyDrag.dimension) + delta);
    }
    const Timeline::KeyRef grabbed{layer.id.v, property->uid, s_keyDrag.currentTime};
    const Project::Tick delta = targetCompTime - layer.compTime(s_keyDrag.currentTime);
    if (delta.v == 0 && value == keyIt->value) return false;
    Timeline::KeyMoveResult moved = Timeline::moveSelectedKeys(comp, s_selectedKeys, delta, grabbed, value);
    s_selectedKeys = std::move(moved.selected);
    if (moved.grabbed) {
        s_keyDrag.currentTime = moved.grabbed->time;
        s_selectedKey = {moved.grabbed->layer, moved.grabbed->property, moved.grabbed->time,
                         s_keyDrag.dimension};
    } else if (s_selectedKeys.empty()) {
        s_keyDrag = {};
        s_selectedKey = {};
    } else {
        s_selectedKey = {s_selectedKeys.back().layer, s_selectedKeys.back().property,
                         s_selectedKeys.back().time, 0};
    }
    project->dirty = true;
    invalidateFrameCache();
    return true;
}

// Apply the Atelier ImGui style (call once).
// after EffectCraft crates/ui-egui/src/theme.rs
static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 0;  s.FrameRounding   = 2;  s.GrabRounding    = 2;
    s.PopupRounding     = 2;  s.ScrollbarRounding = 2; s.TabRounding     = 0;
    s.WindowBorderSize  = 0;  s.FrameBorderSize  = 0;  s.PopupBorderSize = 1;
    s.WindowPadding     = {6, 5};
    s.FramePadding      = {6, 3};
    s.ItemSpacing       = {5, 4};
    s.ScrollbarSize     = 10;
    s.TabBarBorderSize  = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]             = col4(C_TEXT);
    c[ImGuiCol_TextDisabled]     = col4(C_DIM);
    c[ImGuiCol_WindowBg]         = col4(C_PANEL);
    c[ImGuiCol_ChildBg]          = col4(C_PANEL);
    c[ImGuiCol_PopupBg]          = col4(IM_COL32(0x1e,0x1e,0x1e,255));
    c[ImGuiCol_Border]           = col4(C_LINE);
    c[ImGuiCol_Separator]        = col4(C_LINE);
    c[ImGuiCol_FrameBg]          = col4(C_WELL);
    c[ImGuiCol_FrameBgHovered]   = col4(IM_COL32(0x2c,0x2c,0x2c,255));
    c[ImGuiCol_FrameBgActive]    = col4(IM_COL32(0x30,0x30,0x30,255));
    c[ImGuiCol_TitleBg]          = col4(C_APP_BG);
    c[ImGuiCol_TitleBgActive]    = col4(C_APP_BG);
    c[ImGuiCol_MenuBarBg]        = col4(C_APP_BG);
    c[ImGuiCol_ScrollbarBg]      = col4(C_PANEL);
    c[ImGuiCol_ScrollbarGrab]    = col4(C_LINE);
    c[ImGuiCol_ScrollbarGrabHovered] = col4(IM_COL32(0x48,0x48,0x48,255));
    c[ImGuiCol_CheckMark]        = col4(C_BLUE);
    c[ImGuiCol_SliderGrab]       = col4(C_BLUE);
    c[ImGuiCol_SliderGrabActive] = col4(C_DEEP);
    c[ImGuiCol_Button]           = col4(IM_COL32(0x30,0x30,0x30,255));
    c[ImGuiCol_ButtonHovered]    = col4(C_DEEP);
    c[ImGuiCol_ButtonActive]     = col4(C_BLUE);
    c[ImGuiCol_Header]           = col4(IM_COL32(0x30,0x30,0x30,255));
    c[ImGuiCol_HeaderHovered]    = col4(C_DEEP);
    c[ImGuiCol_HeaderActive]     = col4(C_BLUE);
    c[ImGuiCol_Tab]              = col4(C_TAB_BG);
    c[ImGuiCol_TabActive]        = col4(C_TAB_ACT);
    c[ImGuiCol_TabHovered]       = col4(C_DEEP);
    c[ImGuiCol_TabUnfocused]     = col4(C_TAB_BG);
    c[ImGuiCol_TabUnfocusedActive]= col4(C_TAB_ACT);
    c[ImGuiCol_ModalWindowDimBg] = {0,0,0,0.45f};
}

void init() {
    applyStyle();
}

// ---------- low-level drawing helpers ----------

static void hLine(ImDrawList* dl, float x0, float x1, float y) {
    dl->AddLine({x0,y}, {x1,y}, C_LINE, DIVIDER);
}
static void vLine(ImDrawList* dl, float x, float y0, float y1) {
    dl->AddLine({x,y0}, {x,y1}, C_LINE, DIVIDER);
}

// Draw a tab strip for `n` tabs; returns the index of the active one after interaction.
// after EffectCraft crates/ui-egui/src/dock.rs TAB_STRIP / craft-ui tabs
static int tabStrip(const char* tabs[], int n, int active,
                    float x, float y, float w, float rowH,
                    ImDrawList* dl, ImGuiIO& io) {
    dl->AddRectFilled({x,y},{x+w, y+rowH}, C_TAB_BG);
    hLine(dl, x, x+w, y+rowH);

    const float tabW  = std::max(60.0f, w / n);
    int result = active;
    for (int i = 0; i < n; ++i) {
        const float tx = x + i * tabW;
        const float ty = y;
        const float tw = (i == n-1) ? (x+w-tx) : tabW;
        const bool on = (i == active);
        const ImVec2 tMin{tx, ty}, tMax{tx+tw, ty+rowH};
        dl->AddRectFilled(tMin, tMax, on ? C_TAB_ACT : C_TAB_BG);
        if (on) {
            dl->AddLine({tx,ty},{tx+tw,ty}, C_BLUE, 2.0f);
        }
        ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
        const float lx = tx + (tw-ts.x)*0.5f;
        const float ly = ty + (rowH-ts.y)*0.5f;
        dl->AddText({lx,ly}, on ? C_TEXT : C_DIM, tabs[i]);
        if (ImGui::IsMouseClicked(0)) {
            const ImVec2& mp = io.MousePos;
            if (mp.x >= tMin.x && mp.x < tMax.x && mp.y >= tMin.y && mp.y < tMax.y)
                result = i;
        }
        if (i < n-1)
            vLine(dl, tx+tw, ty+2, ty+rowH-2);
    }
    return result;
}

// Panel hamburger menu button in the tab strip corner
// after EffectCraft crates/ui-egui/src/dock_ui.rs tab_menu_button
static void panelMenuButton(float rx, float cy, ImDrawList* dl) {
    Icons::paintCentered(dl, {rx - 8.0f, cy}, 12.0f, Icons::Icon::Hamburger, C_DIM);
}

// ---------- panel content helpers ----------

// after EffectCraft crates/ui-egui/src/panels/project.rs
static void drawProjectPanel(Project::Project* proj) {
    if (!proj) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        ImGui::TextDisabled("%s", L("Select an item to see its details"));
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW  = ImGui::GetContentRegionAvail().x;
    const float avH  = ImGui::GetContentRegionAvail().y;

    // --- Thumbnail / details header (~74 px) ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs details_header
    const float headH = 74.0f;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p0, {p0.x + avW, p0.y + headH}, C_APP_BG);
    if (!proj->compositions.empty() && proj->activeComp >= 0) {
        const auto& comp = proj->compositions[(size_t)proj->activeComp];
        // Thumbnail placeholder rectangle with Comp icon
        const ImVec2 thMin{p0.x + 8.0f, p0.y + 10.0f};
        const ImVec2 thMax{p0.x + 100.0f, p0.y + 64.0f};
        dl->AddRectFilled(thMin, thMax, C_WELL, 2.0f);
        dl->AddRect(thMin, thMax, C_LINE, 2.0f);
        Icons::paintCentered(dl, {(thMin.x + thMax.x)*0.5f, (thMin.y + thMax.y)*0.5f}, 24.0f, Icons::Icon::Comp, C_DIM);

        // Details text
        dl->AddText({p0.x + 108.0f, p0.y + 10.0f}, C_TEXT, comp.name.c_str());
        char dims[64]; snprintf(dims, sizeof dims, "%d x %d (1.00)", comp.width, comp.height);
        dl->AddText({p0.x + 108.0f, p0.y + 26.0f}, C_DIM, dims);
        char fps[64]; snprintf(fps, sizeof fps, "%.3g fps", comp.fps());
        dl->AddText({p0.x + 108.0f, p0.y + 40.0f}, C_DIM, fps);
        const Project::Tick dur = comp.duration;
        const Project::FrameRate fr = comp.frameRate;
        std::string tc = Project::format_timecode_ae(fr.frame_at(dur), fr, false);
        char durBuf[64]; snprintf(durBuf, sizeof durBuf, "\xce\x94 %s", tc.c_str());
        dl->AddText({p0.x + 108.0f, p0.y + 54.0f}, C_DIM, durBuf);
    } else {
        ImVec2 hs = ImGui::CalcTextSize(L("Select an item to see its details"));
        dl->AddText({p0.x + (avW - hs.x) * 0.5f, p0.y + (headH - hs.y) * 0.5f}, C_FAINT, L("Select an item to see its details"));
    }
    ImGui::Dummy({avW, headH});
    hLine(dl, p0.x, p0.x + avW, p0.y + headH);

    // --- Search bar with Search icon ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs search bar
    static char s_search[128] = {};
    const ImVec2 sp0 = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(sp0, {sp0.x + avW, sp0.y + 26.0f}, C_WELL);
    Icons::paintCentered(dl, {sp0.x + 14.0f, sp0.y + 13.0f}, 14.0f, Icons::Icon::Search, C_DIM);
    ImGui::SetCursorScreenPos({sp0.x + 28.0f, sp0.y + 2.0f});
    ImGui::SetNextItemWidth(avW - 36.0f);
    ImGui::InputTextWithHint("##projsearch", L("Search project"), s_search, sizeof s_search);
    ImGui::SetCursorScreenPos({sp0.x, sp0.y + 26.0f});
    hLine(dl, sp0.x, sp0.x + avW, sp0.y + 26.0f);

    // --- Column header: Name | Label diamond | Type | Frame Rate ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs draw_header / COLUMNS
    const float hdrH = 20.0f;
    const ImVec2 hdrP = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(hdrP, {hdrP.x + avW, hdrP.y + hdrH}, C_APP_BG);
    dl->AddText({hdrP.x + 28.0f, hdrP.y + 3.0f}, C_DIM, L("Name"));
    // Label diamond icon
    Icons::paintCentered(dl, {hdrP.x + avW * 0.48f, hdrP.y + hdrH * 0.5f}, 12.0f, Icons::Icon::Keyframe, C_DIM);
    dl->AddText({hdrP.x + avW * 0.55f, hdrP.y + 3.0f}, C_DIM, L("Type"));
    dl->AddText({hdrP.x + avW * 0.78f, hdrP.y + 3.0f}, C_DIM, L("Frame Rate"));
    ImGui::Dummy({avW, hdrH});
    hLine(dl, hdrP.x, hdrP.x + avW, hdrP.y + hdrH);

    // --- Item list ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs visible_rows
    const float botBarH = 28.0f;
    const float listH = avH - headH - 26.0f - hdrH - botBarH - 4.0f;
    ImGui::BeginChild("##projlist", {avW, listH}, false, ImGuiWindowFlags_NoScrollbar);
    for (int i = 0; i < (int)proj->compositions.size(); ++i) {
        const auto& comp = proj->compositions[(size_t)i];
        const bool active = (i == proj->activeComp);
        ImGui::PushID(i);
        const ImVec2 rp = ImGui::GetCursorScreenPos();
        // Label color swatch
        const ImU32 swCol = labelColor(i % 17 + (i > 0 ? 1 : 0));
        dl->AddRectFilled({rp.x + 4.0f, rp.y + 4.0f}, {rp.x + 14.0f, rp.y + ROW_H - 4.0f}, swCol, 1.0f);
        // Comp icon
        Icons::paintCentered(dl, {rp.x + 24.0f, rp.y + ROW_H * 0.5f}, 14.0f, Icons::Icon::Comp, active ? C_BLUE : C_DIM);
        // Selectable item name
        ImGui::SetCursorScreenPos({rp.x + 36.0f, rp.y});
        char rowBuf[256]; snprintf(rowBuf, sizeof rowBuf, "%s###item%d", comp.name.c_str(), i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Text, col4(C_BLUE));
        if (ImGui::Selectable(rowBuf, active, 0, {avW * 0.48f - 38.0f, ROW_H})) {
            if (g_actions && g_actions->onSelectComp) g_actions->onSelectComp(i);
        }
        if (active) ImGui::PopStyleColor();
        // Type column
        dl->AddText({rp.x + avW * 0.55f, rp.y + 2.0f}, C_DIM, L("Composition"));
        // Frame rate column
        char fpsBuf[32]; snprintf(fpsBuf, sizeof fpsBuf, "%.3g", comp.fps());
        dl->AddText({rp.x + avW * 0.78f, rp.y + 2.0f}, C_DIM, fpsBuf);

        ImGui::SetCursorScreenPos({rp.x, rp.y + ROW_H});
        ImGui::PopID();
    }
    ImGui::EndChild();

    // --- Bottom bar: Interpret | New Folder | New Comp | bpc | Delete ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs bottom bar buttons
    const ImVec2 bp0 = ImGui::GetCursorScreenPos();
    hLine(dl, bp0.x, bp0.x + avW, bp0.y);
    dl->AddRectFilled({bp0.x, bp0.y + 1}, {bp0.x + avW, bp0.y + botBarH}, C_APP_BG);

    ImGui::SetCursorScreenPos({bp0.x + 6.0f, bp0.y + 3.0f});
    if (Icons::iconButton("##p_interpret", Icons::Icon::Link, false, {22, 22})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Interpret Footage"));
    ImGui::SameLine(0, 4);
    if (Icons::iconButton("##p_newfolder", Icons::Icon::NewFolder, false, {22, 22})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("New Folder"));
    ImGui::SameLine(0, 4);
    if (Icons::iconButton("##p_newcomp", Icons::Icon::NewComp, false, {22, 22})) {
        if (g_actions && g_actions->onNew) g_actions->onNew();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("New Composition"));
    ImGui::SameLine(0, 10);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
    ImGui::TextDisabled("8 bpc");
    ImGui::SameLine(avW - 28.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
    if (Icons::iconButton("##p_delete", Icons::Icon::Trash, false, {22, 22})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Delete"));
}

// after EffectCraft crates/ui-egui/src/panels/effect_controls.rs
static void drawEffectControlsPanel(Project::Project* /*proj*/) {
    ImGui::TextDisabled("%s", L("No layer selected."));
}

// after EffectCraft crates/ui-egui/src/panels/viewer.rs / viewers.rs
static void drawCompositionViewer(Project::Project* proj) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();

    const float barH = 26.0f;
    const float canvasH = avail.y - barH - 1.0f;

    // Checkerboard transparency background
    // after EffectCraft crates/ui-egui/src/panels/viewer.rs transparency grid
    const float cs = 16.0f;
    for (float y = 0; y < canvasH; y += cs) {
        for (float x = 0; x < avail.x; x += cs) {
            const bool odd = (((int)(x / cs)) + ((int)(y / cs))) & 1;
            dl->AddRectFilled(
                {p.x + x, p.y + y},
                {p.x + std::min(x + cs, avail.x), p.y + std::min(y + cs, canvasH)},
                odd ? IM_COL32(0x28, 0x28, 0x28, 255) : IM_COL32(0x1e, 0x1e, 0x1e, 255));
        }
    }

    static GLuint s_compTexture = 0;
    static uint64_t s_textureComp = 0;
    static int64_t s_textureFrame = std::numeric_limits<int64_t>::min();
    static uint64_t s_textureGeneration = 0;

    // Centered composition canvas frame (matches comp aspect ratio or 16:9)
    float aspect = 16.0f / 9.0f;
    if (proj && proj->activeComp >= 0 && (size_t)proj->activeComp < proj->compositions.size()) {
        const auto& c = proj->compositions[(size_t)proj->activeComp];
        if (c.width > 0 && c.height > 0) aspect = (float)c.width / (float)c.height;
    }
    float cw = avail.x * 0.75f;
    float ch = cw / aspect;
    if (ch > canvasH * 0.8f) {
        ch = canvasH * 0.8f;
        cw = ch * aspect;
    }
    const ImVec2 cMin{ p.x + (avail.x - cw) * 0.5f, p.y + (canvasH - ch) * 0.5f };
    const ImVec2 cMax{ cMin.x + cw, cMin.y + ch };

    bool has_rendered = false;
    if (proj && proj->activeComp >= 0 && (size_t)proj->activeComp < proj->compositions.size()) {
        const auto& comp = proj->compositions[(size_t)proj->activeComp];
        if (!comp.layers.empty()) {
            const int64_t frameNumber = comp.frameRate.frame_at(s_currentTime);
            const Raster::Image* frame = cachedFrame(proj, comp);
            if (frame && !frame->is_empty()) {
                if (s_textureComp != comp.id.v || s_textureFrame != frameNumber ||
                    s_textureGeneration != s_cacheGeneration) {
                    std::vector<uint8_t> rgba = frame->to_rgba8();
                    if (s_compTexture == 0) {
                        glGenTextures(1, &s_compTexture);
                    }
                    glBindTexture(GL_TEXTURE_2D, s_compTexture);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)frame->width, (GLsizei)frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                    s_textureComp = comp.id.v;
                    s_textureFrame = frameNumber;
                    s_textureGeneration = s_cacheGeneration;
                }
                has_rendered = true;
            }
        }
    }

    if (has_rendered && s_compTexture != 0) {
        dl->AddImage((ImTextureID)(intptr_t)s_compTexture, cMin, cMax);
        dl->AddRect(cMin, cMax, C_LINE, 1.0f);
    } else {
        dl->AddRectFilled(cMin, cMax, IM_COL32(0x12, 0x12, 0x12, 255));
        dl->AddRect(cMin, cMax, C_LINE, 1.0f);
        // Centered comp name in canvas
        const char* label = (proj && proj->activeComp >= 0)
            ? proj->compositions[(size_t)proj->activeComp].name.c_str()
            : L("No composition");
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText({cMin.x + (cw - ts.x) * 0.5f, cMin.y + (ch - ts.y) * 0.5f}, C_DIM, label);
    }

    ImGui::Dummy({avail.x, canvasH});
    hLine(dl, p.x, p.x + avail.x, p.y + canvasH);

    // --- Bottom bar: zoom / Fit, grid, timecode, snapshots, channels, resolution, ROI, checker, fast previews ---
    // after EffectCraft crates/ui-egui/src/panels/viewer.rs bottom bar
    const ImVec2 bb = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(bb, {bb.x + avail.x, bb.y + barH}, C_APP_BG);

    ImGui::SetCursorScreenPos({bb.x + 4.0f, bb.y + 2.0f});
    // Fit / Zoom
    if (ImGui::SmallButton(L("Fit (100%)"))) {}
    ImGui::SameLine(0, 4);
    Icons::paintCentered(dl, {ImGui::GetCursorScreenPos().x + 6.0f, bb.y + barH * 0.5f}, 10.0f, Icons::Icon::ChevronDown, C_DIM);
    ImGui::Dummy({14, barH - 4.0f});

    ImGui::SameLine(0, 4);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 4);
    if (Icons::iconButton("##v_grid", Icons::Icon::Grid, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Grid and Guide Options"));

    ImGui::SameLine(0, 6);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 6);
    // Current timecode in blue
    std::string tcStr = "0:00:00:00";
    if (proj && proj->activeComp >= 0) {
        const auto& comp = proj->compositions[(size_t)proj->activeComp];
        tcStr = Project::format_timecode_ae(0, comp.frameRate, false);
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    ImGui::TextColored(col4(C_BLUE), "%s", tcStr.c_str());

    ImGui::SameLine(0, 6);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 6);
    if (Icons::iconButton("##v_snap", Icons::Icon::Snapshot, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Take Snapshot"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##v_showsnap", Icons::Icon::ShowSnapshot, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Show Snapshot"));

    ImGui::SameLine(0, 6);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 6);
    // Channel picker icon + "RGB"
    Icons::paintCentered(dl, {ImGui::GetCursorScreenPos().x + 8.0f, bb.y + barH * 0.5f}, 14.0f, Icons::Icon::Channel, C_TEXT);
    ImGui::Dummy({16, barH - 4.0f});
    ImGui::SameLine(0, 2);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    ImGui::TextDisabled("RGB");

    ImGui::SameLine(0, 6);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 6);
    // Resolution
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    ImGui::TextDisabled("%s", L("Auto (Full)"));
    ImGui::SameLine(0, 2);
    Icons::paintCentered(dl, {ImGui::GetCursorScreenPos().x + 6.0f, bb.y + barH * 0.5f}, 10.0f, Icons::Icon::ChevronDown, C_DIM);
    ImGui::Dummy({12, barH - 4.0f});

    ImGui::SameLine(0, 6);
    vLine(dl, ImGui::GetCursorScreenPos().x, bb.y + 4.0f, bb.y + barH - 4.0f);
    ImGui::Dummy({4, barH - 4.0f});

    ImGui::SameLine(0, 6);
    if (Icons::iconButton("##v_roi", Icons::Icon::Region, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Region of Interest"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##v_checker", Icons::Icon::Checker, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Toggle Transparency Grid"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##v_draft3d", Icons::Icon::Draft3D, false, {20, 20})) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Fast Previews"));
}

// after EffectCraft crates/engine/src/cmd/layer/contents.rs add_shape_item
static bool appendShapeItem(Project::Project* project, Project::Layer& layer,
                            const std::string& kind, uint64_t parentUid) {
    if (layer.kind != Project::LayerKind::Shape) return false;
    Props::PropGroup* contents = layer.props.group_mut("contents");
    if (!contents) return false;
    Props::Ids ids(&project->nextId);
    const std::optional<Props::Uid> added =
        Props::Build::addShapeItem(ids, *contents, kind, parentUid);
    if (!added) return false;
    if (kind == "group") {
        s_openGroups.insert(*added);
        s_selectedShapeGroup = *added;
    }
    s_selectedShapeLayer = layer.id.v;
    project->dirty = true;
    invalidateFrameCache();
    return true;
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs shape_add_popup
struct PendingShapeItem {
    uint64_t layer = 0;
    uint64_t parent = 0;
    std::string kind;
};

static void drawShapeAddMenu(Project::Project* project, Project::Layer& layer,
                             Props::PropGroup& contents,
                             std::optional<PendingShapeItem>& pending) {
    if (!project || layer.kind != Project::LayerKind::Shape || contents.match_id != "contents") return;
    const bool addButton = Icons::iconButton("##shape-add", Icons::Icon::ChevronRight, false, {18, 17});
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Add a shape, fill, stroke or path"));
    if (addButton) ImGui::OpenPopup("##shape-add-menu");
    if (!ImGui::BeginPopup("##shape-add-menu")) return;

    const struct { const char* kind; const char* label; bool separator; } items[] = {
        {"group", "Group (empty)", false},
        {"", "", true},
        {"rect", "Rectangle", false},
        {"ellipse", "Ellipse", false},
        {"path", "Path", false},
        {"", "", true},
        {"fill", "Fill", false},
        {"stroke", "Stroke", false},
        {"", "", true},
        {"repeater", "Repeater", false},
        {"round", "Round Corners", false},
        {"trim", "Trim Paths", false},
        {"twist", "Twist", false},
        {"zigzag", "Zig Zag", false},
        {"pucker", "Pucker & Bloat", false},
    };
    for (const auto& item : items) {
        if (item.separator) {
            ImGui::Separator();
        } else if (ImGui::MenuItem(item.label)) {
            const uint64_t parentUid = s_selectedShapeLayer == layer.id.v
                ? s_selectedShapeGroup : 0;
            pending = PendingShapeItem{layer.id.v, parentUid, item.kind};
            ImGui::CloseCurrentPopup();
            break;
        }
    }
    ImGui::EndPopup();
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs
static void drawTimeline(Project::Project* proj) {
    if (!proj || proj->activeComp < 0) {
        ImGui::TextDisabled("%s", L("No composition active."));
        return;
    }
    auto& comp = proj->compositions[(size_t)proj->activeComp];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW = ImGui::GetContentRegionAvail().x;
    const float avH = ImGui::GetContentRegionAvail().y;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    struct Row {
        enum class Kind { Layer, Group, Property } kind;
        size_t layer;
        Props::PropGroup* group = nullptr;
        Props::Property* property = nullptr;
        int depth = 0;
    };
    std::vector<Row> rows;
    std::function<void(size_t, Props::PropGroup&, int)> appendGroup;
    std::function<void(size_t, Props::PropGroup&, int)> appendChildren;
    appendGroup = [&](size_t layerIndex, Props::PropGroup& group, int depth) {
        rows.push_back({Row::Kind::Group, layerIndex, &group, nullptr, depth});
        const bool open = group.name == "Transform" || s_openGroups.count(group.uid) != 0;
        if (!open) return;
        appendChildren(layerIndex, group, depth + 1);
    };
    appendChildren = [&](size_t layerIndex, Props::PropGroup& group, int depth) {
        for (auto& node : group.children) {
            if (auto* property = node.as_prop_mut()) {
                if (property->ui.kind == Props::ParamUiKind::Hidden) continue;
                if (group.match_id == "rect" &&
                    (property->match_id == "topLeftRoundness" ||
                     property->match_id == "topRightRoundness" ||
                     property->match_id == "bottomRightRoundness" ||
                     property->match_id == "bottomLeftRoundness")) {
                    const Props::Property* perCorner = group.prop("perCorner");
                    if (!perCorner || !perCorner->value_at(comp.layers[layerIndex].layerTime(s_currentTime)).as_bool())
                        continue;
                }
                rows.push_back({Row::Kind::Property, layerIndex, nullptr, property, depth});
            } else if (auto* child = node.as_group_mut()) {
                if (group.match_id == "group" && child->match_id == "contents")
                    appendChildren(layerIndex, *child, depth);
                else
                    appendGroup(layerIndex, *child, depth);
            }
        }
    };
    for (size_t i = 0; i < comp.layers.size(); ++i) {
        auto& layer = comp.layers[i];
        if (comp.hideShy && layer.switches.shy) continue;
        rows.push_back({Row::Kind::Layer, i});
        if (s_openLayers.count(layer.id.v) == 0) continue;
        for (auto& node : layer.props.children) {
            if (auto* property = node.as_prop_mut()) {
                rows.push_back({Row::Kind::Property, i, nullptr, property, 1});
            } else if (auto* group = node.as_group_mut()) {
                appendGroup(i, *group, 1);
            }
        }
    }

    // after EffectCraft crates/ui-egui/src/panels/timeline.rs header_timecode
    const float hdrH = 28.0f;
    dl->AddRectFilled(p0, {p0.x + avW, p0.y + hdrH}, C_APP_BG);

    const Project::FrameRate fr = comp.frameRate;
    const int64_t frameNow = fr.frame_at(s_currentTime);
    std::string tc = Project::format_timecode_ae(frameNow, fr, false);
    ImGui::SetCursorScreenPos({p0.x + 8.0f, p0.y + 5.0f});
    ImGui::TextColored(col4(C_BLUE), "%s", tc.c_str());
    ImGui::SameLine(0, 4);
    char fpsBuf[64]; snprintf(fpsBuf, sizeof fpsBuf, "(%05lld / %s fps)",
                               (long long)frameNow, fr.label().c_str());
    ImGui::TextDisabled("%s", fpsBuf);

    ImGui::SameLine(avW * 0.38f - 130.0f);
    static char s_tlsearch[64] = {};
    Icons::paintCentered(dl, {ImGui::GetCursorScreenPos().x + 8.0f, p0.y + hdrH * 0.5f}, 12.0f, Icons::Icon::Search, C_DIM);
    ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x + 18.0f, p0.y + 4.0f});
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputTextWithHint("##tlsearch", L("Search"), s_tlsearch, sizeof s_tlsearch);

    ImGui::SameLine(avW - 104.0f);
    ImGui::SetCursorScreenPos({p0.x + avW - 104.0f, p0.y + 3.0f});
    if (Icons::iconButton("##tl_shy", Icons::Icon::Shy, comp.hideShy, {22, 22})) {
        comp.hideShy = !comp.hideShy;
        proj->dirty = true;
        invalidateFrameCache();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Hide all layers for which the Shy switch is set"));
    if (Icons::iconButton("##tl_fb", Icons::Icon::FrameBlend, comp.enableFrameBlending, {22, 22})) {
        comp.enableFrameBlending = !comp.enableFrameBlending;
        proj->dirty = true;
        invalidateFrameCache();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Enable Frame Blending"));
    if (Icons::iconButton("##tl_mb", Icons::Icon::MotionBlur, comp.enableMotionBlur, {22, 22})) {
        comp.enableMotionBlur = !comp.enableMotionBlur;
        proj->dirty = true;
        invalidateFrameCache();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Enable Motion Blur"));
    if (Icons::iconButton("##tl_graph", Icons::Icon::Graph, s_graphEditor, {22, 22})) {
        s_graphEditor = !s_graphEditor;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Graph Editor (Shift+F3)"));

    hLine(dl, p0.x, p0.x + avW, p0.y + hdrH);

    const float colHdrH = 20.0f;
    const ImVec2 chp = {p0.x, p0.y + hdrH};
    dl->AddRectFilled(chp, {chp.x + avW, chp.y + colHdrH}, C_APP_BG);

    Icons::paintCentered(dl, {chp.x + 10.0f, chp.y + colHdrH * 0.5f}, 12.0f, Icons::Icon::Eye, C_DIM);
    Icons::paintCentered(dl, {chp.x + 24.0f, chp.y + colHdrH * 0.5f}, 12.0f, Icons::Icon::Speaker, C_DIM);
    Icons::paintCentered(dl, {chp.x + 38.0f, chp.y + colHdrH * 0.5f}, 12.0f, Icons::Icon::Solo, C_DIM);
    Icons::paintCentered(dl, {chp.x + 52.0f, chp.y + colHdrH * 0.5f}, 12.0f, Icons::Icon::Lock, C_DIM);
    Icons::paintCentered(dl, {chp.x + 68.0f, chp.y + colHdrH * 0.5f}, 10.0f, Icons::Icon::Keyframe, C_DIM);
    dl->AddText({chp.x + 80.0f, chp.y + 3.0f}, C_DIM, "#");
    dl->AddText({chp.x + 100.0f, chp.y + 3.0f}, C_DIM, L("Layer Name"));

    const float swBaseX = avW * 0.38f - 110.0f;
    Icons::paintCentered(dl, {chp.x + swBaseX, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Shy, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 13.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Collapse, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 26.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Quality, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 39.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Fx, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 52.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::FrameBlend, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 65.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::MotionBlur, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 78.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Adjustment, C_DIM);
    Icons::paintCentered(dl, {chp.x + swBaseX + 91.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::Cube, C_DIM);

    dl->AddText({chp.x + swBaseX + 108.0f, chp.y + 3.0f}, C_DIM, L("Parent & Link"));
    Icons::paintCentered(dl, {chp.x + swBaseX + 175.0f, chp.y + colHdrH * 0.5f}, 11.0f, Icons::Icon::PickWhip, C_DIM);

    hLine(dl, chp.x, chp.x + avW, chp.y + colHdrH);

    const float botBarH = 22.0f;
    const float layerAreaH = std::max(1.0f, avH - hdrH - colHdrH - botBarH - 2.0f);
    const float rulerH = 24.0f;
    const float leftSideW = avW * 0.38f;
    const float rightSideW = avW - leftSideW;
    const float rowTop = p0.y + hdrH + colHdrH;
    const float barAreaX = p0.x + leftSideW + 1.0f;
    const float barAreaW = std::max(1.0f, rightSideW - 1.0f);
    const double duration = std::max(0.01, comp.durationSeconds());
    const double fitPps = Timeline::fitPps(barAreaW, duration);
    const double pps = s_timelinePps > 0.0 ? std::max(fitPps, s_timelinePps) : fitPps;
    const double visible = barAreaW / pps;
    if (s_playing && s_timelinePps > 0.0)
        s_timelineStart = Timeline::pageToCti(s_timelineStart, visible, duration, s_currentTime.seconds());
    s_timelineStart = std::clamp(s_timelineStart, 0.0, std::max(0.0, duration - visible));

    // Layer outline with folded property rows.
    ImGui::SetCursorScreenPos({p0.x, rowTop});
    ImGui::SetNextWindowScroll({0.0f, s_timelineScroll});
    ImGui::BeginChild("##tlayers", {leftSideW, layerAreaH}, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rulerH);
    bool resizeCursor = false;
    std::optional<PendingShapeItem> pendingShapeItem;
    s_layerRowHits.clear();
    for (size_t ri = 0; ri < rows.size(); ++ri) {
        const Row& row = rows[ri];
        Project::Layer& layer = comp.layers[row.layer];
        const ImVec2 rp = ImGui::GetCursorScreenPos();
        ImDrawList* rowDl = ImGui::GetWindowDrawList();
        if (row.kind == Row::Kind::Layer) {
            ImGui::PushID((int)layer.id.v);
            const bool isSelected = s_selectedLayers.count(layer.id.v) != 0;
            s_layerRowHits.push_back({rp, {rp.x + leftSideW, rp.y + ROW_H}, layer.id.v, row.layer});
            if (isSelected)
                rowDl->AddRectFilled(rp, {rp.x + leftSideW, rp.y + ROW_H}, C_DEEP);
            ImGui::SetCursorScreenPos(rp);
            ImGui::InvisibleButton("##layer", {leftSideW, ROW_H});
            if (ImGui::IsItemClicked()) {
                const bool shift = ImGui::GetIO().KeyShift;
                const bool toggle = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
                if (shift && s_selectionAnchor != 0) {
                    auto anchor = std::find_if(comp.layers.begin(), comp.layers.end(), [](const Project::Layer& candidate) {
                        return candidate.id.v == s_selectionAnchor;
                    });
                    const size_t anchorIndex = anchor == comp.layers.end() ? row.layer :
                        static_cast<size_t>(anchor - comp.layers.begin());
                    const size_t first = std::min(anchorIndex, row.layer);
                    const size_t last = std::max(anchorIndex, row.layer);
                    s_selectedLayers.clear();
                    for (size_t index = first; index <= last; ++index) {
                        const auto& candidate = comp.layers[index];
                        if (!candidate.switches.locked && (!comp.hideShy || !candidate.switches.shy))
                            s_selectedLayers.insert(candidate.id.v);
                    }
                } else if (toggle) {
                    if (isSelected) s_selectedLayers.erase(layer.id.v);
                    else s_selectedLayers.insert(layer.id.v);
                    s_selectionAnchor = layer.id.v;
                } else {
                    s_selectedLayers.clear();
                    s_selectedLayers.insert(layer.id.v);
                    s_selectionAnchor = layer.id.v;
                }
                s_selectedLayer = layer.id.v;
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f) &&
                !layer.switches.locked && !s_layerReorderActive) {
                s_layerReorderActive = true;
                s_layerReorderIds.clear();
                if (s_selectedLayers.count(layer.id.v) != 0) {
                    for (const auto& candidate : comp.layers)
                        if (s_selectedLayers.count(candidate.id.v) != 0 && !candidate.switches.locked)
                            s_layerReorderIds.push_back(candidate.id.v);
                } else {
                    s_layerReorderIds.push_back(layer.id.v);
                }
            }
            const ImVec2 center{rp.x + 11.0f, rp.y + ROW_H * 0.5f};
            const bool expanded = s_openLayers.count(layer.id.v) != 0;
            Icons::paintCentered(rowDl, center, 10.0f, expanded ? Icons::Icon::ChevronDown : Icons::Icon::ChevronRight, C_DIM);
            if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect({rp.x + 4.0f, rp.y}, {rp.x + 18.0f, rp.y + ROW_H})) {
                if (expanded) s_openLayers.erase(layer.id.v);
                else s_openLayers.insert(layer.id.v);
            }
            Icons::paintCentered(rowDl, {rp.x + 28.0f, center.y}, 11.0f,
                                 Icons::Icon::Eye,
                                 layer.switches.video ? C_TEXT : C_FAINT);
            Icons::paintCentered(rowDl, {rp.x + 43.0f, center.y}, 11.0f,
                                 layer.switches.locked ? Icons::Icon::Lock : Icons::Icon::LockOpen,
                                 layer.switches.locked ? C_TEXT : C_FAINT);
            rowDl->AddRectFilled({rp.x + 52.0f, rp.y + 4.0f}, {rp.x + 62.0f, rp.y + ROW_H - 4.0f},
                                 labelColor(static_cast<int>(layer.label)), 1.0f);
            char num[8];
            snprintf(num, sizeof num, "%d", (int)row.layer + 1);
            rowDl->AddText({rp.x + 66.0f, rp.y + 2.0f}, C_DIM, num);
            Icons::Icon typeIcon = Icons::Icon::Solid;
            switch (layer.kind) {
            case Project::LayerKind::Text: typeIcon = Icons::Icon::TextLayer; break;
            case Project::LayerKind::Shape: typeIcon = Icons::Icon::ShapeLayer; break;
            case Project::LayerKind::Null: typeIcon = Icons::Icon::Null; break;
            case Project::LayerKind::Camera: typeIcon = Icons::Icon::Camera; break;
            case Project::LayerKind::Light: typeIcon = Icons::Icon::Light; break;
            case Project::LayerKind::Footage: typeIcon = Icons::Icon::Footage; break;
            case Project::LayerKind::Composition: typeIcon = Icons::Icon::Comp; break;
            case Project::LayerKind::Image: typeIcon = Icons::Icon::Image; break;
            default: break;
            }
            Icons::paintCentered(rowDl, {rp.x + 86.0f, center.y}, 11.0f, typeIcon, C_DIM);
            rowDl->AddText({rp.x + 96.0f, rp.y + 2.0f}, isSelected ? C_TEXT : C_DIM, layer.name.c_str());
            ImGui::PopID();
        } else {
            ImGui::PushID((int)(row.group ? row.group->uid : row.property->uid));
            const float indent = 14.0f * (float)row.depth;
            if (row.kind == Row::Kind::Group) {
                const bool open = row.group->name == "Transform" || s_openGroups.count(row.group->uid) != 0;
                const Props::PropGroup* parent = layer.props.parent_of(row.group->uid);
                const bool shapeItem = layer.kind == Project::LayerKind::Shape &&
                    parent && parent->match_id == "contents";
                const bool contents = layer.kind == Project::LayerKind::Shape &&
                    row.group->match_id == "contents";
                Icons::paintCentered(rowDl, {rp.x + indent + 10.0f, rp.y + ROW_H * 0.5f}, 9.0f,
                                     open ? Icons::Icon::ChevronDown : Icons::Icon::ChevronRight, C_DIM);
                rowDl->AddText({rp.x + indent + 18.0f, rp.y + 2.0f},
                               row.group->enabled ? C_DIM : C_FAINT, row.group->name.c_str());
                ImGui::SetCursorScreenPos({rp.x + indent, rp.y});
                const float reserved = contents ? 26.0f : shapeItem ? 24.0f : 2.0f;
                ImGui::InvisibleButton("##group", {leftSideW - indent - reserved, ROW_H});
                if (ImGui::IsItemClicked()) {
                    if (row.group->name != "Transform") {
                        if (open) s_openGroups.erase(row.group->uid);
                        else s_openGroups.insert(row.group->uid);
                    }
                    if (layer.kind == Project::LayerKind::Shape) {
                        s_selectedShapeLayer = layer.id.v;
                        s_selectedShapeGroup = row.group->match_id == "group" ? row.group->uid : 0;
                    }
                }
                if (shapeItem) {
                    ImGui::SetCursorScreenPos({rp.x + leftSideW - 21.0f, rp.y + 1.0f});
                    if (Icons::iconButton("##shape-visible", Icons::Icon::Eye,
                                          row.group->enabled, {18, 17})) {
                        row.group->enabled = !row.group->enabled;
                        proj->dirty = true;
                        invalidateFrameCache();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",
                        row.group->enabled ? L("Disable") : L("Enable"));
                }
                if (contents) {
                    ImGui::SetCursorScreenPos({rp.x + leftSideW - 43.0f, rp.y + 1.0f});
                    drawShapeAddMenu(proj, layer, *row.group, pendingShapeItem);
                }
            } else {
                Props::Property& property = *row.property;
                const Project::Tick layerTime = layer.layerTime(s_currentTime);
                float nameX = rp.x + indent + 8.0f;
                if (!property.static_only) {
                    ImGui::SetCursorScreenPos({nameX, rp.y + 2.0f});
                    if (Icons::iconButton("##stopwatch", Icons::Icon::Stopwatch,
                                          property.is_animated(), {14, 14})) {
                        property.set_animated(!property.is_animated(), layerTime);
                        if (!property.is_animated()) layer.syncPropsToTransform();
                        proj->dirty = true;
                        invalidateFrameCache();
                    }
                    nameX += 17.0f;
                }
                if (property.is_animated()) {
                    const auto currentKey = std::find_if(property.keys.begin(), property.keys.end(),
                        [layerTime](const KeyframeModel::Keyframe& key) { return key.time == layerTime; });
                    const auto previousKey = std::find_if(property.keys.rbegin(), property.keys.rend(),
                        [layerTime](const KeyframeModel::Keyframe& key) { return key.time < layerTime; });
                    const auto nextKey = std::find_if(property.keys.begin(), property.keys.end(),
                        [layerTime](const KeyframeModel::Keyframe& key) { return key.time > layerTime; });
                    const bool hasPrevious = previousKey != property.keys.rend();
                    const bool hasNext = nextKey != property.keys.end();
                    const Project::Tick previousTime = hasPrevious ? previousKey->time : Project::Tick::ZERO;
                    const Project::Tick nextTime = hasNext ? nextKey->time : Project::Tick::ZERO;
                    ImGui::SetCursorScreenPos({nameX, rp.y + 2.0f});
                    if (Icons::iconButton("##prevkey", Icons::Icon::ChevronLeft, false, {12, 14}) && hasPrevious)
                        setCurrentTime(layer.compTime(previousTime));
                    nameX += 12.0f;
                    ImGui::SetCursorScreenPos({nameX, rp.y + 2.0f});
                    if (Icons::iconButton("##keytoggle", Icons::Icon::Keyframe,
                                          currentKey != property.keys.end(), {14, 14})) {
                        Timeline::toggleKeyframe(property, layerTime);
                        if (!property.is_animated()) layer.syncPropsToTransform();
                        proj->dirty = true;
                        invalidateFrameCache();
                    }
                    nameX += 14.0f;
                    ImGui::SetCursorScreenPos({nameX, rp.y + 2.0f});
                    if (Icons::iconButton("##nextkey", Icons::Icon::ChevronRight, false, {12, 14}) && hasNext)
                        setCurrentTime(layer.compTime(nextTime));
                    nameX += 12.0f;
                }
                rowDl->AddText({nameX, rp.y + 2.0f}, C_DIM, property.name.c_str());

                const KeyframeModel::Value value = property.value_at(layerTime);
                float numeric[3] = {
                    static_cast<float>(value.kind == KeyframeModel::ValueKind::Scalar ? value.scalar :
                        value.kind == KeyframeModel::ValueKind::Vec2 ? value.v2[0] :
                        value.kind == KeyframeModel::ValueKind::Vec3 ? value.v3[0] : 0.0),
                    static_cast<float>(value.kind == KeyframeModel::ValueKind::Vec2 ? value.v2[1] :
                        value.kind == KeyframeModel::ValueKind::Vec3 ? value.v3[1] : 0.0),
                    static_cast<float>(value.kind == KeyframeModel::ValueKind::Vec3 ? value.v3[2] : 0.0)
                };
                const size_t valueWidth = value.kind == KeyframeModel::ValueKind::Vec3 ? 112 :
                    value.kind == KeyframeModel::ValueKind::Vec2 ? 82 :
                    value.kind == KeyframeModel::ValueKind::Color ? 48 :
                    value.kind == KeyframeModel::ValueKind::Bool ? 48 :
                    value.kind == KeyframeModel::ValueKind::Enum ? 110 :
                    value.kind == KeyframeModel::ValueKind::Path ? 74 : 62;
                ImGui::SetCursorScreenPos({rp.x + leftSideW - static_cast<float>(valueWidth) - 6.0f, rp.y + 1.0f});
                ImGui::SetNextItemWidth(static_cast<float>(valueWidth));
                bool changed = false;
                KeyframeModel::Value edited = value;
                if (value.kind == KeyframeModel::ValueKind::Scalar) {
                    changed = ImGui::DragFloat("##value", &numeric[0], 0.1f, 0.0f, 0.0f, "%.3g");
                } else if (value.kind == KeyframeModel::ValueKind::Vec2) {
                    changed = ImGui::DragFloat2("##value", numeric, 0.1f, 0.0f, 0.0f, "%.3g");
                } else if (value.kind == KeyframeModel::ValueKind::Vec3) {
                    changed = ImGui::DragFloat3("##value", numeric, 0.1f, 0.0f, 0.0f, "%.3g");
                } else if (value.kind == KeyframeModel::ValueKind::Color) {
                    float color[4] = {
                        static_cast<float>(value.color[0]), static_cast<float>(value.color[1]),
                        static_cast<float>(value.color[2]), static_cast<float>(value.color[3])
                    };
                    changed = ImGui::ColorEdit4("##value", color,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    if (changed)
                        edited = KeyframeModel::Value::Color(color[0], color[1], color[2], color[3]);
                } else if (value.kind == KeyframeModel::ValueKind::Bool) {
                    bool enabled = value.b;
                    changed = ImGui::Checkbox("##value", &enabled);
                    if (changed) edited = KeyframeModel::Value::Bool(enabled);
                } else if (value.kind == KeyframeModel::ValueKind::Enum && !property.ui.options.empty()) {
                    const uint32_t selected = value.e;
                    const char* preview = selected < property.ui.options.size()
                        ? property.ui.options[selected].c_str() : "";
                    if (ImGui::BeginCombo("##value", preview)) {
                        for (size_t option = 0; option < property.ui.options.size(); ++option) {
                            const bool isSelected = selected == option;
                            if (ImGui::Selectable(property.ui.options[option].c_str(), isSelected)) {
                                edited = KeyframeModel::Value::Enum(static_cast<uint32_t>(option));
                                changed = true;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else if (value.kind == KeyframeModel::ValueKind::Path) {
                    if (ImGui::Button("Shape...")) ImGui::OpenPopup("##path-editor");
                    if (ImGui::BeginPopup("##path-editor")) {
                        KeyframeModel::ShapePath path = value.path;
                        path.in_tangents.resize(path.vertices.size(), {0.0, 0.0});
                        path.out_tangents.resize(path.vertices.size(), {0.0, 0.0});
                        bool pathChanged = ImGui::Checkbox("Closed", &path.closed);
                        ImGui::Separator();
                        for (size_t vertex = 0; vertex < path.vertices.size(); ++vertex) {
                            ImGui::PushID(static_cast<int>(vertex));
                            float point[2] = {
                                static_cast<float>(path.vertices[vertex][0]),
                                static_cast<float>(path.vertices[vertex][1])
                            };
                            ImGui::Text("%zu", vertex + 1);
                            ImGui::SameLine();
                            if (ImGui::DragFloat2("##vertex", point, 0.1f, 0.0f, 0.0f, "%.3g")) {
                                path.vertices[vertex] = {point[0], point[1]};
                                pathChanged = true;
                            }
                            float inTangent[2] = {
                                static_cast<float>(path.in_tangents[vertex][0]),
                                static_cast<float>(path.in_tangents[vertex][1])
                            };
                            ImGui::Text("In");
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(116.0f);
                            if (ImGui::DragFloat2("##in-tangent", inTangent, 0.1f, 0.0f, 0.0f, "%.3g")) {
                                path.in_tangents[vertex] = {inTangent[0], inTangent[1]};
                                pathChanged = true;
                            }
                            float outTangent[2] = {
                                static_cast<float>(path.out_tangents[vertex][0]),
                                static_cast<float>(path.out_tangents[vertex][1])
                            };
                            ImGui::Text("Out");
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(116.0f);
                            if (ImGui::DragFloat2("##out-tangent", outTangent, 0.1f, 0.0f, 0.0f, "%.3g")) {
                                path.out_tangents[vertex] = {outTangent[0], outTangent[1]};
                                pathChanged = true;
                            }
                            ImGui::PopID();
                        }
                        if (ImGui::Button("Add Vertex")) {
                            const auto point = path.vertices.empty()
                                ? std::array<double, 2>{0.0, 0.0}
                                : std::array<double, 2>{path.vertices.back()[0] + 20.0,
                                                        path.vertices.back()[1]};
                            path.vertices.push_back(point);
                            path.in_tangents.resize(path.vertices.size(), {0.0, 0.0});
                            path.out_tangents.resize(path.vertices.size(), {0.0, 0.0});
                            pathChanged = true;
                        }
                        if (!path.vertices.empty()) {
                            ImGui::SameLine();
                            if (ImGui::Button("Remove Vertex")) {
                                path.vertices.pop_back();
                                if (path.in_tangents.size() > path.vertices.size())
                                    path.in_tangents.resize(path.vertices.size());
                                if (path.out_tangents.size() > path.vertices.size())
                                    path.out_tangents.resize(path.vertices.size());
                                pathChanged = true;
                            }
                        }
                        if (pathChanged) {
                            edited = KeyframeModel::Value::Path(path);
                            changed = true;
                        }
                        ImGui::EndPopup();
                    }
                }
                if (changed) {
                    if (value.kind == KeyframeModel::ValueKind::Scalar)
                        edited = KeyframeModel::Value::Scalar(numeric[0]);
                    else if (value.kind == KeyframeModel::ValueKind::Vec2)
                        edited = KeyframeModel::Value::Vec2(numeric[0], numeric[1]);
                    else if (value.kind == KeyframeModel::ValueKind::Vec3)
                        edited = KeyframeModel::Value::Vec3(numeric[0], numeric[1], numeric[2]);
                    property.set_value_at(layerTime, edited);
                    if (!property.is_animated()) layer.syncPropsToTransform();
                    proj->dirty = true;
                    invalidateFrameCache();
                }
            }
            ImGui::PopID();
        }
        rowDl->AddLine({rp.x, rp.y + ROW_H - 1.0f}, {rp.x + leftSideW, rp.y + ROW_H - 1.0f}, C_LINE);
        ImGui::SetCursorScreenPos({rp.x, rp.y + ROW_H});
    }
    const ImVec2 blankStart = ImGui::GetCursorScreenPos();
    const ImVec2 blankSize = ImGui::GetContentRegionAvail();
    if (blankSize.y > 1.0f) {
        ImGui::SetCursorScreenPos(blankStart);
        ImGui::InvisibleButton("##layer-marquee", {leftSideW - 8.0f, blankSize.y});
        if (ImGui::IsItemActivated()) {
            s_layerMarqueeActive = true;
            s_layerMarqueeStart = ImGui::GetIO().MousePos;
        }
    }
    if (s_layerMarqueeActive && (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                 ImGui::IsMouseReleased(ImGuiMouseButton_Left))) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const ImVec2 a{std::min(s_layerMarqueeStart.x, mouse.x), std::min(s_layerMarqueeStart.y, mouse.y)};
        const ImVec2 b{std::max(s_layerMarqueeStart.x, mouse.x), std::max(s_layerMarqueeStart.y, mouse.y)};
        ImGui::GetWindowDrawList()->AddRectFilled(a, b, IM_COL32(0, 123, 183, 40));
        ImGui::GetWindowDrawList()->AddRect(a, b, C_BLUE);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl)
                s_selectedLayers.clear();
            for (const LayerRowHit& hit : s_layerRowHits) {
                const bool intersects = hit.min.y <= b.y && hit.max.y >= a.y &&
                    hit.min.x <= b.x && hit.max.x >= a.x;
                if (intersects && hit.index < comp.layers.size() &&
                    !comp.layers[hit.index].switches.locked)
                    s_selectedLayers.insert(hit.id);
            }
            s_layerMarqueeActive = false;
            s_selectedLayer = s_selectedLayers.empty() ? 0 : *s_selectedLayers.begin();
        }
    }
    if (s_layerReorderActive && !s_layerReorderIds.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float mouseY = ImGui::GetIO().MousePos.y;
        float lineY = mouseY;
        for (const LayerRowHit& hit : s_layerRowHits) {
            if (std::find(s_layerReorderIds.begin(), s_layerReorderIds.end(), hit.id) != s_layerReorderIds.end())
                continue;
            if (mouseY < (hit.min.y + hit.max.y) * 0.5f) {
                lineY = hit.min.y;
                break;
            }
            lineY = hit.max.y;
        }
        ImGui::GetForegroundDrawList()->AddLine(
            {p0.x, lineY}, {p0.x + leftSideW, lineY}, C_BLUE, 2.0f);
    }
    s_timelineScroll = ImGui::GetScrollY();
    ImGui::EndChild();

    vLine(dl, p0.x + leftSideW, p0.y + hdrH + colHdrH, p0.y + hdrH + colHdrH + layerAreaH);

    const ImVec2 barAreaP = {barAreaX, rowTop};
    ImGui::SetCursorScreenPos(barAreaP);
    ImGui::SetNextWindowScroll({0.0f, s_timelineScroll});
    ImGui::BeginChild("##tlbars", {barAreaW, layerAreaH}, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImDrawList* trDl = ImGui::GetWindowDrawList();

    const ImVec2 rp2 = ImGui::GetCursorScreenPos();
    trDl->AddRectFilled(rp2, {rp2.x + barAreaW, rp2.y + rulerH}, C_APP_BG);
    hLine(trDl, rp2.x, rp2.x + barAreaW, rp2.y + rulerH);

    const double candidates[] = {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 120.0};
    double tickStep = candidates[0];
    for (double step : candidates) {
        if (step * pps >= 58.0) { tickStep = step; break; }
        tickStep = step;
    }
    const double firstTick = std::floor(s_timelineStart / tickStep) * tickStep;
    for (double t = firstTick; t <= s_timelineStart + visible + tickStep; t += tickStep) {
        const float x = rp2.x + (float)((t - s_timelineStart) * pps);
        trDl->AddLine({x, rp2.y + rulerH - 7.0f}, {x, rp2.y + rulerH}, C_LINE);
        const double frameSeconds = fr.frame_duration().seconds();
        int64_t labelFrame = fr.frame_at(Project::Tick::from_seconds_f64(t));
        if (frameSeconds > 0.0 && std::isfinite(frameSeconds)) {
            const double frame = t / frameSeconds;
            if (std::isfinite(frame) && frame < (double)INT64_MAX - 2048.0 &&
                frame > (double)INT64_MIN + 2048.0)
                labelFrame = (int64_t)std::llround(frame);
            else if (frame >= 0.0)
                labelFrame = INT64_MAX;
            else
                labelFrame = INT64_MIN;
        }
        const std::string label = Project::format_timecode_ae(labelFrame, fr, false);
        trDl->AddText({x + 2.0f, rp2.y + 2.0f}, C_DIM, label.c_str());
        for (int sub = 1; sub < 5; ++sub) {
            const float sx = x + (float)(tickStep * pps * sub / 5.0);
            trDl->AddLine({sx, rp2.y + rulerH - 3.0f}, {sx, rp2.y + rulerH}, C_LINE);
        }
    }
    const double workStart = comp.workAreaStart.seconds();
    const double workDuration = comp.workAreaDur.v > 0 ? comp.workAreaDur.seconds() : duration;
    const float workX0 = rp2.x + (float)((workStart - s_timelineStart) * pps);
    const float workX1 = rp2.x + (float)((workStart + workDuration - s_timelineStart) * pps);
    trDl->AddRectFilled({workX0, rp2.y}, {workX1, rp2.y + 3.0f}, C_BLUE);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rulerH);

    struct BarDrag {
        uint64_t layer = 0;
        int mode = 0; // 1 = move, 2 = trim in, 3 = trim out
        float startX = 0.0f;
        Project::Tick in = Project::Tick::ZERO;
        Project::Tick out = Project::Tick::ZERO;
        Project::Tick startTime = Project::Tick::ZERO;
    };
    static BarDrag drag;
    const ImVec2 graphMin = {rp2.x, rp2.y + rulerH};
    const ImVec2 graphMax = {rp2.x + barAreaW, rp2.y + layerAreaH};
    s_propertyRowHits.clear();
    s_keyMarkerClicked = false;
    if (!s_graphEditor) {
        ImGui::SetCursorScreenPos(graphMin);
        ImGui::InvisibleButton("##key-marquee-background", {barAreaW, layerAreaH - rulerH});
        ImGui::SetItemAllowOverlap();
        if (ImGui::IsItemActivated()) {
            s_keyMarqueeActive = true;
            s_keyMarqueeStart = ImGui::GetIO().MousePos;
        }
    } else {
        s_keyMarqueeActive = false;
    }
    ImGui::SetCursorScreenPos(graphMin);
    double graphValueMin = std::numeric_limits<double>::infinity();
    double graphValueMax = -std::numeric_limits<double>::infinity();
    if (s_graphEditor) {
        trDl->AddRectFilled(graphMin, graphMax, C_WELL);
        const int rangeSamples = std::clamp(static_cast<int>(barAreaW / 8.0f), 2, 120);
        for (const Row& row : rows) {
            if (row.kind != Row::Kind::Property || !row.property || row.property->keys.empty())
                continue;
            Project::Layer& layer = comp.layers[row.layer];
            Props::Property& property = *row.property;
            const size_t dimensions = numericDimensions(property.value);
            const size_t graphDimensions = s_graphSpeed && property.spatial ? 1 : dimensions;
            for (int i = 0; i < rangeSamples; ++i) {
                const double time = s_timelineStart + visible * i / (rangeSamples - 1);
                const Project::Tick compTime = Project::Tick::from_seconds_f64(time);
                for (size_t d = 0; d < graphDimensions; ++d) {
                    const double value = graphComponentAt(layer, property, compTime, d);
                    if (!std::isfinite(value)) continue;
                    graphValueMin = std::min(graphValueMin, value);
                    graphValueMax = std::max(graphValueMax, value);
                }
            }
        }
        const bool hasCurves = std::isfinite(graphValueMin) && std::isfinite(graphValueMax);
        if (hasCurves) {
            const double padding = std::max(1.0, (graphValueMax - graphValueMin) * 0.1);
            graphValueMin -= padding;
            graphValueMax += padding;
            if (graphValueMax - graphValueMin < 1e-12) graphValueMax = graphValueMin + 1.0;
        }
        trDl->PushClipRect(graphMin, graphMax, true);
        for (int i = 0; i <= 4; ++i) {
            const float y = graphMin.y + (graphMax.y - graphMin.y) * static_cast<float>(i) / 4.0f;
            trDl->AddLine({graphMin.x + 42.0f, y}, {graphMax.x, y}, C_LINE);
            if (hasCurves) {
                const double value = graphValueMax - (graphValueMax - graphValueMin) * i / 4.0;
                char label[32];
                snprintf(label, sizeof label, "%.3g", value);
                trDl->AddText({graphMin.x + 4.0f, y - 6.0f}, C_DIM, label);
            }
        }
        if (!hasCurves) {
            trDl->AddText({graphMin.x + 54.0f, graphMin.y + 18.0f}, C_DIM,
                          L("Select an animated numeric property"));
        } else {
            const ImU32 graphColors[] = {
                IM_COL32(232, 200, 64, 255), IM_COL32(208, 112, 224, 255),
                IM_COL32(64, 200, 200, 255), IM_COL32(240, 144, 64, 255),
                IM_COL32(156, 216, 80, 255), IM_COL32(240, 112, 144, 255)
            };
            const int sampleCount = std::clamp(static_cast<int>(barAreaW / 8.0f), 2, 120);
            size_t curveIndex = 0;
            for (const Row& row : rows) {
                if (row.kind != Row::Kind::Property || !row.property || row.property->keys.empty())
                    continue;
                Project::Layer& layer = comp.layers[row.layer];
                Props::Property& property = *row.property;
                const size_t dimensions = numericDimensions(property.value);
                if (dimensions == 0) continue;
                const size_t graphDimensions = s_graphSpeed && property.spatial ? 1 : dimensions;
                for (size_t dimension = 0; dimension < graphDimensions; ++dimension) {
                    std::vector<ImVec2> points;
                    points.reserve(static_cast<size_t>(sampleCount));
                    for (int i = 0; i < sampleCount; ++i) {
                        const float x = graphMin.x + (graphMax.x - graphMin.x) * i / (sampleCount - 1);
                        const double time = s_timelineStart + (x - rp2.x) / pps;
                        const Project::Tick compTime = Project::Tick::from_seconds_f64(time);
                        const double component = graphComponentAt(layer, property, compTime, dimension);
                        const double unit = std::clamp((component - graphValueMin) /
                            (graphValueMax - graphValueMin), 0.0, 1.0);
                        const float y = graphMax.y - static_cast<float>(unit) * (graphMax.y - graphMin.y);
                        points.push_back({x, y});
                    }
                    if (points.size() >= 2)
                        trDl->AddPolyline(points.data(), static_cast<int>(points.size()),
                                          graphColors[curveIndex % (sizeof(graphColors) / sizeof(graphColors[0]))],
                                          ImDrawFlags_None, 1.5f);
                    const ImU32 color = graphColors[curveIndex % (sizeof(graphColors) / sizeof(graphColors[0]))];
                    for (size_t ki = 0; ki < property.keys.size(); ++ki) {
                        auto& key = property.keys[ki];
                        const Project::Tick compTime = layer.compTime(key.time);
                        const double value = s_graphSpeed
                            ? graphComponentAt(layer, property, compTime, dimension)
                            : numericComponent(key.value, dimension);
                        if (!std::isfinite(value)) continue;
                        const float x = rp2.x + static_cast<float>((compTime.seconds() - s_timelineStart) * pps);
                        if (x < graphMin.x - 8.0f || x > graphMax.x + 8.0f) continue;
                        const double unit = std::clamp((value - graphValueMin) /
                            (graphValueMax - graphValueMin), 0.0, 1.0);
                        const float y = graphMax.y - static_cast<float>(unit) * (graphMax.y - graphMin.y);
                        trDl->AddCircleFilled({x, y}, 4.0f, color);
                        for (bool out : {false, true})
                            graphEaseHandle(proj, layer, property, ki, dimension, out, trDl,
                                            graphMin, graphMax, graphValueMin, graphValueMax, pps);
                        keyframeMarker(proj, comp, layer, property, key, ki, dimension, trDl,
                                       {x, y}, true, !s_graphSpeed, graphValueMin, graphValueMax,
                                       graphMax.y - graphMin.y);
                    }
                    ++curveIndex;
                }
            }
        }
        trDl->PopClipRect();
        trDl->AddRect(graphMin, graphMax, C_LINE);
        ImGui::SetCursorScreenPos({rp2.x, graphMax.y});
    } else {
    for (size_t ri = 0; ri < rows.size(); ++ri) {
        const Row& row = rows[ri];
        Project::Layer& layer = comp.layers[row.layer];
        const ImVec2 brp = ImGui::GetCursorScreenPos();
        if (row.kind == Row::Kind::Layer) {
            const float inX = rp2.x + (float)((layer.inPt.seconds() - s_timelineStart) * pps);
            const float outX = rp2.x + (float)((layer.outPt.seconds() - s_timelineStart) * pps);
            const float barWidth = std::max(0.0f, outX - inX);
            const float k = s_selectedLayer == layer.id.v ? 0.62f : 0.45f;
            const ImVec4 label = col4(labelColor(static_cast<int>(layer.label)));
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32({
                label.x * k + 0x3a / 255.0f * (1.0f - k),
                label.y * k + 0x3a / 255.0f * (1.0f - k),
                label.z * k + 0x3a / 255.0f * (1.0f - k), 1.0f});
            if (barWidth > 0.0f) {
                trDl->AddRectFilled({inX, brp.y + 2.0f}, {outX, brp.y + ROW_H - 2.0f}, fill, 2.0f);
                trDl->AddLine({inX, brp.y + 2.0f}, {inX, brp.y + ROW_H - 2.0f}, IM_COL32(220, 220, 220, 220), 1.0f);
                trDl->AddLine({outX, brp.y + 2.0f}, {outX, brp.y + ROW_H - 2.0f}, IM_COL32(220, 220, 220, 220), 1.0f);
                if (barWidth > 30.0f)
                    trDl->AddText({inX + 5.0f, brp.y + 2.0f}, IM_COL32(230, 230, 230, 255), layer.name.c_str());
            }
            ImGui::SetCursorScreenPos({rp2.x, brp.y});
            ImGui::InvisibleButton("##bar-hit", {barAreaW, ROW_H});
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool nearIn = mouse.x >= inX - 4.0f && mouse.x <= inX + std::min(5.0f, barWidth / 3.0f);
            const bool nearOut = mouse.x >= outX - std::min(5.0f, barWidth / 3.0f) && mouse.x <= outX + 4.0f;
            if (ImGui::IsItemHovered() && (nearIn || nearOut))
                resizeCursor = true;
            if (ImGui::IsItemClicked() && mouse.x >= inX - 4.0f && mouse.x <= outX + 4.0f) {
                s_keyMarqueeActive = false;
                s_selectedLayer = layer.id.v;
                s_selectedLayers.clear();
                s_selectedLayers.insert(layer.id.v);
                s_selectionAnchor = layer.id.v;
                drag.layer = layer.id.v;
                drag.mode = nearOut ? 3 : (nearIn ? 2 : 1);
                drag.startX = mouse.x;
                drag.in = layer.inPt;
                drag.out = layer.outPt;
                drag.startTime = layer.startTime;
            }
            if (drag.layer == layer.id.v && ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
                const Project::Tick offset = fr.snap_nearest(Project::Tick::from_seconds_f64((mouse.x - drag.startX) / pps));
                Project::Tick appliedOffset = offset;
                if (drag.mode == 1) {
                    if (ImGui::GetIO().KeyShift) {
                        double targets[2 * 64];
                        size_t count = 0;
                        for (const auto& other : comp.layers) {
                            if (other.id == layer.id || count + 2 > 128) continue;
                            targets[count++] = other.inPt.seconds();
                            targets[count++] = other.outPt.seconds();
                        }
                        const double shiftedIn = (drag.in + appliedOffset).seconds();
                        const double shiftedOut = (drag.out + appliedOffset).seconds();
                        const double a = Timeline::snapTime(targets, count, shiftedIn, 8.0 / pps) - shiftedIn;
                        const double b = Timeline::snapTime(targets, count, shiftedOut, 8.0 / pps) - shiftedOut;
                        if (std::abs(a) <= std::abs(b) && a != 0.0) appliedOffset += Project::Tick::from_seconds_f64(a);
                        else if (b != 0.0) appliedOffset += Project::Tick::from_seconds_f64(b);
                    }
                }
                const Timeline::BarEdit edit = drag.mode == 2 ? Timeline::BarEdit::TrimIn
                    : drag.mode == 3 ? Timeline::BarEdit::TrimOut : Timeline::BarEdit::Move;
                const Timeline::LayerTiming editedTime = Timeline::editLayerTiming(
                    {drag.in, drag.out, drag.startTime}, edit, appliedOffset, fr.frame_duration(), comp.duration);
                Project::Layer& edited = comp.layers[row.layer];
                if (edited.inPt != editedTime.in || edited.outPt != editedTime.out || edited.startTime != editedTime.start) {
                    edited.inPt = editedTime.in;
                    edited.outPt = editedTime.out;
                    edited.startTime = editedTime.start;
                    proj->dirty = true;
                    invalidateFrameCache();
                }
            }
        } else if (row.kind == Row::Kind::Group) {
            trDl->AddRectFilled({rp2.x, brp.y}, {rp2.x + barAreaW, brp.y + ROW_H}, IM_COL32(36, 36, 36, 255));
            trDl->AddText({rp2.x + 8.0f, brp.y + 2.0f}, C_DIM, row.group->name.c_str());
        } else {
            trDl->AddLine({rp2.x, brp.y + ROW_H - 1.0f}, {rp2.x + barAreaW, brp.y + ROW_H - 1.0f}, C_LINE);
            Props::Property& property = *row.property;
            s_propertyRowHits.push_back({brp.y + ROW_H * 0.5f, row.layer, property.uid});
            for (size_t ki = 0; ki < property.keys.size(); ++ki) {
                auto& key = property.keys[ki];
                const Project::Tick compTime = layer.compTime(key.time);
                const float x = rp2.x + static_cast<float>((compTime.seconds() - s_timelineStart) * pps);
                if (x < rp2.x - 8.0f || x > rp2.x + barAreaW + 8.0f) continue;
                keyframeMarker(proj, comp, layer, property, key, ki, 0, trDl,
                               {x, brp.y + ROW_H * 0.5f}, false, false, 0.0, 1.0, ROW_H);
            }
        }
        if (drag.layer == layer.id.v && !ImGui::IsMouseDown(0))
            drag = {};
        ImGui::SetCursorScreenPos({rp2.x, brp.y + ROW_H});
    }
    }

    if (pendingShapeItem && proj) {
        const auto target = std::find_if(comp.layers.begin(), comp.layers.end(), [&](const Project::Layer& layer) {
            return layer.id.v == pendingShapeItem->layer;
        });
        if (target != comp.layers.end())
            appendShapeItem(proj, *target, pendingShapeItem->kind, pendingShapeItem->parent);
    }

    if (s_keyMarqueeActive && !s_graphEditor) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const ImVec2 min{std::min(s_keyMarqueeStart.x, mouse.x),
                         std::min(s_keyMarqueeStart.y, mouse.y)};
        const ImVec2 max{std::max(s_keyMarqueeStart.x, mouse.x),
                         std::max(s_keyMarqueeStart.y, mouse.y)};
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const double speed = Timeline::edgeScrollSpeedX(graphMin.x, graphMax.x, mouse.x);
            if (speed != 0.0 && pps > 0.0) {
                const double dt = std::clamp(static_cast<double>(ImGui::GetIO().DeltaTime), 0.0, 0.1);
                const double span = (graphMax.x - graphMin.x) / pps;
                const double maxStart = std::max(0.0, comp.duration.seconds() - span);
                s_timelineStart = std::clamp(s_timelineStart + speed * dt / pps, 0.0, maxStart);
            }
            trDl->PushClipRect(graphMin, graphMax, true);
            trDl->AddRectFilled(min, max, IM_COL32(0, 123, 183, 40));
            trDl->AddRect(min, max, C_BLUE);
            trDl->PopClipRect();
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            s_keyMarqueeActive = false;
            if (!s_keyMarkerClicked &&
                std::abs(max.x - min.x) + std::abs(max.y - min.y) >= 3.0f) {
                if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper)
                    s_selectedKeys.clear();
                std::vector<Timeline::KeyMarker> markers;
                for (const PropertyRowHit& hit : s_propertyRowHits) {
                    if (hit.layer >= comp.layers.size()) continue;
                    Project::Layer& layer = comp.layers[hit.layer];
                    Props::Property* property = layer.props.find_mut(hit.property);
                    if (!property) continue;
                    for (const auto& key : property->keys) {
                        const float x = rp2.x + static_cast<float>(
                            (layer.compTime(key.time).seconds() - s_timelineStart) * pps);
                        markers.push_back({layer.id.v, property->uid, key.time, x, hit.y, 9.0f});
                    }
                }
                const auto hits = Timeline::keysInBox(markers, min.x, min.y, max.x, max.y);
                for (const auto& hit : hits) {
                    const Timeline::KeyRef selected{hit.layer, hit.property, hit.time};
                    if (std::none_of(s_selectedKeys.begin(), s_selectedKeys.end(),
                        [&](const Timeline::KeyRef& existing) {
                            return existing.layer == selected.layer &&
                                existing.property == selected.property && existing.time == selected.time;
                        }))
                        s_selectedKeys.push_back(selected);
                }
                if (!hits.empty())
                    s_selectedKey = {hits.back().layer, hits.back().property, hits.back().time, 0};
                else if (s_selectedKeys.empty())
                    s_selectedKey = {};
            }
        }
    }

    updateKeyDrag(proj, comp, pps);

    const float playheadX = rp2.x + (float)((s_currentTime.seconds() - s_timelineStart) * pps);
    trDl->AddLine({playheadX, rp2.y}, {playheadX, rp2.y + layerAreaH}, C_BLUE, 1.5f);
    const ImVec2 headPts[] = {
        {playheadX - 5.0f, rp2.y}, {playheadX + 5.0f, rp2.y},
        {playheadX + 5.0f, rp2.y + 10.0f}, {playheadX, rp2.y + 15.0f},
        {playheadX - 5.0f, rp2.y + 10.0f}
    };
    trDl->AddConvexPolyFilled(headPts, 5, C_BLUE);

    ImGui::SetCursorScreenPos({rp2.x, rp2.y + 2.0f});
    ImGui::InvisibleButton("##ruler-scrub", {barAreaW, rulerH - 2.0f});
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        const float x = std::clamp(ImGui::GetIO().MousePos.x, rp2.x, rp2.x + barAreaW);
        setCurrentTime(Timeline::timeAtPixel(x, rp2.x, s_timelineStart, pps, fr, comp.duration));
        s_playing = false;
    }
    if (resizeCursor)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::GetWindowDrawList()->AddLine({rp2.x, rp2.y + rulerH}, {rp2.x + barAreaW, rp2.y + rulerH}, C_LINE);
    s_timelineScroll = ImGui::GetScrollY();
    ImGui::EndChild();

    if (s_layerReorderActive && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                                 ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !s_layerReorderIds.empty()) {
            const float mouseY = ImGui::GetIO().MousePos.y;
            std::optional<uint64_t> before;
            for (const LayerRowHit& hit : s_layerRowHits) {
                if (std::find(s_layerReorderIds.begin(), s_layerReorderIds.end(), hit.id) != s_layerReorderIds.end())
                    continue;
                if (mouseY < (hit.min.y + hit.max.y) * 0.5f) {
                    before = hit.id;
                    break;
                }
            }
            std::vector<uint64_t> order;
            order.reserve(comp.layers.size());
            for (const auto& layer : comp.layers) order.push_back(layer.id.v);
            const std::vector<uint64_t> reordered = Timeline::reorderLayers(order, s_layerReorderIds, before);
            if (reordered != order) {
                std::unordered_map<uint64_t, Project::Layer> layersById;
                layersById.reserve(comp.layers.size());
                for (auto& layer : comp.layers)
                    layersById.emplace(layer.id.v, std::move(layer));
                comp.layers.clear();
                comp.layers.reserve(reordered.size());
                for (uint64_t id : reordered) {
                    auto found = layersById.find(id);
                    if (found != layersById.end())
                        comp.layers.push_back(std::move(found->second));
                }
                proj->dirty = true;
                invalidateFrameCache();
            }
        }
        s_layerReorderActive = false;
        s_layerReorderIds.clear();
    }

    const ImVec2 bp0 = {p0.x, p0.y + hdrH + colHdrH + layerAreaH};
    hLine(dl, bp0.x, bp0.x + avW, bp0.y);
    dl->AddRectFilled(bp0, {bp0.x + avW, bp0.y + botBarH}, C_APP_BG);

    if (s_graphEditor) {
        ImGui::SetCursorScreenPos({bp0.x + leftSideW + 8.0f, bp0.y + 2.0f});
        if (ImGui::SmallButton("Value Graph"))
            s_graphSpeed = false;
        ImGui::SameLine(0, 3);
        if (ImGui::SmallButton("Speed Graph"))
            s_graphSpeed = true;
        ImGui::SameLine(0, 3);
        if (ImGui::SmallButton("Fit Graph")) {
            s_timelineStart = 0.0;
            s_timelinePps = 0.0;
            s_timelineZoom = 0.0f;
        }
    }

    ImGui::SetCursorScreenPos({bp0.x + 8.0f, bp0.y + 2.0f});
    if (ImGui::SmallButton(L("Toggle Switches / Modes"))) {}

    ImGui::SetCursorScreenPos({bp0.x + avW - 150.0f, bp0.y + 2.0f});
    Icons::paintCentered(dl, {bp0.x + avW - 140.0f, bp0.y + botBarH * 0.5f}, 12.0f, Icons::Icon::MountainSmall, C_DIM);
    ImGui::SetCursorScreenPos({bp0.x + avW - 130.0f, bp0.y + 2.0f});
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::SliderFloat("##tlzoom", &s_timelineZoom, 0.0f, 100.0f, "")) {
        const double factor = std::pow(50.0, s_timelineZoom / 100.0f);
        Timeline::setZoom(s_timelineStart, s_timelinePps, fitPps * factor, fitPps,
                          duration, s_currentTime.seconds(), barAreaW * 0.5);
    }
    Icons::paintCentered(dl, {bp0.x + avW - 14.0f, bp0.y + botBarH * 0.5f}, 14.0f, Icons::Icon::MountainLarge, C_DIM);
}

// after EffectCraft crates/engine/src/cmd/layer.rs new_shape
uint64_t createShapeLayer(Project::ProjectData* project) {
    if (!project || project->activeComp < 0 ||
        static_cast<size_t>(project->activeComp) >= project->compositions.size()) return 0;
    Project::Composition& comp = project->compositions[static_cast<size_t>(project->activeComp)];
    Project::Layer layer;
    layer.id.v = project->alloc();
    layer.name = comp.uniqueLayerName("Shape Layer");
    layer.kind = Project::LayerKind::Shape;
    layer.inPt = Project::Tick::ZERO;
    layer.outPt = comp.duration;
    layer.startTime = Project::Tick::ZERO;
    layer.initProps(&project->nextId, 0.0, 0.0,
                    static_cast<double>(comp.width) * 0.5,
                    static_cast<double>(comp.height) * 0.5);
    const uint64_t layerId = layer.id.v;
    const Props::PropGroup* contents = layer.props.group("contents");
    if (contents) s_openGroups.insert(contents->uid);
    comp.layers.push_back(std::move(layer));
    s_selectedLayer = layerId;
    s_selectedLayers.clear();
    s_selectedLayers.insert(layerId);
    s_selectionAnchor = layerId;
    s_openLayers.insert(layerId);
    s_selectedShapeLayer = layerId;
    s_selectedShapeGroup = 0;
    project->dirty = true;
    invalidateFrameCache();
    return layerId;
}

// after EffectCraft crates/ui-egui/src/panels/live_preview.rs
static void drawPreviewPanel(Project::Project* proj) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const Project::Composition* comp = nullptr;
    if (proj && proj->activeComp >= 0 && (size_t)proj->activeComp < proj->compositions.size())
        comp = &proj->compositions[(size_t)proj->activeComp];

    // Transport buttons row with vector icons
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs transport buttons
    ImGui::SetCursorScreenPos({p0.x + 8.0f, p0.y + 4.0f});
    if (Icons::iconButton("##p_first", Icons::Icon::First, false, {22, 22}) && comp) {
        setCurrentTime(Project::Tick::ZERO);
        s_playing = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("First Frame"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##p_stepback", Icons::Icon::StepBack, false, {22, 22}) && comp) {
        setCurrentTime(comp->frameRate.tick_of(comp->frameRate.frame_at(s_currentTime) - 1).max_t(Project::Tick::ZERO));
        s_playing = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Previous Frame"));
    ImGui::SameLine(0, 2);
    static bool s_playing = false;
    if (Icons::iconButton("##p_play", s_playing ? Icons::Icon::Pause : Icons::Icon::Play, s_playing, {26, 22})) {
        s_playing = !s_playing;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s_playing ? L("Stop") : L("Play"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##p_stepfwd", Icons::Icon::StepFwd, false, {22, 22}) && comp) {
        const Project::Tick next = comp->frameRate.tick_of(comp->frameRate.frame_at(s_currentTime) + 1);
        setCurrentTime(next.min_t(comp->duration));
        s_playing = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Next Frame"));
    ImGui::SameLine(0, 2);
    if (Icons::iconButton("##p_last", Icons::Icon::Last, false, {22, 22}) && comp) {
        setCurrentTime(comp->frameRate.tick_of(std::max<int64_t>(0, comp->frameRate.frame_at(comp->duration - Project::Tick(1)))));
        s_playing = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Last Frame"));
    ImGui::SameLine(0, 4);
    static bool s_loop = true;
    if (Icons::iconButton("##p_loop", Icons::Icon::Loop, s_loop, {22, 22})) {
        s_loop = !s_loop;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("Loop"));

    hLine(dl, p0.x, p0.x + avW, p0.y + 32.0f);

    // Settings rows
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs settings rows
    ImGui::SetCursorScreenPos({p0.x + 8.0f, p0.y + 36.0f});
    ImGui::TextDisabled("%s", L("Shortcut")); ImGui::SameLine(90); ImGui::TextDisabled("Spacebar");
    ImGui::Checkbox(L("Loop"), &s_loop);

    ImGui::TextDisabled("%s", L("Range"));    ImGui::SameLine(90);
    ImGui::TextDisabled(L("Work Area Extended By Current Frame"));
    ImGui::TextDisabled("%s", L("Play From")); ImGui::SameLine(90);
    ImGui::TextDisabled(L("Current Time"));
    ImGui::TextDisabled("%s", L("Frame Rate")); ImGui::SameLine(90);
    ImGui::TextDisabled("(29.97)");
    ImGui::TextDisabled("%s", L("Resolution")); ImGui::SameLine(90);
    ImGui::TextDisabled(L("Auto"));

    hLine(dl, p0.x, p0.x + avW, ImGui::GetCursorScreenPos().y + 2.0f);

    // Cached-frames progress bar
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs cached frames bar
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    const ImVec2 cbP = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(cbP, {cbP.x + avW, cbP.y + 6.0f}, C_WELL, 2.0f);
    dl->AddRectFilled(cbP, {cbP.x + avW * 0.12f, cbP.y + 6.0f}, C_BLUE, 2.0f);
    ImGui::Dummy({avW, 8.0f});
    const int64_t totalFrames = comp ? std::max<int64_t>(1, comp->frameRate.frame_at(comp->duration)) : 0;
    char cachedText[96];
    snprintf(cachedText, sizeof cachedText, "%u / %lld %s",
             (unsigned)s_frameCache.size(), (long long)totalFrames, L("frames cached"));
    ImGui::TextDisabled("%s", cachedText);
}

// after EffectCraft crates/ui-egui/src/panels/properties.rs
static void drawPropertiesPanel(Project::Project* proj) {
    if (!proj || proj->activeComp < 0) {
        ImGui::TextDisabled("%s", L("No composition active."));
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    const auto& comp = proj->compositions[(size_t)proj->activeComp];
    char hdr[256]; snprintf(hdr, sizeof hdr, "%s: %s", L("Properties"), comp.name.c_str());
    dl->AddText({p0.x + 8.0f, p0.y + 4.0f}, C_TEXT, hdr);
    ImGui::Dummy({avW, 20.0f});
    hLine(dl, p0.x, p0.x + avW, ImGui::GetCursorScreenPos().y);

    // Layer Transform section
    // after EffectCraft crates/ui-egui/src/panels/properties.rs layer_transform section
    if (ImGui::CollapsingHeader(L("Layer Transform"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SameLine(avW - 46.0f);
        if (ImGui::SmallButton(L("Reset"))) {}

        static const struct { const char* name; float v[2]; bool point; } xformProps[] = {
            { L("Anchor Point"), {0, 0},     true },
            { L("Position"),     {960, 540}, true },
            { L("Scale"),        {100, 100}, true },
            { L("Rotation"),     {0, 0},     false },
            { L("Opacity"),      {100, 0},   false },
        };
        for (const auto& prop : xformProps) {
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            // Stopwatch icon
            Icons::paintCentered(dl, {cur.x + 8.0f, cur.y + 10.0f}, 12.0f, Icons::Icon::Stopwatch, C_BLUE);
            ImGui::Dummy({16, 18});
            ImGui::SameLine(0, 4);
            ImGui::TextDisabled("%s", prop.name);
            ImGui::SameLine(130.0f);
            if (prop.point) {
                ImGui::TextColored(col4(C_BLUE), "%.0f  %.0f", prop.v[0], prop.v[1]);
            } else {
                ImGui::TextColored(col4(C_BLUE), "%.0f", prop.v[0]);
            }
        }
    }
}

// after EffectCraft crates/ui-egui/src/panels/info.rs
static void drawInfoPanel(Project::Project* proj) {
    ImGui::TextDisabled("X: 0  Y: 0");
    ImGui::TextDisabled("R: 0  G: 0  B: 0");
    ImGui::TextDisabled("A: 255");
    if (proj) {
        ImGui::Separator();
        ImGui::TextDisabled("%s", proj->name.c_str());
    }
}

// after EffectCraft crates/ui-egui/src/panels/render_queue.rs
static void drawRenderQueue() {
    ImGui::TextDisabled(L("Render Queue is empty."));
}

// Advance playback by one UI frame, quantized to the composition's frame boundaries.
static void advancePlayback(Project::Project* proj) {
    if (!s_playing) return;
    if (!proj || proj->activeComp < 0 || (size_t)proj->activeComp >= proj->compositions.size()) {
        s_playing = false;
        return;
    }
    const auto& comp = proj->compositions[(size_t)proj->activeComp];
    const double dt = std::clamp((double)ImGui::GetIO().DeltaTime, 0.0, 0.1);
    double start = std::max(0.0, comp.workAreaStart.seconds());
    double end = comp.workAreaDur.v > 0
        ? std::min(comp.duration.seconds(), start + comp.workAreaDur.seconds())
        : comp.duration.seconds();
    if (end <= start) {
        start = 0.0;
        end = comp.duration.seconds();
    }
    const double span = end - start;
    if (span <= 0.0) {
        s_playing = false;
        setCurrentTime(Project::Tick::ZERO);
        return;
    }
    double next = s_currentTime.seconds() + dt;
    if (next >= end) {
        if (s_loop) {
            next = start + std::fmod(std::max(0.0, next - start), span);
        } else {
            next = std::max(start, end - comp.frameDuration().seconds());
            s_playing = false;
        }
    }
    setCurrentTime(comp.frameRate.snap_nearest(Project::Tick::from_seconds_f64(next)));
}

// ---------- header / tool bar ----------
// after EffectCraft crates/ui-egui/src/header.rs show()
// Tools bar: Morpheus brand | Home | tool slots | snapping | workspaces | » menu
static void drawHeader(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({x, y}, {x + w, y + h}, C_HEADER);
    hLine(dl, x, x + w, y + h);

    const float cy = y + h * 0.5f;
    float cx = x + 10.0f;

    // Brand mark (Morpheus M in Atelier blue)
    {
        const ImVec2 bmMin{cx, cy - 11.0f};
        const ImVec2 bmMax{cx + 22.0f, cy + 11.0f};
        dl->AddRectFilled(bmMin, bmMax, C_BLUE, 3.0f);
        // Draw crisp 'M' glyph inside
        const ImVec2 mPts[] = {
            {cx + 5.0f, cy + 7.0f}, {cx + 5.0f, cy - 6.0f},
            {cx + 11.0f, cy + 2.0f},
            {cx + 17.0f, cy - 6.0f}, {cx + 17.0f, cy + 7.0f}
        };
        dl->AddPolyline(mPts, 5, IM_COL32(255, 255, 255, 255), 0, 2.0f);
        cx += 28.0f;
    }

    // Home button
    {
        const ImVec2 hmMin{cx, cy - 12.0f};
        const ImVec2 hmMax{cx + 24.0f, cy + 12.0f};
        const bool hover = ImGui::IsMouseHoveringRect(hmMin, hmMax);
        if (hover) dl->AddRectFilled(hmMin, hmMax, IM_COL32(0x30, 0x30, 0x30, 255), 2.0f);
        Icons::paintCentered(dl, {cx + 12.0f, cy}, 16.0f, Icons::Icon::Home, hover ? C_TEXT : C_DIM);
        if (hover) ImGui::SetTooltip("%s", L("Home"));
        cx += 28.0f;
    }

    // Vertical divider
    vLine(dl, cx, y + 6.0f, y + h - 6.0f);
    cx += 8.0f;

    // Tool slots (12 slots from EffectCraft crates/ui-egui/src/state.rs Tool::SLOTS)
    struct ToolSlotDef {
        Icons::Icon icon;
        const char* label;
        const char* tip;
        bool hasFlyout;
    };
    static const ToolSlotDef SLOTS[] = {
        { Icons::Icon::Selection,   "Selection",   "Selection Tool (V)", false },
        { Icons::Icon::Hand,        "Hand",        "Hand Tool (H)", false },
        { Icons::Icon::Zoom,        "Zoom",        "Zoom Tool (Z)", false },
        { Icons::Icon::Orbit,       "Orbit",       "Orbit Camera Tool (C)", true },
        { Icons::Icon::Rotate,      "Rotation",    "Rotation Tool (W)", false },
        { Icons::Icon::PanBehind,   "PanBehind",   "Pan Behind (Anchor Point) Tool (Y)", false },
        { Icons::Icon::Rectangle,   "Rectangle",   "Rectangle Tool (Q)", true },
        { Icons::Icon::Pen,         "Pen",         "Pen Tool (G)", true },
        { Icons::Icon::Type,        "Type",        "Horizontal Type Tool (Ctrl+T)", true },
        { Icons::Icon::Brush,       "Brush",       "Brush Tool (Ctrl+B)", true },
        { Icons::Icon::RotoBrush,   "RotoBrush",   "Roto Brush Tool (Alt+W)", true },
        { Icons::Icon::Puppet,      "Puppet",      "Puppet Position Pin Tool (Ctrl+P)", true },
    };
    static int s_activeTool = 0;
    // Group separators after slots 2 (zoom), 5 (pan behind), 7 (pen), 10 (roto brush)
    static const int SEP_AFTER[] = { 2, 5, 7, 10 };

    for (int i = 0; i < 12; ++i) {
        const auto& slot = SLOTS[i];
        const bool active = (i == s_activeTool);
        const ImVec2 btnMin{cx, cy - 13.0f};
        const ImVec2 btnMax{cx + 26.0f, cy + 13.0f};
        const bool hover = ImGui::IsMouseHoveringRect(btnMin, btnMax);

        if (active) {
            dl->AddRectFilled(btnMin, btnMax, C_DEEP, 2.0f);
            dl->AddRect(btnMin, btnMax, C_BLUE, 2.0f);
        } else if (hover) {
            dl->AddRectFilled(btnMin, btnMax, IM_COL32(0x30, 0x30, 0x30, 255), 2.0f);
        }

        // Draw the exact EffectCraft vector icon
        Icons::paintCentered(dl, {cx + 13.0f, cy}, 16.0f, slot.icon, active ? C_TEXT : (hover ? C_TEXT : C_DIM));

        // Flyout corner triangle
        if (slot.hasFlyout) {
            const ImVec2 tri[] = {
                {btnMax.x - 2.0f, btnMax.y - 6.0f},
                {btnMax.x - 2.0f, btnMax.y - 2.0f},
                {btnMax.x - 6.0f, btnMax.y - 2.0f}
            };
            dl->AddConvexPolyFilled(tri, 3, active ? C_TEXT : C_DIM);
        }

        if (hover && ImGui::IsMouseClicked(0)) s_activeTool = i;
        if (hover) ImGui::SetTooltip("%s", slot.tip);

        cx += 28.0f;
        for (int sa : SEP_AFTER) {
            if (i == sa) {
                cx += 2.0f;
                vLine(dl, cx, y + 8.0f, y + h - 8.0f);
                cx += 6.0f;
                break;
            }
        }
    }

    cx += 6.0f;
    vLine(dl, cx, y + 8.0f, y + h - 8.0f);
    cx += 10.0f;

    // Snapping toggle
    static bool s_snap = true;
    {
        const ImVec2 cbMin{cx, cy - 8.0f};
        const ImVec2 cbMax{cx + 16.0f, cy + 8.0f};
        const bool hover = ImGui::IsMouseHoveringRect(cbMin, cbMax);
        dl->AddRectFilled(cbMin, cbMax, s_snap ? C_BLUE : C_WELL, 2.0f);
        dl->AddRect(cbMin, cbMax, C_LINE, 2.0f);
        if (s_snap) {
            const ImVec2 chk[] = { {cx + 4.0f, cy}, {cx + 7.0f, cy + 4.0f}, {cx + 13.0f, cy - 4.0f} };
            dl->AddPolyline(chk, 3, IM_COL32(255, 255, 255, 255), 0, 1.8f);
        }
        if (hover && ImGui::IsMouseClicked(0)) s_snap = !s_snap;

        ImVec2 ts = ImGui::CalcTextSize(L("Snapping"));
        const ImVec2 lblPos{cx + 22.0f, cy - ts.y * 0.5f};
        dl->AddText(lblPos, s_snap ? C_TEXT : C_DIM, L("Snapping"));
        cx += 26.0f + ts.x;
    }

    // Workspaces tabs on right
    // after EffectCraft crates/ui-egui/src/header.rs workspace_tabs
    static const char* WS_NAMES[] = { "Default", "Review", "Learn", "Small Screen", "Standard" };
    static int s_wsIndex = 0;
    float rx = x + w - 12.0f;

    // "»" workspace overflow menu button
    {
        const ImVec2 moreMin{rx - 20.0f, cy - 11.0f};
        const ImVec2 moreMax{rx, cy + 11.0f};
        const bool hover = ImGui::IsMouseHoveringRect(moreMin, moreMax);
        dl->AddText({rx - 15.0f, cy - 7.0f}, hover ? C_TEXT : C_DIM, "\xc2\xbb"); // »
        rx -= 26.0f;
    }

    // Workspaces drawn right to left: Standard, Small Screen, Learn, Review, Default
    for (int i = 4; i >= 0; --i) {
        ImVec2 ts = ImGui::CalcTextSize(WS_NAMES[i]);
        const float tw = ts.x + 16.0f;
        rx -= tw;
        const ImVec2 tabMin{rx, cy - 12.0f};
        const ImVec2 tabMax{rx + tw, cy + 12.0f};
        const bool active = (i == s_wsIndex);
        const bool hover = ImGui::IsMouseHoveringRect(tabMin, tabMax);

        dl->AddText({rx + 8.0f, cy - ts.y * 0.5f},
                    active ? C_BLUE : (hover ? C_TEXT : C_DIM),
                    WS_NAMES[i]);
        if (active) {
            dl->AddLine({rx + 8.0f, y + h - 3.0f}, {rx + tw - 8.0f, y + h - 3.0f}, C_BLUE, 2.0f);
        }
        if (hover && ImGui::IsMouseClicked(0)) s_wsIndex = i;
        rx -= 4.0f;
    }
}

// ---------- main draw ----------
void draw(Project::Project* proj, const Actions& actions) {
    g_actions = &actions;
    advancePlayback(proj);
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space))
        s_playing = !s_playing;

    // Keyboard shortcuts: Ctrl+N/O/S/Q, Ctrl+Shift+S
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_N) && actions.onNew)    actions.onNew();
        if (ImGui::IsKeyPressed(ImGuiKey_O) && actions.onOpen)   actions.onOpen();
        if (ImGui::IsKeyPressed(ImGuiKey_S) && actions.onSave)   actions.onSave();
        if (ImGui::IsKeyPressed(ImGuiKey_Q) && actions.onQuit)   actions.onQuit();
    }
    if (io.KeyCtrl && io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_S) && actions.onSaveAs) actions.onSaveAs();
    }

    // Full-window root
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_APP_BG));
    ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_MenuBar;
    ImGui::Begin("##root", nullptr, rootFlags);
    ImGui::PopStyleColor();

    // ----- Menu bar -----
    // after EffectCraft crates/ui-egui/src/menus/: File, Edit, Composition, Layer, Effect,
    // Animation, View, Window, Help
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(L("File"))) {
            if (ImGui::MenuItem(L("New Composition..."),    "Ctrl+N"))  if (actions.onNew)    actions.onNew();
            if (ImGui::MenuItem(L("Open..."),              "Ctrl+O"))  if (actions.onOpen)   actions.onOpen();
            if (ImGui::MenuItem(L("Save"),                 "Ctrl+S"))  if (actions.onSave)   actions.onSave();
            if (ImGui::MenuItem(L("Save As..."),    "Ctrl+Shift+S"))   if (actions.onSaveAs) actions.onSaveAs();
            ImGui::Separator();
            if (ImGui::MenuItem(L("Quit"),                 "Ctrl+Q"))  if (actions.onQuit)   actions.onQuit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Edit"))) {
            ImGui::MenuItem(L("Undo"),           "Ctrl+Z");
            ImGui::MenuItem(L("Redo"),     "Ctrl+Shift+Z");
            ImGui::Separator();
            ImGui::MenuItem(L("Cut"),            "Ctrl+X");
            ImGui::MenuItem(L("Copy"),           "Ctrl+C");
            ImGui::MenuItem(L("Paste"),          "Ctrl+V");
            ImGui::MenuItem(L("Duplicate"),      "Ctrl+D");
            ImGui::MenuItem(L("Select All"),     "Ctrl+A");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Composition"))) {
            ImGui::MenuItem(L("New Composition..."));
            ImGui::MenuItem(L("Composition Settings..."),  "Ctrl+K");
            ImGui::Separator();
            ImGui::MenuItem(L("Add to Render Queue"),      "Ctrl+M");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Layer"))) {
            ImGui::MenuItem(L("New Solid..."),          "Ctrl+Y");
            const bool canCreateLayer = proj && proj->activeComp >= 0 &&
                static_cast<size_t>(proj->activeComp) < proj->compositions.size();
            if (ImGui::MenuItem(L("New Shape Layer"), nullptr, false, canCreateLayer))
                createShapeLayer(proj);
            ImGui::MenuItem(L("New Text Layer"),        "Ctrl+Alt+T");
            ImGui::MenuItem(L("New Null Object"),       "Ctrl+Alt+Shift+Y");
            ImGui::MenuItem(L("New Camera..."),         "Ctrl+Alt+Shift+C");
            ImGui::MenuItem(L("New Light..."),          "Ctrl+Alt+Shift+L");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Effect"))) {
            ImGui::MenuItem(L("Effect Controls"),       "F3");
            ImGui::Separator();
            ImGui::MenuItem(L("(Effects in phase 7)"));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Animation"))) {
            ImGui::MenuItem(L("Add Keyframe"));
            ImGui::MenuItem(L("Toggle Hold Keyframe"), "Ctrl+Alt+H");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("View"))) {
            ImGui::MenuItem(L("Zoom In"),  "=");
            ImGui::MenuItem(L("Zoom Out"), "-");
            ImGui::Separator();
            ImGui::MenuItem(L("Snapping"), "Shift+/");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Window"))) {
            ImGui::MenuItem(L("Project"),          "Ctrl+0");
            ImGui::MenuItem(L("Effect Controls"),  "F3");
            ImGui::MenuItem(L("Preview"),          "Ctrl+3");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Help"))) {
            ImGui::MenuItem(L("About Morpheus..."));
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Client area geometry
    const ImVec2 cPos  = ImGui::GetCursorScreenPos();
    const float  cW    = ImGui::GetContentRegionAvail().x;
    const float  cH    = ImGui::GetContentRegionAvail().y;

    // ----- Header / Tools bar (full width, HEADER_H px) -----
    // after EffectCraft crates/ui-egui/src/header.rs show()
    {
        ImGui::SetNextWindowPos({cPos.x, cPos.y});
        ImGui::BeginChild("##header", {cW, HEADER_H}, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 hp = ImGui::GetWindowPos();
        drawHeader(hp.x, hp.y, cW, HEADER_H);
        ImGui::EndChild();
    }

    // Advance past the header
    const float panelY  = cPos.y + HEADER_H + 1;
    const float panelH  = cH - HEADER_H - 1;

    // Centre column width (flex)
    const float centreW = cW - LEFT_W - RIGHT_W - 2;

    // Centre split heights
    const float centreTopH = panelH * SPLIT_Y;
    const float centreBotH = panelH - centreTopH - 1;

    // Right split heights
    const float rightTopH = PREVIEW_H;
    const float rightBotH = panelH - rightTopH - 1;

    // ===== Left column: Project / Effect Controls =====
    // after EffectCraft crates/ui-egui/src/dock.rs Default workspace: tabs([Project, EffectControls])
    {
        ImGui::SetNextWindowPos({cPos.x, panelY});
        ImGui::BeginChild("##leftcol", {LEFT_W, panelH}, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 lp = ImGui::GetWindowPos();
        ImDrawList* ldl = ImGui::GetWindowDrawList();
        ldl->AddRectFilled(lp, {lp.x+LEFT_W, lp.y+panelH}, C_PANEL);

        // Tab strip: Project | Effect Controls
        const char* leftTabs[] = { L("Project"), L("Effect Controls") };
        s_leftTab = tabStrip(leftTabs, 2, s_leftTab,
                             lp.x, lp.y, LEFT_W, TAB_H, ldl, ImGui::GetIO());
        panelMenuButton(lp.x+LEFT_W-2, lp.y+TAB_H*0.5f, ldl);

        // Panel content
        ImGui::SetCursorScreenPos({lp.x, lp.y+TAB_H+2});
        ImGui::BeginChild("##leftcontent", {LEFT_W, panelH-TAB_H-2}, false);
        if (s_leftTab == 0) drawProjectPanel(proj);
        else                drawEffectControlsPanel(proj);
        ImGui::EndChild();
        ImGui::EndChild();
    }

    // 1-px vertical divider
    ImGui::GetForegroundDrawList()->AddLine(
        {cPos.x+LEFT_W, panelY},
        {cPos.x+LEFT_W, panelY+panelH},
        C_LINE, DIVIDER);

    // ===== Centre column =====
    // after EffectCraft crates/ui-egui/src/dock.rs Default workspace:
    //   vsplit(Ratio(0.56), tabs([Composition,Layer]), tabs([Timeline,RenderQueue]))
    {
        const float cx0 = cPos.x + LEFT_W + 1;
        ImGui::SetNextWindowPos({cx0, panelY});
        ImGui::BeginChild("##centrecol", {centreW, panelH}, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 cp = ImGui::GetWindowPos();
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddRectFilled(cp, {cp.x+centreW, cp.y+panelH}, C_PANEL);

        // --- Centre top: Composition / Layer viewer tabs ---
        const char* centreTopTabs[] = { L("Composition"), L("Layer") };
        s_centreTop = tabStrip(centreTopTabs, 2, s_centreTop,
                               cp.x, cp.y, centreW, TAB_H, cdl, ImGui::GetIO());
        // Comp breadcrumb path
        if (proj && proj->activeComp >= 0) {
            const auto& comp = proj->compositions[(size_t)proj->activeComp];
            char crumb[256]; snprintf(crumb, sizeof crumb, " %s", comp.name.c_str());
            const ImVec2 crs = ImGui::CalcTextSize(crumb);
            cdl->AddText({cp.x+centreW*0.4f, cp.y+(TAB_H-crs.y)*0.5f}, C_DIM, crumb);
        }
        panelMenuButton(cp.x+centreW-2, cp.y+TAB_H*0.5f, cdl);

        ImGui::SetCursorScreenPos({cp.x, cp.y+TAB_H});
        ImGui::BeginChild("##centretop", {centreW, centreTopH-TAB_H}, false);
        if (s_centreTop == 0) drawCompositionViewer(proj);
        ImGui::EndChild();

        hLine(cdl, cp.x, cp.x+centreW, cp.y+centreTopH);

        // --- Centre bottom: Timeline / Render Queue tabs ---
        const char* centreBotTabs[] = { L("Timeline"), L("Render Queue") };
        s_centreBot = tabStrip(centreBotTabs, 2, s_centreBot,
                               cp.x, cp.y+centreTopH+1, centreW, TAB_H, cdl, ImGui::GetIO());
        panelMenuButton(cp.x+centreW-2, cp.y+centreTopH+1+TAB_H*0.5f, cdl);

        ImGui::SetCursorScreenPos({cp.x, cp.y+centreTopH+1+TAB_H});
        ImGui::BeginChild("##centrebot", {centreW, centreBotH-TAB_H-1}, false);
        if (s_centreBot == 0) drawTimeline(proj);
        else                  drawRenderQueue();
        ImGui::EndChild();

        ImGui::EndChild();
    }

    // 1-px vertical divider
    ImGui::GetForegroundDrawList()->AddLine(
        {cPos.x+LEFT_W+centreW+1, panelY},
        {cPos.x+LEFT_W+centreW+1, panelY+panelH},
        C_LINE, DIVIDER);

    // ===== Right column =====
    // after EffectCraft crates/ui-egui/src/dock.rs Default workspace:
    //   vsplit(FixedFirst(PREVIEW_FRAME), tabs([Preview]), tabs([Properties,Align,Audio,EffectsPresets]))
    {
        const float rx0 = cPos.x + LEFT_W + centreW + 2;
        ImGui::SetNextWindowPos({rx0, panelY});
        ImGui::BeginChild("##rightcol", {RIGHT_W, panelH}, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 rp = ImGui::GetWindowPos();
        ImDrawList* rdl = ImGui::GetWindowDrawList();
        rdl->AddRectFilled(rp, {rp.x+RIGHT_W, rp.y+panelH}, C_PANEL);

        // --- Right top: Preview (tabs: Preview / Info / Audio) ---
        const char* rightTopTabs[] = { L("Preview"), L("Info"), L("Audio") };
        static int s_previewTab = 0;
        s_previewTab = tabStrip(rightTopTabs, 3, s_previewTab,
                                rp.x, rp.y, RIGHT_W, TAB_H, rdl, ImGui::GetIO());
        panelMenuButton(rp.x+RIGHT_W-2, rp.y+TAB_H*0.5f, rdl);

        ImGui::SetCursorScreenPos({rp.x, rp.y+TAB_H});
        ImGui::BeginChild("##righttop", {RIGHT_W, rightTopH-TAB_H}, false);
        if (s_previewTab == 0)              drawPreviewPanel(proj);
        else if (s_previewTab == 1) drawInfoPanel(proj);
        else                        ImGui::TextDisabled(L("Audio meters in phase 9."));
        ImGui::EndChild();

        hLine(rdl, rp.x, rp.x+RIGHT_W, rp.y+rightTopH);

        // --- Right bottom: Properties / Align / Audio / Effects & Presets ---
        const char* rightBotTabs[] = {
            L("Properties"), L("Align"), L("Audio"), L("Effects & Presets") };
        s_rightBot = tabStrip(rightBotTabs, 4, s_rightBot,
                              rp.x, rp.y+rightTopH+1, RIGHT_W, TAB_H, rdl, ImGui::GetIO());
        panelMenuButton(rp.x+RIGHT_W-2, rp.y+rightTopH+1+TAB_H*0.5f, rdl);

        ImGui::SetCursorScreenPos({rp.x, rp.y+rightTopH+1+TAB_H});
        ImGui::BeginChild("##rightbot", {RIGHT_W, rightBotH-TAB_H-1}, false);
        if (s_rightBot == 0)      drawPropertiesPanel(proj);
        else if (s_rightBot == 1) ImGui::TextDisabled(L("Align panel in phase 1."));
        else if (s_rightBot == 2) ImGui::TextDisabled(L("Audio panel in phase 9."));
        else                      ImGui::TextDisabled(L("Effects & Presets in phase 7."));
        ImGui::EndChild();

        ImGui::EndChild();
    }

    ImGui::End();
}

}  // namespace Panels
