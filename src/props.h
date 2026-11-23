// Morpheus - property tree: groups, properties, and nodes addressed by paths.
// Ported function-by-function from EffectCraft crates/project/src/props.rs and build.rs.
// after EffectCraft (crates/project/src/props.rs, crates/project/src/build.rs)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "time_.h"
#include "keyframe.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Props {

using Project::Tick;
using Uid = uint64_t;
using KeyframeModel::Value;
using KeyframeModel::Keyframe;
using KeyframeModel::ShapePath;
using KeyframeModel::Gradient;

// ---- ParamUi ---------------------------------------------------------
// after EffectCraft crates/project/src/props.rs ParamUi
enum class ParamUiKind {
    Number,
    Slider,
    Percent,
    Angle,
    Pixels,
    Point,
    Point3,
    Color,
    Checkbox,
    Popup,
    Layer,
    Mask,
    Path,
    Text,
    File,
    Gradient,
    Hidden
};

struct ParamUi {
    ParamUiKind kind = ParamUiKind::Number;
    double min        = 0.0;
    double max        = 100.0;
    double slider_min = 0.0;
    double slider_max = 100.0;
    uint8_t decimals  = 0;
    std::vector<std::string> options;
    std::vector<std::string> filters;

    static ParamUi make_slider(double min, double max, double smin, double smax, uint8_t decimals = 0) {
        ParamUi ui;
        ui.kind = ParamUiKind::Slider;
        ui.min = min; ui.max = max;
        ui.slider_min = smin; ui.slider_max = smax;
        ui.decimals = decimals;
        return ui;
    }

    static ParamUi make_popup(const std::vector<std::string>& opts) {
        ParamUi ui;
        ui.kind = ParamUiKind::Popup;
        ui.options = opts;
        return ui;
    }
};

// ---- Expression ------------------------------------------------------
// after EffectCraft crates/project/src/props.rs Expression
struct Expression {
    std::string text;
    bool enabled = false;

    bool operator==(const Expression& o) const {
        return text == o.text && enabled == o.enabled;
    }
};

// ---- Property --------------------------------------------------------
// after EffectCraft crates/project/src/props.rs Property
struct Property {
    Uid uid = 0;
    std::string match_id;
    std::string name;
    Value value;
    std::vector<Keyframe> keys;
    std::optional<Expression> expr;
    ParamUi ui;
    bool spatial       = false;
    bool hold_only     = false;
    bool three_d_only  = false;
    bool two_d_only    = false;
    uint8_t shown_dims = 0;
    bool static_only   = false;

    Property() = default;
    Property(Uid u, const std::string& m, const std::string& n, Value v);

    Property& with_ui(ParamUi u) { ui = std::move(u); return *this; }
    Property& set_spatial()      { spatial = true; return *this; }

    bool is_animated() const { return !keys.empty(); }
    bool has_expression() const {
        return expr.has_value() && expr->enabled && !expr->text.empty();
    }

    Value value_at(Tick t) const;
    void set_value_at(Tick t, Value v);
    void set_animated(bool on, Tick t);

    bool operator==(const Property& o) const;
    bool operator!=(const Property& o) const { return !(*this == o); }
};

// ---- MaskMode / MaskMotionBlur / FeatherFalloff ----------------------
// after EffectCraft crates/project/src/props.rs MaskMode, MaskMotionBlur, FeatherFalloff
enum class MaskMode {
    None,
    Add,
    Subtract,
    Intersect,
    Lighten,
    Darken,
    Difference
};

static inline const char* mask_mode_label(MaskMode m) {
    switch (m) {
    case MaskMode::None:       return "None";
    case MaskMode::Add:        return "Add";
    case MaskMode::Subtract:   return "Subtract";
    case MaskMode::Intersect:  return "Intersect";
    case MaskMode::Lighten:    return "Lighten";
    case MaskMode::Darken:     return "Darken";
    case MaskMode::Difference: return "Difference";
    }
    return "Add";
}

std::optional<MaskMode> mask_mode_from_name(const std::string& s);

enum class MaskMotionBlur {
    SameAsLayer,
    On,
    Off
};

enum class FeatherFalloff {
    Smooth,
    Linear
};

// ---- GroupKind -------------------------------------------------------
// after EffectCraft crates/project/src/props.rs GroupKind
enum class GroupKindTag {
    Plain,
    Indexed,
    Mask,
    Effect,
    Tracker,
    Essential
};

struct GroupKind {
    GroupKindTag tag = GroupKindTag::Plain;
    // For Mask
    MaskMode mask_mode             = MaskMode::Add;
    bool mask_inverted             = false;
    uint8_t mask_color[3]          = {255, 0, 0};
    bool mask_locked               = false;
    MaskMotionBlur mask_motion_blur = MaskMotionBlur::SameAsLayer;
    FeatherFalloff mask_falloff    = FeatherFalloff::Smooth;
    bool mask_roto_bezier          = false;
    // For Effect
    std::string effect_id;

    bool operator==(const GroupKind& o) const;
};

// Forward declare PropGroup and Node
struct PropGroup;

enum class NodeKind {
    Prop,
    Group
};

// ---- Node ------------------------------------------------------------
// after EffectCraft crates/project/src/props.rs Node
struct Node {
    NodeKind kind = NodeKind::Prop;
    std::unique_ptr<Property> prop_data;
    std::unique_ptr<PropGroup> group_data;

    Node();
    Node(Property p);
    Node(PropGroup g);
    Node(const Node& o);
    Node(Node&& o) noexcept;
    Node& operator=(const Node& o);
    Node& operator=(Node&& o) noexcept;
    ~Node();

    Uid uid() const;
    const std::string& match_id() const;
    const std::string& name() const;

    const Property* as_prop() const;
    Property* as_prop_mut();
    const PropGroup* as_group() const;
    PropGroup* as_group_mut();

    bool operator==(const Node& o) const;
    bool operator!=(const Node& o) const { return !(*this == o); }
};

// ---- Seg & parse_path ------------------------------------------------
// after EffectCraft crates/project/src/props.rs Seg, parse_path
enum class SegKind {
    Uid,
    Match,
    Index
};

struct Seg {
    SegKind kind = SegKind::Match;
    Uid uid_val  = 0;
    std::string match_str;
    size_t count = 1;
    size_t index_val = 0;
};

std::vector<Seg> parse_path(const std::string& s);

// ---- PropGroup -------------------------------------------------------
// after EffectCraft crates/project/src/props.rs PropGroup
struct PropGroup {
    Uid uid = 0;
    std::string match_id;
    std::string name;
    GroupKind kind;
    bool enabled = true;
    uint8_t label = 0; // 0 = none
    std::vector<Node> children;

    PropGroup() = default;
    PropGroup(Uid u, const std::string& m, const std::string& n)
        : uid(u), match_id(m), name(n) {}

    bool is_instance() const {
        return kind.tag == GroupKindTag::Indexed || kind.tag == GroupKindTag::Effect ||
               kind.tag == GroupKindTag::Mask || kind.tag == GroupKindTag::Tracker;
    }

    PropGroup& with(Node n) {
        children.push_back(std::move(n));
        return *this;
    }
    PropGroup& with(Property p) {
        children.push_back(Node(std::move(p)));
        return *this;
    }
    PropGroup& with(PropGroup g) {
        children.push_back(Node(std::move(g)));
        return *this;
    }

    const Node* node(const std::string& path) const;
    Node* node_mut(const std::string& path);

    const Property* prop(const std::string& path) const;
    Property* prop_mut(const std::string& path);

    const PropGroup* group(const std::string& path) const;
    PropGroup* group_mut(const std::string& path);

    const Property* get(const std::string& match_id) const;
    Property* get_mut(const std::string& match_id);

    const PropGroup* sub(const std::string& match_id) const;
    PropGroup* sub_mut(const std::string& match_id);

    std::optional<size_t> child_index(const Seg& seg) const;

    const Property* find(Uid u) const;
    Property* find_mut(Uid u);
    const PropGroup* find_group(Uid u) const;
    PropGroup* find_group_mut(Uid u);
    const PropGroup* parent_of(Uid u) const;
    PropGroup* parent_of_mut(Uid u);

    std::optional<std::string> path_of(Uid u) const;
    std::optional<std::string> match_path_of(Uid u) const;
    std::optional<std::string> name_path_of(Uid u) const;
    std::vector<const Node*> node_chain(Uid u) const;

    bool any_animated() const;
    void reassign_uids(Uid& next);
    Uid max_uid() const;

    bool operator==(const PropGroup& o) const;
    bool operator!=(const PropGroup& o) const { return !(*this == o); }
};

// ---- Ids allocator ---------------------------------------------------
// after EffectCraft crates/project/src/build.rs Ids
struct Ids {
    uint64_t* counter = nullptr;
    explicit Ids(uint64_t* c) : counter(c) {}

    uint64_t alloc() {
        uint64_t v = *counter;
        *counter += 1;
        return v;
    }
    Property prop(const std::string& m, const std::string& name, Value v) {
        return Property(alloc(), m, name, std::move(v));
    }
    PropGroup group(const std::string& m, const std::string& name) {
        return PropGroup(alloc(), m, name);
    }
};

// ---- Standard Builders -----------------------------------------------
// after EffectCraft crates/project/src/build.rs
namespace Build {

PropGroup transform(Ids& ids, std::array<double, 2> anchor, std::array<double, 2> position);
PropGroup masks(Ids& ids);
PropGroup effects(Ids& ids);
PropGroup mask(Ids& ids, const std::string& name, ShapePath path, MaskMode mode, std::array<uint8_t, 3> color);
PropGroup shapeContents(Ids& ids);
PropGroup shapeTransform(Ids& ids);
PropGroup shapeGroup(Ids& ids, const std::string& name, std::vector<PropGroup> items);
PropGroup shapeRect(Ids& ids, std::array<double, 2> size, std::array<double, 2> position, double roundness);
PropGroup shapeEllipse(Ids& ids, std::array<double, 2> size, std::array<double, 2> position);
PropGroup shapePath(Ids& ids, ShapePath path);
PropGroup shapeFill(Ids& ids, std::array<double, 4> color);
PropGroup shapeStroke(Ids& ids, std::array<double, 4> color, double width);
PropGroup shapeTrim(Ids& ids, double start, double end, double offset);
PropGroup shapeStrokeTaper(Ids& ids);
PropGroup shapeStrokeWave(Ids& ids);
PropGroup path_options(Ids& ids);
PropGroup more_options(Ids& ids);
std::optional<PropGroup> shapeItem(Ids& ids, const std::string& kind);
std::optional<Uid> addShapeItem(Ids& ids, PropGroup& contents, const std::string& kind,
                                Uid parentGroup = 0);

} // namespace Build

} // namespace Props
