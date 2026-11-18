// Morpheus - vector icons drawn on a 16x16 design grid.
// Ported function-by-function from EffectCraft crates/ui-egui/src/icons.rs (853 lines).
// Original artwork, MIT OR Apache-2.0.
// after EffectCraft (crates/ui-egui/src/icons.rs)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <imgui.h>
#include <vector>

namespace Icons {

enum class Icon {
    // tools
    Home,
    Selection,
    Hand,
    Zoom,
    Orbit,
    PanCamera,
    Dolly,
    Rotate,
    PanBehind,
    Rectangle,
    RoundedRect,
    Ellipse,
    Polygon,
    Star,
    StarFilled,
    Pen,
    PenAdd,
    PenDelete,
    PenConvert,
    MaskFeather,
    Type,
    TypeVertical,
    Brush,
    Clone,
    Eraser,
    RotoBrush,
    RefineEdge,
    ObjectMatte,
    Puppet,
    PuppetAdvanced,
    PuppetBend,
    PuppetStarch,
    PuppetOverlap,
    // switches & timeline
    Eye,
    Speaker,
    Solo,
    Lock,
    LockOpen,
    Shy,
    Collapse,
    Quality,
    Fx,
    FrameBlend,
    MotionBlur,
    Adjustment,
    Cube,
    Draft3D,
    Graph,
    Stopwatch,
    Keyframe,
    PickWhip,
    Flowchart,
    MarkerBin,
    // project items
    Folder,
    Comp,
    Footage,
    Image,
    Audio,
    Solid,
    TextLayer,
    ShapeLayer,
    Null,
    Camera,
    Light,
    // ui
    Search,
    Trash,
    NewFolder,
    NewComp,
    Gear,
    Hamburger,
    ChevronDown,
    ChevronRight,
    ChevronLeft,
    Play,
    Pause,
    StepBack,
    StepFwd,
    First,
    Last,
    Loop,
    Snapshot,
    ShowSnapshot,
    Grid,
    MaskVis,
    Checker,
    Region,
    ExtendedViewer,
    Plus,
    Minus,
    Close,
    Link,
    MountainSmall,
    MountainLarge,
    Fullscreen,
    Channel,
    Exposure,
    StrokeWidth,
    // links
    Chat,
    Globe,
    Code,
    Sparkle,
    // appearance modes
    Monitor,
    Sun,
    Moon,
};

// KeyHalf for keyframe glyphs
// after EffectCraft effectcraft_engine::keyframe::KeyHalf
enum class KeyHalf {
    Linear,
    Hold,
    Bezier,
    Auto,
};

// Paint icon into a bounding box [rectMin, rectMax] on dl.
// after EffectCraft crates/ui-egui/src/icons.rs paint()
void paint(ImDrawList* dl, ImVec2 rectMin, ImVec2 rectMax, Icon icon, ImU32 color, float width = 1.25f);

// Convenience wrapper: paint icon centered at center with diameter size.
void paintCentered(ImDrawList* dl, ImVec2 center, float size, Icon icon, ImU32 color, float width = 1.25f);

// Interactive icon button in ImGui. Returns true when clicked.
// after EffectCraft crates/ui-egui/src/widgets.rs icon_button
bool iconButton(const char* str_id, Icon icon, bool active = false, ImVec2 size = ImVec2(24, 24), ImU32 tint = 0);

// Keyframe glyph drawing
// after EffectCraft crates/ui-egui/src/icons.rs keyframe()
void drawKeyframe(ImDrawList* dl, ImVec2 center, float size, KeyHalf left, KeyHalf right, ImU32 fill);

struct RectBounds {
    ImVec2 min;
    ImVec2 max;
};
// after EffectCraft crates/ui-egui/src/icons.rs keyframe_bounds()
RectBounds keyframeBounds(ImVec2 center, float size, KeyHalf left, KeyHalf right);

// Get the keyframe polygon shapes for tests
// after EffectCraft crates/ui-egui/src/icons.rs keyframe_shapes()
std::vector<std::vector<ImVec2>> keyframeShapes(ImVec2 center, float size, KeyHalf left, KeyHalf right);

// Concave glyph pieces for test checking
// after EffectCraft crates/ui-egui/src/icons.rs CONCAVE
struct PolyPiece {
    const ImVec2* pts;
    int count;
};
struct ConcaveGlyph {
    const PolyPiece* pieces;
    int count;
};
const ConcaveGlyph* getConcaveGlyphs(int* outCount);

} // namespace Icons
