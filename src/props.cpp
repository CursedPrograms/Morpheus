// Morpheus - property tree implementation.
// Ported function-by-function from EffectCraft crates/project/src/props.rs and build.rs.
// after EffectCraft (crates/project/src/props.rs, crates/project/src/build.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "props.h"

#include <cctype>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Props {

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
            return false;
    }
    return true;
}

// =====================================================================
// Property  (after EffectCraft crates/project/src/props.rs Property)
// =====================================================================

Property::Property(Uid u, const std::string& m, const std::string& n, Value v)
    : uid(u), match_id(m), name(n), value(std::move(v)) {
    hold_only = !value.interpolates();
}

Value Property::value_at(Tick t) const {
    if (keys.empty()) {
        return value;
    }
    if (match_id == "orientation" && value.kind == KeyframeModel::ValueKind::Vec3) {
        Value out;
        if (KeyframeModel::evaluate_orientation(keys, t, out)) return out;
        return value;
    }
    Value out;
    if (KeyframeModel::evaluate(keys, t, spatial, out)) return out;
    return value;
}

void Property::set_value_at(Tick t, Value v) {
    if (keys.empty()) {
        value = std::move(v);
    } else {
        Keyframe k(t, std::move(v));
        if (hold_only) k = k.hold();
        KeyframeModel::set_key(keys, k);
    }
}

void Property::set_animated(bool on, Tick t) {
    if (on && keys.empty()) {
        Keyframe k(t, value);
        if (hold_only) k = k.hold();
        keys.push_back(k);
    } else if (!on && !keys.empty()) {
        value = value_at(t);
        keys.clear();
    }
}

bool Property::operator==(const Property& o) const {
    return uid == o.uid && match_id == o.match_id && name == o.name &&
           value == o.value && keys == o.keys && expr == o.expr &&
           spatial == o.spatial && hold_only == o.hold_only &&
           three_d_only == o.three_d_only && two_d_only == o.two_d_only &&
           shown_dims == o.shown_dims && static_only == o.static_only;
}

// =====================================================================
// MaskMode helpers  (after EffectCraft crates/project/src/props.rs)
// =====================================================================

std::optional<MaskMode> mask_mode_from_name(const std::string& s) {
    static const MaskMode ALL[] = {
        MaskMode::None, MaskMode::Add, MaskMode::Subtract, MaskMode::Intersect,
        MaskMode::Lighten, MaskMode::Darken, MaskMode::Difference
    };
    for (auto m : ALL) {
        if (iequals(mask_mode_label(m), s)) return m;
    }
    return std::nullopt;
}

bool GroupKind::operator==(const GroupKind& o) const {
    if (tag != o.tag) return false;
    if (tag == GroupKindTag::Mask) {
        return mask_mode == o.mask_mode && mask_inverted == o.mask_inverted &&
               mask_color[0] == o.mask_color[0] && mask_color[1] == o.mask_color[1] &&
               mask_color[2] == o.mask_color[2] && mask_locked == o.mask_locked &&
               mask_motion_blur == o.mask_motion_blur && mask_falloff == o.mask_falloff &&
               mask_roto_bezier == o.mask_roto_bezier;
    }
    if (tag == GroupKindTag::Effect) {
        return effect_id == o.effect_id;
    }
    return true;
}

// =====================================================================
// Node  (after EffectCraft crates/project/src/props.rs Node)
// =====================================================================

Node::Node() : kind(NodeKind::Prop), prop_data(std::make_unique<Property>()) {}

Node::Node(Property p) : kind(NodeKind::Prop), prop_data(std::make_unique<Property>(std::move(p))) {}

Node::Node(PropGroup g) : kind(NodeKind::Group), group_data(std::make_unique<PropGroup>(std::move(g))) {}

Node::Node(const Node& o) : kind(o.kind) {
    if (o.kind == NodeKind::Prop && o.prop_data) {
        prop_data = std::make_unique<Property>(*o.prop_data);
    } else if (o.kind == NodeKind::Group && o.group_data) {
        group_data = std::make_unique<PropGroup>(*o.group_data);
    }
}

Node::Node(Node&& o) noexcept = default;

Node& Node::operator=(const Node& o) {
    if (this == &o) return *this;
    kind = o.kind;
    if (o.kind == NodeKind::Prop && o.prop_data) {
        prop_data = std::make_unique<Property>(*o.prop_data);
        group_data.reset();
    } else if (o.kind == NodeKind::Group && o.group_data) {
        group_data = std::make_unique<PropGroup>(*o.group_data);
        prop_data.reset();
    } else {
        prop_data.reset();
        group_data.reset();
    }
    return *this;
}

Node& Node::operator=(Node&& o) noexcept = default;

Node::~Node() = default;

Uid Node::uid() const {
    if (kind == NodeKind::Prop && prop_data) return prop_data->uid;
    if (kind == NodeKind::Group && group_data) return group_data->uid;
    return 0;
}

const std::string& Node::match_id() const {
    static const std::string empty;
    if (kind == NodeKind::Prop && prop_data) return prop_data->match_id;
    if (kind == NodeKind::Group && group_data) return group_data->match_id;
    return empty;
}

const std::string& Node::name() const {
    static const std::string empty;
    if (kind == NodeKind::Prop && prop_data) return prop_data->name;
    if (kind == NodeKind::Group && group_data) return group_data->name;
    return empty;
}

const Property* Node::as_prop() const {
    return kind == NodeKind::Prop ? prop_data.get() : nullptr;
}

Property* Node::as_prop_mut() {
    return kind == NodeKind::Prop ? prop_data.get() : nullptr;
}

const PropGroup* Node::as_group() const {
    return kind == NodeKind::Group ? group_data.get() : nullptr;
}

PropGroup* Node::as_group_mut() {
    return kind == NodeKind::Group ? group_data.get() : nullptr;
}

bool Node::operator==(const Node& o) const {
    if (kind != o.kind) return false;
    if (kind == NodeKind::Prop) {
        if (!prop_data || !o.prop_data) return prop_data == o.prop_data;
        return *prop_data == *o.prop_data;
    } else {
        if (!group_data || !o.group_data) return group_data == o.group_data;
        return *group_data == *o.group_data;
    }
}

// =====================================================================
// Path parsing  (after EffectCraft crates/project/src/props.rs parse_path)
// =====================================================================

static std::optional<Uid> bare_uid(const std::string& path) {
    if (path.empty() || path[0] != '@') return std::nullopt;
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '/' || path[i] == '.') return std::nullopt;
        if (!std::isdigit((unsigned char)path[i])) return std::nullopt;
    }
    return std::stoull(path.substr(1));
}

std::vector<Seg> parse_path(const std::string& s) {
    std::vector<Seg> segs;
    size_t start = 0;
    while (start < s.size()) {
        while (start < s.size() && (s[start] == '/' || s[start] == '.')) ++start;
        if (start >= s.size()) break;
        size_t end = start;
        while (end < s.size() && s[end] != '/' && s[end] != '.') ++end;
        std::string token = s.substr(start, end - start);
        start = end;
        if (token.empty()) continue;

        if (token[0] == '@') {
            try {
                Seg seg; seg.kind = SegKind::Uid;
                seg.uid_val = std::stoull(token.substr(1));
                segs.push_back(seg);
            } catch (...) {}
        } else if (token[0] == '#') {
            try {
                Seg seg; seg.kind = SegKind::Index;
                seg.index_val = std::stoull(token.substr(1));
                segs.push_back(seg);
            } catch (...) {}
        } else {
            size_t hash_pos = token.rfind('#');
            if (hash_pos != std::string::npos && hash_pos + 1 < token.size()) {
                bool all_digits = true;
                for (size_t k = hash_pos + 1; k < token.size(); ++k) {
                    if (!std::isdigit((unsigned char)token[k])) { all_digits = false; break; }
                }
                if (all_digits) {
                    Seg seg; seg.kind = SegKind::Match;
                    seg.match_str = token.substr(0, hash_pos);
                    seg.count = std::stoull(token.substr(hash_pos + 1));
                    segs.push_back(seg);
                    continue;
                }
            }
            Seg seg; seg.kind = SegKind::Match;
            seg.match_str = token;
            seg.count = 1;
            segs.push_back(seg);
        }
    }
    return segs;
}

// =====================================================================
// PropGroup  (after EffectCraft crates/project/src/props.rs PropGroup)
// =====================================================================

std::optional<size_t> PropGroup::child_index(const Seg& seg) const {
    switch (seg.kind) {
    case SegKind::Uid: {
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i].uid() == seg.uid_val) return i;
        }
        return std::nullopt;
    }
    case SegKind::Index: {
        if (seg.index_val >= 1 && seg.index_val <= children.size()) {
            return seg.index_val - 1;
        }
        return std::nullopt;
    }
    case SegKind::Match: {
        size_t seen = 0;
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i].match_id() == seg.match_str || iequals(children[i].name(), seg.match_str)) {
                seen += 1;
                if (seen == seg.count) return i;
            }
        }
        return std::nullopt;
    }
    }
    return std::nullopt;
}

const Node* PropGroup::node(const std::string& path) const {
    auto segs = parse_path(path);
    if (segs.empty()) return nullptr;
    const PropGroup* g = this;
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        auto idx = g->child_index(segs[i]);
        if (!idx.has_value()) return nullptr;
        g = g->children[*idx].as_group();
        if (!g) return nullptr;
    }
    auto last_idx = g->child_index(segs.back());
    if (!last_idx.has_value()) return nullptr;
    return &g->children[*last_idx];
}

Node* PropGroup::node_mut(const std::string& path) {
    auto segs = parse_path(path);
    if (segs.empty()) return nullptr;
    PropGroup* g = this;
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        auto idx = g->child_index(segs[i]);
        if (!idx.has_value()) return nullptr;
        g = g->children[*idx].as_group_mut();
        if (!g) return nullptr;
    }
    auto last_idx = g->child_index(segs.back());
    if (!last_idx.has_value()) return nullptr;
    return &g->children[*last_idx];
}

const Property* PropGroup::prop(const std::string& path) const {
    auto bu = bare_uid(path);
    if (bu.has_value()) return find(*bu);
    const Node* n = node(path);
    return n ? n->as_prop() : nullptr;
}

Property* PropGroup::prop_mut(const std::string& path) {
    auto bu = bare_uid(path);
    if (bu.has_value()) return find_mut(*bu);
    Node* n = node_mut(path);
    return n ? n->as_prop_mut() : nullptr;
}

const PropGroup* PropGroup::group(const std::string& path) const {
    const Node* n = node(path);
    return n ? n->as_group() : nullptr;
}

PropGroup* PropGroup::group_mut(const std::string& path) {
    Node* n = node_mut(path);
    return n ? n->as_group_mut() : nullptr;
}

const Property* PropGroup::get(const std::string& match_id_val) const {
    for (const auto& c : children) {
        if (const auto* p = c.as_prop()) {
            if (p->match_id == match_id_val) return p;
        }
    }
    return nullptr;
}

Property* PropGroup::get_mut(const std::string& match_id_val) {
    for (auto& c : children) {
        if (auto* p = c.as_prop_mut()) {
            if (p->match_id == match_id_val) return p;
        }
    }
    return nullptr;
}

const PropGroup* PropGroup::sub(const std::string& match_id_val) const {
    for (const auto& c : children) {
        if (const auto* g = c.as_group()) {
            if (g->match_id == match_id_val) return g;
        }
    }
    return nullptr;
}

PropGroup* PropGroup::sub_mut(const std::string& match_id_val) {
    for (auto& c : children) {
        if (auto* g = c.as_group_mut()) {
            if (g->match_id == match_id_val) return g;
        }
    }
    return nullptr;
}

const Property* PropGroup::find(Uid u) const {
    for (const auto& c : children) {
        if (const auto* p = c.as_prop()) {
            if (p->uid == u) return p;
        } else if (const auto* g = c.as_group()) {
            if (const auto* p = g->find(u)) return p;
        }
    }
    return nullptr;
}

Property* PropGroup::find_mut(Uid u) {
    for (auto& c : children) {
        if (auto* p = c.as_prop_mut()) {
            if (p->uid == u) return p;
        } else if (auto* g = c.as_group_mut()) {
            if (auto* p = g->find_mut(u)) return p;
        }
    }
    return nullptr;
}

const PropGroup* PropGroup::find_group(Uid u) const {
    if (uid == u) return this;
    for (const auto& c : children) {
        if (const auto* g = c.as_group()) {
            if (const auto* found = g->find_group(u)) return found;
        }
    }
    return nullptr;
}

PropGroup* PropGroup::find_group_mut(Uid u) {
    if (uid == u) return this;
    for (auto& c : children) {
        if (auto* g = c.as_group_mut()) {
            if (auto* found = g->find_group_mut(u)) return found;
        }
    }
    return nullptr;
}

const PropGroup* PropGroup::parent_of(Uid u) const {
    for (const auto& c : children) {
        if (c.uid() == u) return this;
    }
    for (const auto& c : children) {
        if (const auto* g = c.as_group()) {
            if (const auto* p = g->parent_of(u)) return p;
        }
    }
    return nullptr;
}

PropGroup* PropGroup::parent_of_mut(Uid u) {
    for (auto& c : children) {
        if (c.uid() == u) return this;
    }
    for (auto& c : children) {
        if (auto* g = c.as_group_mut()) {
            if (auto* p = g->parent_of_mut(u)) return p;
        }
    }
    return nullptr;
}

std::optional<std::string> PropGroup::path_of(Uid u) const {
    for (const auto& c : children) {
        if (c.uid() == u) {
            return "@" + std::to_string(u);
        }
        if (const auto* g = c.as_group()) {
            auto sub_path = g->path_of(u);
            if (sub_path.has_value()) {
                return "@" + std::to_string(g->uid) + "/" + *sub_path;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> PropGroup::match_path_of(Uid u) const {
    for (size_t i = 0; i < children.size(); ++i) {
        const auto& c = children[i];
        size_t count = 0;
        for (size_t k = 0; k <= i; ++k) {
            if (children[k].match_id() == c.match_id()) count += 1;
        }
        std::string seg = (count == 1) ? c.match_id() : (c.match_id() + "#" + std::to_string(count));
        if (c.uid() == u) {
            return seg;
        }
        if (const auto* g = c.as_group()) {
            auto sub_path = g->match_path_of(u);
            if (sub_path.has_value()) {
                return seg + "/" + *sub_path;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> PropGroup::name_path_of(Uid u) const {
    for (const auto& c : children) {
        if (c.uid() == u) {
            return c.name();
        }
        if (const auto* g = c.as_group()) {
            auto sub_path = g->name_path_of(u);
            if (sub_path.has_value()) {
                return g->name + "/" + *sub_path;
            }
        }
    }
    return std::nullopt;
}

std::vector<const Node*> PropGroup::node_chain(Uid u) const {
    for (const auto& c : children) {
        if (c.uid() == u) {
            return {&c};
        }
        if (const auto* g = c.as_group()) {
            auto rest = g->node_chain(u);
            if (!rest.empty()) {
                rest.insert(rest.begin(), &c);
                return rest;
            }
        }
    }
    return {};
}

bool PropGroup::any_animated() const {
    for (const auto& c : children) {
        if (const auto* p = c.as_prop()) {
            if (p->is_animated() || p->has_expression()) return true;
        } else if (const auto* g = c.as_group()) {
            if (g->any_animated()) return true;
        }
    }
    return false;
}

void PropGroup::reassign_uids(Uid& next) {
    next += 1;
    uid = next;
    for (auto& c : children) {
        if (auto* p = c.as_prop_mut()) {
            next += 1;
            p->uid = next;
        } else if (auto* g = c.as_group_mut()) {
            g->reassign_uids(next);
        }
    }
}

Uid PropGroup::max_uid() const {
    Uid m = uid;
    for (const auto& c : children) {
        if (const auto* p = c.as_prop()) {
            m = std::max(m, p->uid);
        } else if (const auto* g = c.as_group()) {
            m = std::max(m, g->max_uid());
        }
    }
    return m;
}

bool PropGroup::operator==(const PropGroup& o) const {
    return uid == o.uid && match_id == o.match_id && name == o.name &&
           kind == o.kind && enabled == o.enabled && label == o.label &&
           children == o.children;
}

// =====================================================================
// Standard Builders  (after EffectCraft crates/project/src/build.rs)
// =====================================================================

namespace Build {

PropGroup transform(Ids& ids, std::array<double, 2> anchor, std::array<double, 2> position) {
    Property a = ids.prop("anchor", "Anchor Point", Value::Vec3(anchor[0], anchor[1], 0.0));
    a.with_ui(ParamUi{ParamUiKind::Point});
    a.set_spatial();
    a.shown_dims = 2;

    Property p = ids.prop("position", "Position", Value::Vec3(position[0], position[1], 0.0));
    p.with_ui(ParamUi{ParamUiKind::Point});
    p.set_spatial();
    p.shown_dims = 2;

    Property s = ids.prop("scale", "Scale", Value::Vec3(100.0, 100.0, 100.0));
    s.with_ui(ParamUi{ParamUiKind::Percent});
    s.shown_dims = 2;

    Property o = ids.prop("orientation", "Orientation", Value::Vec3(0.0, 0.0, 0.0));
    o.with_ui(ParamUi{ParamUiKind::Angle});
    o.three_d_only = true;

    Property rx = ids.prop("rotationX", "X Rotation", Value::Scalar(0.0));
    rx.with_ui(ParamUi{ParamUiKind::Angle});
    rx.three_d_only = true;

    Property ry = ids.prop("rotationY", "Y Rotation", Value::Scalar(0.0));
    ry.with_ui(ParamUi{ParamUiKind::Angle});
    ry.three_d_only = true;

    Property rz = ids.prop("rotation", "Rotation", Value::Scalar(0.0));
    rz.with_ui(ParamUi{ParamUiKind::Angle});

    Property op = ids.prop("opacity", "Opacity", Value::Scalar(100.0));
    op.with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 0));

    return ids.group("transform", "Transform")
        .with(std::move(a))
        .with(std::move(p))
        .with(std::move(s))
        .with(std::move(o))
        .with(std::move(rx))
        .with(std::move(ry))
        .with(std::move(rz))
        .with(std::move(op));
}

PropGroup masks(Ids& ids) {
    return ids.group("masks", "Masks");
}

PropGroup effects(Ids& ids) {
    return ids.group("effects", "Effects");
}

// after EffectCraft crates/project/src/build.rs contents and shape_transform
PropGroup shapeContents(Ids& ids) {
    return ids.group("contents", "Contents");
}

PropGroup shapeTransform(Ids& ids) {
    PropGroup group = ids.group("transform", "Transform");
    group.with(ids.prop("anchor", "Anchor Point", Value::Vec2({0.0, 0.0})).with_ui(ParamUi{ParamUiKind::Point}));
    group.with(ids.prop("position", "Position", Value::Vec2({0.0, 0.0})).with_ui(ParamUi{ParamUiKind::Point}));
    group.with(ids.prop("scale", "Scale", Value::Vec2({100.0, 100.0})).with_ui(ParamUi{ParamUiKind::Percent}));
    group.with(ids.prop("skew", "Skew", Value::Scalar(0.0)).with_ui(ParamUi::make_slider(-85.0, 85.0, -85.0, 85.0, 1)));
    group.with(ids.prop("skewAxis", "Skew Axis", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Angle}));
    group.with(ids.prop("rotation", "Rotation", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Angle}));
    group.with(ids.prop("opacity", "Opacity", Value::Scalar(100.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0)));
    return group;
}

// after EffectCraft crates/project/src/build.rs indexed and shape_group
PropGroup shapeGroup(Ids& ids, const std::string& name, std::vector<PropGroup> items) {
    PropGroup contents = shapeContents(ids);
    for (auto& item : items) contents.children.push_back(Node(std::move(item)));
    PropGroup group = ids.group("group", name);
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("blend", "Blend Mode", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(std::move(contents));
    group.with(shapeTransform(ids));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_rect
PropGroup shapeRect(Ids& ids, std::array<double, 2> size,
                   std::array<double, 2> position, double roundness) {
    PropGroup group = ids.group("rect", "Rectangle Path 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("direction", "Path Direction", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(ids.prop("size", "Size", Value::Vec2(size)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("position", "Position", Value::Vec2(position)).with_ui(ParamUi{ParamUiKind::Point}));
    group.with(ids.prop("roundness", "Roundness", Value::Scalar(roundness)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("perCorner", "Per-Corner Roundness", Value::Bool(false)).with_ui(ParamUi{ParamUiKind::Checkbox}));
    const double radius = std::isfinite(roundness) ? std::max(0.0, roundness) : 0.0;
    group.with(ids.prop("topLeftRoundness", "Top Left Roundness", Value::Scalar(radius)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("topRightRoundness", "Top Right Roundness", Value::Scalar(radius)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("bottomRightRoundness", "Bottom Right Roundness", Value::Scalar(radius)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("bottomLeftRoundness", "Bottom Left Roundness", Value::Scalar(radius)).with_ui(ParamUi{ParamUiKind::Pixels}));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_ellipse
PropGroup shapeEllipse(Ids& ids, std::array<double, 2> size, std::array<double, 2> position) {
    PropGroup group = ids.group("ellipse", "Ellipse Path 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("direction", "Path Direction", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(ids.prop("size", "Size", Value::Vec2(size)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("position", "Position", Value::Vec2(position)).with_ui(ParamUi{ParamUiKind::Point}));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_path
PropGroup shapePath(Ids& ids, ShapePath path) {
    PropGroup group = ids.group("path", "Path 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("direction", "Path Direction", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(ids.prop("path", "Path", Value::Path(path)).with_ui(ParamUi{ParamUiKind::Path}));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_fill
PropGroup shapeFill(Ids& ids, std::array<double, 4> color) {
    PropGroup group = ids.group("fill", "Fill 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("blend", "Blend Mode", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(ids.prop("composite", "Composite", Value::Enum(0)).with_ui(ParamUi::make_popup({"Below Previous in Same Group", "Above Previous in Same Group"})));
    group.with(ids.prop("rule", "Fill Rule", Value::Enum(0)).with_ui(ParamUi::make_popup({"Non-Zero Winding", "Even-Odd"})));
    group.with(ids.prop("color", "Color", Value::Color(color)).with_ui(ParamUi{ParamUiKind::Color}));
    group.with(ids.prop("opacity", "Opacity", Value::Scalar(100.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0)));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_stroke, stroke_taper and stroke_wave
PropGroup shapeStrokeTaper(Ids& ids) {
    PropGroup group = ids.group("taper", "Taper");
    group.with(ids.prop("units", "Length Units", Value::Enum(0)).with_ui(ParamUi::make_popup({"Pixels", "Percent"})));
    group.with(ids.prop("startLength", "Start Length", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("endLength", "End Length", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("startWidth", "Start Width", Value::Scalar(100.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("endWidth", "End Width", Value::Scalar(100.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("startEase", "Start Ease", Value::Scalar(0.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("endEase", "End Ease", Value::Scalar(0.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    return group;
}

PropGroup shapeStrokeWave(Ids& ids) {
    PropGroup group = ids.group("wave", "Wave");
    group.with(ids.prop("amount", "Amount", Value::Scalar(0.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("units", "Units", Value::Enum(0)).with_ui(ParamUi::make_popup({"Pixels", "Cycles"})));
    group.with(ids.prop("wavelength", "Wavelength", Value::Scalar(20.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("cycles", "Cycles", Value::Scalar(10.0)).with_ui(ParamUi::make_slider(0.0, 1000.0, 0.0, 50.0, 1)));
    group.with(ids.prop("phase", "Phase", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Angle}));
    return group;
}

PropGroup shapeStroke(Ids& ids, std::array<double, 4> color, double width) {
    PropGroup group = ids.group("stroke", "Stroke 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("blend", "Blend Mode", Value::Enum(0)).with_ui(ParamUi{ParamUiKind::Hidden}));
    group.with(ids.prop("composite", "Composite", Value::Enum(0)).with_ui(ParamUi::make_popup({"Below Previous in Same Group", "Above Previous in Same Group"})));
    group.with(ids.prop("color", "Color", Value::Color(color)).with_ui(ParamUi{ParamUiKind::Color}));
    group.with(ids.prop("opacity", "Opacity", Value::Scalar(100.0)).with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0)));
    group.with(ids.prop("width", "Stroke Width", Value::Scalar(width)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(ids.prop("cap", "Line Cap", Value::Enum(0)).with_ui(ParamUi::make_popup({"Butt Cap", "Round Cap", "Projecting Cap"})));
    group.with(ids.prop("join", "Line Join", Value::Enum(0)).with_ui(ParamUi::make_popup({"Miter Join", "Round Join", "Bevel Join"})));
    group.with(ids.prop("miter", "Miter Limit", Value::Scalar(4.0)));
    PropGroup dashes = ids.group("dashes", "Dashes");
    dashes.with(ids.prop("dash", "Dash", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    dashes.with(ids.prop("gap", "Gap", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    dashes.with(ids.prop("offset", "Offset", Value::Scalar(0.0)).with_ui(ParamUi{ParamUiKind::Pixels}));
    group.with(std::move(dashes));
    group.with(shapeStrokeTaper(ids));
    group.with(shapeStrokeWave(ids));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_trim
PropGroup shapeTrim(Ids& ids, double start, double end, double offset) {
    PropGroup group = ids.group("trim", "Trim Paths 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("start", "Start", Value::Scalar(start))
        .with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("end", "End", Value::Scalar(end))
        .with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 1)));
    group.with(ids.prop("offset", "Offset", Value::Scalar(offset))
        .with_ui(ParamUi{ParamUiKind::Angle}));
    group.with(ids.prop("mode", "Trim Multiple Shapes", Value::Enum(0))
        .with_ui(ParamUi::make_popup({"Simultaneously", "Individually"})));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_repeater
PropGroup shapeRepeater(Ids& ids, double copies, std::array<double, 2> offsetPosition) {
    PropGroup group = ids.group("repeater", "Repeater 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("copies", "Copies", Value::Scalar(copies)));
    group.with(ids.prop("offset", "Offset", Value::Scalar(0.0)));
    group.with(ids.prop("composite", "Composite", Value::Enum(0))
        .with_ui(ParamUi::make_popup({"Below", "Above"})));
    PropGroup transform = ids.group("transform", "Transform");
    transform.with(ids.prop("anchor", "Anchor Point", Value::Vec2({0.0, 0.0}))
        .with_ui(ParamUi{ParamUiKind::Point}));
    transform.with(ids.prop("position", "Position", Value::Vec2(offsetPosition))
        .with_ui(ParamUi{ParamUiKind::Point}));
    transform.with(ids.prop("scale", "Scale", Value::Vec2({100.0, 100.0}))
        .with_ui(ParamUi{ParamUiKind::Percent}));
    transform.with(ids.prop("rotation", "Rotation", Value::Scalar(0.0))
        .with_ui(ParamUi{ParamUiKind::Angle}));
    transform.with(ids.prop("startOpacity", "Start Opacity", Value::Scalar(100.0))
        .with_ui(ParamUi{ParamUiKind::Percent}));
    transform.with(ids.prop("endOpacity", "End Opacity", Value::Scalar(100.0))
        .with_ui(ParamUi{ParamUiKind::Percent}));
    group.with(std::move(transform));
    return group;
}

// after EffectCraft crates/project/src/build.rs shape_simple_op
PropGroup shapeRound(Ids& ids, double radius) {
    PropGroup group = ids.group("round", "Round Corners 1");
    group.kind.tag = GroupKindTag::Indexed;
    group.with(ids.prop("radius", "Radius", Value::Scalar(radius))
        .with_ui(ParamUi{ParamUiKind::Pixels}));
    return group;
}

// after EffectCraft crates/engine/src/cmd/layer/contents.rs add_shape_item
std::optional<PropGroup> shapeItem(Ids& ids, const std::string& kind) {
    if (kind == "group")
        return shapeGroup(ids, "Group 1", {});
    if (kind == "rect")
        return shapeRect(ids, {200.0, 200.0}, {0.0, 0.0}, 0.0);
    if (kind == "ellipse")
        return shapeEllipse(ids, {200.0, 200.0}, {0.0, 0.0});
    if (kind == "path")
        return shapePath(ids, KeyframeModel::ShapePath{});
    if (kind == "fill")
        return shapeFill(ids, {1.0, 0.0, 0.0, 1.0});
    if (kind == "stroke")
        return shapeStroke(ids, {1.0, 1.0, 1.0, 1.0}, 2.0);
    if (kind == "trim")
        return shapeTrim(ids, 0.0, 100.0, 0.0);
    if (kind == "repeater")
        return shapeRepeater(ids, 3.0, {100.0, 0.0});
    if (kind == "round")
        return shapeRound(ids, 10.0);
    return std::nullopt;
}

// Insert a new item in the layer Contents or a shape group's inner Contents.
std::optional<Uid> addShapeItem(Ids& ids, PropGroup& contents, const std::string& kind,
                                Uid parentGroup) {
    if (contents.match_id != "contents") return std::nullopt;
    PropGroup* destination = &contents;
    if (parentGroup != 0) {
        PropGroup* parent = contents.find_group_mut(parentGroup);
        if (!parent || parent->match_id != "group") return std::nullopt;
        destination = parent->group_mut("contents");
        if (!destination) return std::nullopt;
    }
    std::optional<PropGroup> item = shapeItem(ids, kind);
    if (!item) return std::nullopt;
    const Uid uid = item->uid;
    destination->children.emplace_back(std::move(*item));
    return uid;
}

PropGroup mask(Ids& ids, const std::string& name, ShapePath path, MaskMode mode, std::array<uint8_t, 3> color) {
    PropGroup g = ids.group("mask", name);
    g.kind.tag = GroupKindTag::Mask;
    g.kind.mask_mode = mode;
    g.kind.mask_inverted = false;
    g.kind.mask_color[0] = color[0];
    g.kind.mask_color[1] = color[1];
    g.kind.mask_color[2] = color[2];
    g.kind.mask_locked = false;
    g.kind.mask_motion_blur = MaskMotionBlur::SameAsLayer;
    g.kind.mask_falloff = FeatherFalloff::Smooth;
    g.kind.mask_roto_bezier = false;

    Property p_path = ids.prop("path", "Mask Path", Value::Path(std::move(path)));
    p_path.with_ui(ParamUi{ParamUiKind::Path});

    Property p_feather = ids.prop("feather", "Mask Feather", Value::Vec2(0.0, 0.0));
    p_feather.with_ui(ParamUi{ParamUiKind::Pixels});

    Property p_op = ids.prop("opacity", "Mask Opacity", Value::Scalar(100.0));
    p_op.with_ui(ParamUi::make_slider(0.0, 100.0, 0.0, 100.0, 0));

    Property p_exp = ids.prop("expansion", "Mask Expansion", Value::Scalar(0.0));
    p_exp.with_ui(ParamUi{ParamUiKind::Pixels});

    return g.with(std::move(p_path))
            .with(std::move(p_feather))
            .with(std::move(p_op))
            .with(std::move(p_exp));
}

PropGroup path_options(Ids& ids) {
    Property p_path = ids.prop("path", "Path", Value::Enum(0));
    p_path.with_ui(ParamUi::make_popup({"None"}));
    p_path.static_only = true;

    Property p_rev = ids.prop("reversePath", "Reverse Path", Value::Bool(false));
    p_rev.with_ui(ParamUi{ParamUiKind::Checkbox});

    Property p_perp = ids.prop("perpendicular", "Perpendicular To Path", Value::Bool(true));
    p_perp.with_ui(ParamUi{ParamUiKind::Checkbox});

    Property p_align = ids.prop("forceAlignment", "Force Alignment", Value::Bool(false));
    p_align.with_ui(ParamUi{ParamUiKind::Checkbox});

    Property p_fm = ids.prop("firstMargin", "First Margin", Value::Scalar(0.0));
    p_fm.with_ui(ParamUi{ParamUiKind::Pixels});

    Property p_lm = ids.prop("lastMargin", "Last Margin", Value::Scalar(0.0));
    p_lm.with_ui(ParamUi{ParamUiKind::Pixels});

    return ids.group("pathOptions", "Path Options")
        .with(std::move(p_path))
        .with(std::move(p_rev))
        .with(std::move(p_perp))
        .with(std::move(p_align))
        .with(std::move(p_fm))
        .with(std::move(p_lm));
}

PropGroup more_options(Ids& ids) {
    Property p_ag = ids.prop("anchorGrouping", "Anchor Point Grouping", Value::Enum(0));
    p_ag.with_ui(ParamUi::make_popup({"Character", "Word", "Line", "All"}));
    p_ag.static_only = true;

    Property p_ga = ids.prop("groupingAlignment", "Grouping Alignment", Value::Vec2(0.0, 0.0));
    p_ga.with_ui(ParamUi{ParamUiKind::Percent});

    Property p_fs = ids.prop("fillStroke", "Fill & Stroke", Value::Enum(0));
    p_fs.with_ui(ParamUi::make_popup({"Per Character Palette", "All Fills Over All Strokes", "All Strokes Over All Fills"}));
    p_fs.static_only = true;

    static const std::vector<std::string> blend_modes = {
        "Normal", "Darken", "Multiply", "Color Burn", "Linear Burn", "Darker Color",
        "Add", "Lighten", "Screen", "Color Dodge", "Lighter Color", "Overlay",
        "Soft Light", "Hard Light", "Linear Light", "Vivid Light", "Pin Light",
        "Hard Mix", "Difference", "Exclusion", "Subtract", "Divide", "Hue",
        "Saturation", "Color", "Luminosity"
    };
    Property p_icb = ids.prop("interCharBlend", "Inter-Character Blending", Value::Enum(0));
    p_icb.with_ui(ParamUi::make_popup(blend_modes));
    p_icb.static_only = true;

    return ids.group("moreOptions", "More Options")
        .with(std::move(p_ag))
        .with(std::move(p_ga))
        .with(std::move(p_fs))
        .with(std::move(p_icb));
}

} // namespace Build

} // namespace Props
