// Morpheus - vector icons drawn on a 16x16 design grid.
// Ported function-by-function from EffectCraft crates/ui-egui/src/icons.rs (853 lines).
// Original artwork, MIT OR Apache-2.0.
// after EffectCraft (crates/ui-egui/src/icons.rs)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "icons.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <vector>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Icons {

// Constants for math
static constexpr float PI = 3.14159265358979323846f;
static constexpr float TAU = 6.28318530717958647692f;
static constexpr float DEG2RAD = PI / 180.0f;
static constexpr float FRAC_PI_2 = 1.57079632679489661923f;
static constexpr float FRAC_PI_4 = 0.78539816339744830962f;

// --- Concave glyph pieces (SELECTION_ARROW, SPEAKER, SPARKLE) ---
// after EffectCraft crates/ui-egui/src/icons.rs SELECTION_ARROW, SPEAKER, SPARKLE
static const ImVec2 SEL_0[] = { {4.5f, 2.0f}, {4.5f, 13.0f}, {7.2f, 10.5f}, {10.1f, 7.39f} };
static const ImVec2 SEL_1[] = { {7.5f, 9.0f}, {8.9f, 9.9f}, {12.5f, 9.7f}, {10.1f, 7.39f} };
static const ImVec2 SEL_2[] = { {7.8f, 8.6f}, {7.2f, 10.5f}, {9.2f, 14.6f}, {10.8f, 13.9f}, {8.9f, 9.9f} };
static const PolyPiece SEL_PIECES[] = { {SEL_0, 4}, {SEL_1, 4}, {SEL_2, 5} };

static const ImVec2 SPK_0[] = { {2.0f, 6.0f}, {5.5f, 6.0f}, {5.5f, 10.0f}, {2.0f, 10.0f} };
static const ImVec2 SPK_1[] = { {5.0f, 6.0f}, {9.0f, 2.5f}, {9.0f, 13.5f}, {5.0f, 10.0f} };
static const PolyPiece SPK_PIECES[] = { {SPK_0, 4}, {SPK_1, 4} };

static const ImVec2 SPARK_0[] = { {8.0f, 1.5f}, {9.784f, 8.0f}, {8.0f, 14.5f}, {6.216f, 8.0f} };
static const ImVec2 SPARK_1[] = { {1.5f, 8.0f}, {8.0f, 6.216f}, {14.5f, 8.0f}, {8.0f, 9.784f} };
static const PolyPiece SPARK_PIECES[] = { {SPARK_0, 4}, {SPARK_1, 4} };

static const ConcaveGlyph CONCAVE_GLYPHS[] = {
    { SEL_PIECES, 3 },
    { SPK_PIECES, 2 },
    { SPARK_PIECES, 2 }
};

const ConcaveGlyph* getConcaveGlyphs(int* outCount) {
    if (outCount) *outCount = 3;
    return CONCAVE_GLYPHS;
}

// --- Pen16 helper struct ---
// after EffectCraft crates/ui-egui/src/icons.rs Pen16
struct Pen16 {
    ImDrawList* dl;
    ImVec2 minPos;
    ImVec2 maxPos;
    ImU32 color;
    float width;

    float s() const {
        float w = maxPos.x - minPos.x;
        float h = maxPos.y - minPos.y;
        return (w < h ? w : h) / 16.0f;
    }

    ImVec2 p(float x, float y) const {
        float scale = s();
        ImVec2 center((minPos.x + maxPos.x) * 0.5f, (minPos.y + maxPos.y) * 0.5f);
        ImVec2 o(center.x - 8.0f * scale, center.y - 8.0f * scale);
        return ImVec2(o.x + x * scale, o.y + y * scale);
    }

    void line(const ImVec2* pts, int count) const {
        if (count < 2) return;
        std::vector<ImVec2> mapped(count);
        for (int i = 0; i < count; ++i) mapped[i] = p(pts[i].x, pts[i].y);
        dl->AddPolyline(mapped.data(), count, color, 0, width * s());
    }

    void closed(const ImVec2* pts, int count) const {
        if (count < 2) return;
        std::vector<ImVec2> mapped(count);
        for (int i = 0; i < count; ++i) mapped[i] = p(pts[i].x, pts[i].y);
        dl->AddPolyline(mapped.data(), count, color, ImDrawFlags_Closed, width * s());
    }

    void fill(const ImVec2* pts, int count) const {
        if (count < 3) return;
        std::vector<ImVec2> mapped(count);
        for (int i = 0; i < count; ++i) mapped[i] = p(pts[i].x, pts[i].y);
        dl->AddConvexPolyFilled(mapped.data(), count, color);
    }

    void circle(float x, float y, float r) const {
        dl->AddCircle(p(x, y), r * s(), color, 0, width * s());
    }

    void dot(float x, float y, float r) const {
        dl->AddCircleFilled(p(x, y), r * s(), color, 0);
    }

    void dot_colored(float x, float y, float r, ImU32 col) const {
        float alpha = ((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        ImVec4 v = ImGui::ColorConvertU32ToFloat4(col);
        v.w *= alpha;
        dl->AddCircleFilled(p(x, y), r * s(), ImGui::ColorConvertFloat4ToU32(v), 0);
    }

    void rect(float x0, float y0, float x1, float y1) const {
        dl->AddRect(p(x0, y0), p(x1, y1), color, 1.0f * s(), 0, width * s());
    }

    void rrect(float x0, float y0, float x1, float y1, float rad) const {
        dl->AddRect(p(x0, y0), p(x1, y1), color, rad * s(), 0, width * s());
    }

    void rect_fill(float x0, float y0, float x1, float y1) const {
        dl->AddRectFilled(p(x0, y0), p(x1, y1), color, 0.8f * s());
    }

    void arc(float cx, float cy, float r, float a0_deg, float a1_deg) const {
        const int n = 24;
        ImVec2 pts[n + 1];
        for (int i = 0; i <= n; ++i) {
            float a = (a0_deg + (a1_deg - a0_deg) * (float)i / (float)n) * DEG2RAD;
            pts[i] = ImVec2(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        line(pts, n + 1);
    }

    void pushpin(float cx) const {
        const ImVec2 cap[] = { {cx - 3.0f, 1.5f}, {cx + 3.0f, 1.5f}, {cx + 2.0f, 3.0f}, {cx - 2.0f, 3.0f} };
        fill(cap, 4);
        const ImVec2 body[] = { {cx - 2.0f, 3.0f}, {cx + 2.0f, 3.0f}, {cx + 2.4f, 7.0f}, {cx + 4.0f, 8.8f}, {cx - 4.0f, 8.8f}, {cx - 2.4f, 7.0f} };
        closed(body, 6);
        const ImVec2 needle[] = { {cx, 8.8f}, {cx, 14.8f} };
        line(needle, 2);
    }

    void ellipse(float cx, float cy, float rx, float ry) const {
        const int n = 32;
        ImVec2 pts[n];
        for (int i = 0; i < n; ++i) {
            float a = TAU * (float)i / (float)n;
            pts[i] = ImVec2(cx + rx * std::cos(a), cy + ry * std::sin(a));
        }
        closed(pts, n);
    }

    std::vector<ImVec2> star(float cx, float cy, float ro, float ri, int n) const {
        std::vector<ImVec2> pts(n * 2);
        for (int i = 0; i < n * 2; ++i) {
            float a = PI * (float)i / (float)n - FRAC_PI_2;
            float r = (i % 2 == 0) ? ro : ri;
            pts[i] = ImVec2(cx + r * std::cos(a), cy + r * std::sin(a));
        }
        return pts;
    }
};

// --- Main paint() implementation ---
// after EffectCraft crates/ui-egui/src/icons.rs paint()
void paint(ImDrawList* dl, ImVec2 rectMin, ImVec2 rectMax, Icon icon, ImU32 color, float width) {
    Pen16 pen{ dl, rectMin, rectMax, color, width };

    switch (icon) {
    case Icon::Home: {
        const ImVec2 body[] = { {2.5f, 7.5f}, {8.0f, 2.5f}, {13.5f, 7.5f}, {13.5f, 13.5f}, {2.5f, 13.5f} };
        pen.closed(body, 5);
        pen.rect(6.5f, 9.5f, 9.5f, 13.5f);
        break;
    }
    case Icon::Selection:
        for (const auto& piece : SEL_PIECES) {
            pen.fill(piece.pts, piece.count);
        }
        break;
    case Icon::Hand: {
        const ImVec2 f1[] = { {5.0f, 9.0f}, {5.0f, 4.0f} };
        const ImVec2 f2[] = { {7.3f, 8.0f}, {7.3f, 2.6f} };
        const ImVec2 f3[] = { {9.6f, 8.0f}, {9.6f, 3.2f} };
        const ImVec2 f4[] = { {11.8f, 8.8f}, {11.8f, 5.0f} };
        const ImVec2 palm[] = { {5.0f, 9.0f}, {3.2f, 7.4f}, {2.4f, 8.4f}, {5.6f, 13.4f}, {11.0f, 14.0f}, {12.4f, 11.0f}, {11.8f, 8.8f} };
        pen.line(f1, 2);
        pen.line(f2, 2);
        pen.line(f3, 2);
        pen.line(f4, 2);
        pen.line(palm, 7);
        break;
    }
    case Icon::Zoom: {
        pen.circle(6.8f, 6.8f, 4.3f);
        const ImVec2 handle[] = { {10.0f, 10.0f}, {14.0f, 14.0f} };
        pen.line(handle, 2);
        break;
    }
    case Icon::Orbit: {
        pen.ellipse(8.0f, 8.0f, 6.0f, 2.8f);
        pen.dot(8.0f, 8.0f, 1.6f);
        const ImVec2 arrow[] = { {12.0f, 3.0f}, {14.5f, 5.2f}, {11.4f, 6.0f} };
        pen.fill(arrow, 3);
        break;
    }
    case Icon::PanCamera: {
        const ImVec2 vert[] = { {8.0f, 1.8f}, {8.0f, 14.2f} };
        const ImVec2 horiz[] = { {1.8f, 8.0f}, {14.2f, 8.0f} };
        pen.line(vert, 2);
        pen.line(horiz, 2);
        const ImVec2 topA[] = { {8.0f, 1.0f}, {6.2f, 3.5f}, {9.8f, 3.5f} };
        const ImVec2 botA[] = { {8.0f, 15.0f}, {6.2f, 12.5f}, {9.8f, 12.5f} };
        const ImVec2 leftA[] = { {1.0f, 8.0f}, {3.5f, 6.2f}, {3.5f, 9.8f} };
        const ImVec2 rightA[] = { {15.0f, 8.0f}, {12.5f, 6.2f}, {12.5f, 9.8f} };
        pen.fill(topA, 3);
        pen.fill(botA, 3);
        pen.fill(leftA, 3);
        pen.fill(rightA, 3);
        break;
    }
    case Icon::Dolly: {
        const ImVec2 vert[] = { {8.0f, 1.8f}, {8.0f, 14.2f} };
        pen.line(vert, 2);
        const ImVec2 topA[] = { {8.0f, 1.0f}, {5.5f, 4.5f}, {10.5f, 4.5f} };
        const ImVec2 botA[] = { {8.0f, 15.0f}, {5.5f, 11.5f}, {10.5f, 11.5f} };
        pen.fill(topA, 3);
        pen.fill(botA, 3);
        pen.rect(5.0f, 6.5f, 11.0f, 9.5f);
        break;
    }
    case Icon::Rotate: {
        pen.arc(8.0f, 8.0f, 5.0f, -60.0f, 220.0f);
        const ImVec2 arrow[] = { {10.2f, 1.8f}, {13.6f, 4.6f}, {9.6f, 5.8f} };
        pen.fill(arrow, 3);
        break;
    }
    case Icon::PanBehind: {
        pen.circle(8.0f, 8.0f, 3.0f);
        const ImVec2 l1[] = { {8.0f, 1.5f}, {8.0f, 5.0f} };
        const ImVec2 l2[] = { {8.0f, 11.0f}, {8.0f, 14.5f} };
        const ImVec2 l3[] = { {1.5f, 8.0f}, {5.0f, 8.0f} };
        const ImVec2 l4[] = { {11.0f, 8.0f}, {14.5f, 8.0f} };
        pen.line(l1, 2);
        pen.line(l2, 2);
        pen.line(l3, 2);
        pen.line(l4, 2);
        break;
    }
    case Icon::Rectangle:
        pen.rect(2.5f, 3.5f, 13.5f, 12.5f);
        break;
    case Icon::RoundedRect:
        pen.rrect(2.5f, 3.5f, 13.5f, 12.5f, 3.0f);
        break;
    case Icon::Ellipse:
        pen.ellipse(8.0f, 8.0f, 5.8f, 4.8f);
        break;
    case Icon::Polygon: {
        ImVec2 pts[6];
        for (int i = 0; i < 6; ++i) {
            float a = TAU * (float)i / 6.0f - FRAC_PI_2;
            pts[i] = ImVec2(8.0f + 6.0f * std::cos(a), 8.4f + 6.0f * std::sin(a));
        }
        pen.closed(pts, 6);
        break;
    }
    case Icon::Star: {
        auto pts = pen.star(8.0f, 8.6f, 6.4f, 2.7f, 5);
        pen.closed(pts.data(), (int)pts.size());
        break;
    }
    case Icon::StarFilled: {
        auto pts = pen.star(8.0f, 8.6f, 6.4f, 2.7f, 5);
        ImVec2 c(8.0f, 8.6f);
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const ImVec2 tri[] = { c, pts[i], pts[i + 1] };
            pen.fill(tri, 3);
        }
        if (!pts.empty()) {
            const ImVec2 tri[] = { c, pts.back(), pts.front() };
            pen.fill(tri, 3);
        }
        break;
    }
    case Icon::Pen: {
        const ImVec2 nib[] = { {8.0f, 1.8f}, {12.0f, 9.0f}, {8.0f, 14.0f}, {4.0f, 9.0f} };
        pen.closed(nib, 4);
        pen.dot(8.0f, 8.6f, 1.1f);
        const ImVec2 slit[] = { {8.0f, 1.8f}, {8.0f, 7.6f} };
        pen.line(slit, 2);
        break;
    }
    case Icon::Type: {
        const ImVec2 top[] = { {3.0f, 3.5f}, {13.0f, 3.5f} };
        const ImVec2 stem[] = { {8.0f, 3.5f}, {8.0f, 13.5f} };
        const ImVec2 base[] = { {6.0f, 13.5f}, {10.0f, 13.5f} };
        pen.line(top, 2);
        pen.line(stem, 2);
        pen.line(base, 2);
        break;
    }
    case Icon::TypeVertical: {
        const ImVec2 stem[] = { {12.5f, 3.0f}, {12.5f, 13.0f} };
        const ImVec2 cross[] = { {12.5f, 8.0f}, {3.0f, 8.0f} };
        const ImVec2 ser[] = { {3.0f, 6.0f}, {3.0f, 10.0f} };
        pen.line(stem, 2);
        pen.line(cross, 2);
        pen.line(ser, 2);
        break;
    }
    case Icon::Brush: {
        const ImVec2 handle[] = { {13.5f, 2.5f}, {7.0f, 9.0f} };
        pen.line(handle, 2);
        const ImVec2 bristle[] = { {7.6f, 8.4f}, {4.4f, 9.4f}, {2.6f, 13.6f}, {6.8f, 12.0f}, {7.8f, 9.0f} };
        pen.fill(bristle, 5);
        break;
    }
    case Icon::Clone: {
        pen.rect(6.0f, 8.0f, 10.0f, 14.0f);
        const ImVec2 stem[] = { {8.0f, 8.0f}, {8.0f, 5.5f} };
        pen.line(stem, 2);
        pen.ellipse(8.0f, 3.8f, 3.0f, 1.8f);
        break;
    }
    case Icon::Eraser: {
        const ImVec2 body[] = { {9.0f, 2.5f}, {14.0f, 7.5f}, {8.0f, 13.5f}, {3.0f, 8.5f} };
        pen.closed(body, 4);
        const ImVec2 band[] = { {5.5f, 6.0f}, {11.5f, 11.0f} };
        pen.line(band, 2);
        const ImVec2 base[] = { {6.0f, 14.0f}, {14.0f, 14.0f} };
        pen.line(base, 2);
        break;
    }
    case Icon::RotoBrush: {
        const ImVec2 handle[] = { {13.0f, 2.5f}, {8.0f, 7.5f} };
        pen.line(handle, 2);
        pen.ellipse(6.0f, 10.0f, 3.6f, 3.6f);
        pen.dot(6.0f, 10.0f, 1.0f);
        break;
    }
    case Icon::RefineEdge: {
        const ImVec2 handle[] = { {13.5f, 2.5f}, {9.0f, 7.0f} };
        pen.line(handle, 2);
        pen.ellipse(7.0f, 9.0f, 2.6f, 2.6f);
        const ImVec2 d1[] = { {2.0f, 14.0f}, {3.5f, 12.5f} };
        const ImVec2 d2[] = { {5.0f, 14.0f}, {6.5f, 12.5f} };
        const ImVec2 d3[] = { {8.0f, 14.0f}, {9.5f, 12.5f} };
        const ImVec2 d4[] = { {11.0f, 14.0f}, {12.5f, 12.5f} };
        pen.line(d1, 2);
        pen.line(d2, 2);
        pen.line(d3, 2);
        pen.line(d4, 2);
        break;
    }
    case Icon::ObjectMatte: {
        struct Corner { float x, y, dx, dy; };
        const Corner corners[] = {
            {2.0f, 2.0f, 1.0f, 1.0f},
            {14.0f, 2.0f, -1.0f, 1.0f},
            {14.0f, 14.0f, -1.0f, -1.0f},
            {2.0f, 14.0f, 1.0f, -1.0f}
        };
        for (const auto& c : corners) {
            const ImVec2 brk[] = { {c.x, c.y + 3.0f * c.dy}, {c.x, c.y}, {c.x + 3.0f * c.dx, c.y} };
            pen.line(brk, 3);
        }
        pen.dot(8.0f, 8.0f, 2.2f);
        pen.circle(8.0f, 8.0f, 4.4f);
        break;
    }
    case Icon::Puppet:
        pen.pushpin(8.0f);
        break;
    case Icon::PuppetAdvanced:
        pen.pushpin(6.5f);
        pen.circle(12.0f, 12.0f, 2.4f);
        pen.rect_fill(13.6f, 11.2f, 15.2f, 12.8f);
        break;
    case Icon::PuppetBend: {
        pen.pushpin(6.5f);
        pen.arc(11.8f, 13.2f, 3.0f, 200.0f, 340.0f);
        const ImVec2 arrow[] = { {14.9f, 11.0f}, {15.4f, 13.8f}, {12.9f, 12.6f} };
        pen.fill(arrow, 3);
        break;
    }
    case Icon::PuppetStarch: {
        pen.pushpin(6.5f);
        for (float y : {10.5f, 12.75f, 15.0f}) {
            const ImVec2 bar[] = { {10.5f, y}, {15.0f, y} };
            pen.line(bar, 2);
        }
        break;
    }
    case Icon::PuppetOverlap:
        pen.pushpin(6.5f);
        pen.rect(10.0f, 9.8f, 13.4f, 13.2f);
        pen.rect_fill(11.8f, 11.6f, 15.2f, 15.0f);
        break;
    case Icon::Eye: {
        const ImVec2 pts[] = {
            {1.5f, 8.0f}, {4.0f, 5.0f}, {8.0f, 3.8f}, {12.0f, 5.0f},
            {14.5f, 8.0f}, {12.0f, 11.0f}, {8.0f, 12.2f}, {4.0f, 11.0f}, {1.5f, 8.0f}
        };
        pen.line(pts, 9);
        pen.dot(8.0f, 8.0f, 2.2f);
        break;
    }
    case Icon::Speaker:
        for (const auto& piece : SPK_PIECES) {
            pen.fill(piece.pts, piece.count);
        }
        pen.arc(9.0f, 8.0f, 3.0f, -50.0f, 50.0f);
        pen.arc(9.0f, 8.0f, 5.5f, -50.0f, 50.0f);
        break;
    case Icon::Solo:
        pen.circle(8.0f, 8.0f, 4.5f);
        pen.dot(8.0f, 8.0f, 2.4f);
        break;
    case Icon::Lock:
        pen.rect(3.5f, 7.0f, 12.5f, 14.0f);
        pen.arc(8.0f, 7.0f, 3.0f, 180.0f, 360.0f);
        pen.dot(8.0f, 10.5f, 1.0f);
        break;
    case Icon::LockOpen: {
        pen.rect(3.5f, 7.0f, 12.5f, 14.0f);
        pen.arc(8.0f, 4.0f, 3.0f, 180.0f, 360.0f);
        const ImVec2 shackle[] = { {5.0f, 4.0f}, {5.0f, 7.0f} };
        pen.line(shackle, 2);
        pen.dot(8.0f, 10.5f, 1.0f);
        break;
    }
    case Icon::Shy:
        pen.circle(8.0f, 8.0f, 5.8f);
        pen.dot(6.0f, 7.0f, 0.8f);
        pen.dot(10.0f, 7.0f, 0.8f);
        pen.arc(8.0f, 8.5f, 2.6f, 30.0f, 150.0f);
        break;
    case Icon::Collapse: {
        pen.circle(8.0f, 8.0f, 2.8f);
        for (int i = 0; i < 8; ++i) {
            float a = TAU * (float)i / 8.0f;
            const ImVec2 ray[] = {
                {8.0f + 4.3f * std::cos(a), 8.0f + 4.3f * std::sin(a)},
                {8.0f + 6.3f * std::cos(a), 8.0f + 6.3f * std::sin(a)}
            };
            pen.line(ray, 2);
        }
        break;
    }
    case Icon::Quality: {
        const ImVec2 slash[] = { {4.0f, 2.5f}, {12.0f, 13.5f} };
        pen.line(slash, 2);
        break;
    }
    case Icon::Fx: {
        const ImVec2 fStem[] = { {7.0f, 3.0f}, {5.6f, 3.0f}, {4.6f, 4.2f}, {4.6f, 13.0f} };
        const ImVec2 fBar[] = { {3.0f, 7.0f}, {7.0f, 7.0f} };
        const ImVec2 x1[] = { {8.5f, 7.0f}, {13.0f, 13.0f} };
        const ImVec2 x2[] = { {13.0f, 7.0f}, {8.5f, 13.0f} };
        pen.line(fStem, 4);
        pen.line(fBar, 2);
        pen.line(x1, 2);
        pen.line(x2, 2);
        break;
    }
    case Icon::FrameBlend:
        pen.rect(2.0f, 4.0f, 9.0f, 11.0f);
        pen.rect(7.0f, 5.0f, 14.0f, 12.0f);
        break;
    case Icon::MotionBlur:
        pen.circle(10.5f, 8.0f, 3.5f);
        pen.circle(7.0f, 8.0f, 3.5f);
        pen.circle(4.0f, 8.0f, 3.0f);
        break;
    case Icon::Adjustment: {
        pen.circle(8.0f, 8.0f, 5.5f);
        const ImVec2 half[] = {
            {8.0f, 2.5f}, {8.0f, 13.5f}, {5.2f, 12.7f}, {3.2f, 10.6f},
            {2.5f, 8.0f}, {3.2f, 5.4f}, {5.2f, 3.3f}
        };
        pen.fill(half, 7);
        break;
    }
    case Icon::Cube: {
        const ImVec2 cube[] = { {8.0f, 2.0f}, {13.5f, 5.0f}, {13.5f, 11.0f}, {8.0f, 14.0f}, {2.5f, 11.0f}, {2.5f, 5.0f} };
        pen.closed(cube, 6);
        const ImVec2 topCrease[] = { {2.5f, 5.0f}, {8.0f, 8.0f}, {13.5f, 5.0f} };
        pen.line(topCrease, 3);
        const ImVec2 stem[] = { {8.0f, 8.0f}, {8.0f, 14.0f} };
        pen.line(stem, 2);
        break;
    }
    case Icon::Draft3D: {
        const ImVec2 cube[] = { {8.0f, 2.0f}, {13.5f, 5.0f}, {13.5f, 11.0f}, {8.0f, 14.0f}, {2.5f, 11.0f}, {2.5f, 5.0f} };
        pen.closed(cube, 6);
        const ImVec2 bar[] = { {5.0f, 9.0f}, {11.0f, 9.0f} };
        pen.line(bar, 2);
        break;
    }
    case Icon::Graph: {
        const ImVec2 curve[] = { {2.0f, 13.0f}, {5.0f, 12.0f}, {7.0f, 8.0f}, {9.0f, 4.5f}, {11.5f, 3.2f}, {14.0f, 3.0f} };
        const ImVec2 axes[] = { {2.0f, 2.0f}, {2.0f, 14.0f}, {14.0f, 14.0f} };
        pen.line(curve, 6);
        pen.line(axes, 3);
        break;
    }
    case Icon::Stopwatch: {
        pen.circle(8.0f, 9.0f, 5.2f);
        const ImVec2 hand[] = { {8.0f, 9.0f}, {8.0f, 5.8f} };
        const ImVec2 topBar[] = { {6.5f, 2.0f}, {9.5f, 2.0f} };
        const ImVec2 topStem[] = { {8.0f, 2.0f}, {8.0f, 3.8f} };
        pen.line(hand, 2);
        pen.line(topBar, 2);
        pen.line(topStem, 2);
        break;
    }
    case Icon::Keyframe: {
        const ImVec2 diamond[] = { {8.0f, 2.5f}, {13.5f, 8.0f}, {8.0f, 13.5f}, {2.5f, 8.0f} };
        pen.fill(diamond, 4);
        break;
    }
    case Icon::PickWhip:
        pen.arc(8.0f, 8.0f, 5.5f, 0.0f, 300.0f);
        pen.arc(8.0f, 8.0f, 3.0f, 40.0f, 340.0f);
        pen.dot(8.0f, 8.0f, 1.0f);
        break;
    case Icon::Flowchart: {
        pen.rect(1.5f, 2.5f, 6.0f, 6.5f);
        pen.rect(10.0f, 9.5f, 14.5f, 13.5f);
        const ImVec2 conn[] = { {6.0f, 4.5f}, {12.25f, 4.5f}, {12.25f, 9.5f} };
        pen.line(conn, 3);
        break;
    }
    case Icon::MarkerBin: {
        const ImVec2 marker[] = { {5.0f, 2.0f}, {11.0f, 2.0f}, {11.0f, 7.5f}, {8.0f, 10.5f}, {5.0f, 7.5f} };
        pen.fill(marker, 5);
        const ImVec2 tray[] = { {2.0f, 9.0f}, {2.0f, 14.0f}, {14.0f, 14.0f}, {14.0f, 9.0f} };
        pen.line(tray, 4);
        break;
    }
    case Icon::Folder: {
        const ImVec2 folder[] = { {1.5f, 4.0f}, {6.0f, 4.0f}, {7.5f, 5.5f}, {14.5f, 5.5f}, {14.5f, 13.0f}, {1.5f, 13.0f} };
        pen.closed(folder, 6);
        break;
    }
    case Icon::Comp:
        pen.rect(2.0f, 3.0f, 14.0f, 13.0f);
        pen.rect_fill(4.0f, 5.0f, 8.0f, 9.0f);
        pen.rect(7.0f, 7.5f, 12.0f, 11.0f);
        break;
    case Icon::Footage: {
        pen.rect(2.0f, 3.0f, 14.0f, 13.0f);
        for (float y : {5.0f, 8.0f, 11.0f}) {
            pen.dot(3.8f, y, 0.6f);
            pen.dot(12.2f, y, 0.6f);
        }
        const ImVec2 l1[] = { {5.5f, 3.0f}, {5.5f, 13.0f} };
        const ImVec2 l2[] = { {10.5f, 3.0f}, {10.5f, 13.0f} };
        pen.line(l1, 2);
        pen.line(l2, 2);
        break;
    }
    case Icon::Image: {
        pen.rect(2.0f, 3.0f, 14.0f, 13.0f);
        const ImVec2 mtn[] = { {2.5f, 11.5f}, {6.0f, 8.0f}, {9.0f, 10.5f}, {11.0f, 9.0f}, {13.5f, 11.5f} };
        pen.line(mtn, 5);
        pen.dot(10.5f, 5.8f, 1.1f);
        break;
    }
    case Icon::Audio: {
        const ImVec2 stem[] = { {6.0f, 12.0f}, {6.0f, 3.5f}, {13.0f, 2.5f}, {13.0f, 10.5f} };
        pen.line(stem, 4);
        pen.dot(4.4f, 12.0f, 1.8f);
        pen.dot(11.4f, 10.5f, 1.8f);
        break;
    }
    case Icon::Solid:
        pen.rect_fill(3.0f, 3.0f, 13.0f, 13.0f);
        break;
    case Icon::TextLayer: {
        const ImVec2 top[] = { {3.5f, 3.5f}, {12.5f, 3.5f} };
        const ImVec2 stem[] = { {8.0f, 3.5f}, {8.0f, 13.0f} };
        pen.line(top, 2);
        pen.line(stem, 2);
        break;
    }
    case Icon::ShapeLayer: {
        auto pts = pen.star(8.0f, 8.6f, 6.4f, 2.7f, 5);
        pen.closed(pts.data(), (int)pts.size());
        break;
    }
    case Icon::Null:
        pen.rect(3.0f, 3.0f, 13.0f, 13.0f);
        break;
    case Icon::Camera: {
        pen.rect(1.5f, 5.0f, 10.5f, 12.0f);
        const ImVec2 cone[] = { {10.5f, 7.5f}, {14.5f, 5.5f}, {14.5f, 11.5f}, {10.5f, 9.5f} };
        pen.closed(cone, 4);
        break;
    }
    case Icon::Light: {
        pen.circle(8.0f, 6.5f, 3.8f);
        const ImVec2 base[] = { {6.3f, 10.0f}, {6.3f, 12.5f}, {9.7f, 12.5f}, {9.7f, 10.0f} };
        const ImVec2 contact[] = { {6.8f, 14.2f}, {9.2f, 14.2f} };
        pen.line(base, 4);
        pen.line(contact, 2);
        break;
    }
    case Icon::Search: {
        pen.circle(6.8f, 6.8f, 4.0f);
        const ImVec2 handle[] = { {9.8f, 9.8f}, {13.5f, 13.5f} };
        pen.line(handle, 2);
        break;
    }
    case Icon::Trash: {
        const ImVec2 rim[] = { {2.5f, 4.0f}, {13.5f, 4.0f} };
        const ImVec2 body[] = { {4.0f, 4.0f}, {5.0f, 14.0f}, {11.0f, 14.0f}, {12.0f, 4.0f} };
        const ImVec2 handle[] = { {6.0f, 4.0f}, {6.5f, 2.0f}, {9.5f, 2.0f}, {10.0f, 4.0f} };
        pen.line(rim, 2);
        pen.closed(body, 4);
        pen.line(handle, 4);
        break;
    }
    case Icon::NewFolder: {
        const ImVec2 folder[] = { {1.5f, 4.0f}, {6.0f, 4.0f}, {7.5f, 5.5f}, {14.5f, 5.5f}, {14.5f, 13.0f}, {1.5f, 13.0f} };
        pen.closed(folder, 6);
        const ImVec2 vert[] = { {8.0f, 7.5f}, {8.0f, 11.5f} };
        const ImVec2 horiz[] = { {6.0f, 9.5f}, {10.0f, 9.5f} };
        pen.line(vert, 2);
        pen.line(horiz, 2);
        break;
    }
    case Icon::NewComp: {
        pen.rect(2.0f, 3.0f, 14.0f, 13.0f);
        const ImVec2 vert[] = { {8.0f, 5.5f}, {8.0f, 10.5f} };
        const ImVec2 horiz[] = { {5.5f, 8.0f}, {10.5f, 8.0f} };
        pen.line(vert, 2);
        pen.line(horiz, 2);
        break;
    }
    case Icon::Gear: {
        pen.circle(8.0f, 8.0f, 2.2f);
        for (int i = 0; i < 8; ++i) {
            float a = TAU * (float)i / 8.0f;
            const ImVec2 tooth[] = {
                {8.0f + 4.0f * std::cos(a), 8.0f + 4.0f * std::sin(a)},
                {8.0f + 6.0f * std::cos(a), 8.0f + 6.0f * std::sin(a)}
            };
            pen.line(tooth, 2);
        }
        pen.circle(8.0f, 8.0f, 4.2f);
        break;
    }
    case Icon::StrokeWidth:
        pen.rect_fill(2.5f, 3.0f, 13.5f, 3.75f);
        pen.rect_fill(2.5f, 6.5f, 13.5f, 8.0f);
        pen.rect_fill(2.5f, 10.5f, 13.5f, 13.0f);
        break;
    case Icon::Hamburger:
        for (float y : {4.5f, 8.0f, 11.5f}) {
            const ImVec2 line[] = { {3.0f, y}, {13.0f, y} };
            pen.line(line, 2);
        }
        break;
    case Icon::ChevronDown: {
        const ImVec2 tri[] = { {4.0f, 6.0f}, {12.0f, 6.0f}, {8.0f, 11.0f} };
        pen.fill(tri, 3);
        break;
    }
    case Icon::ChevronRight: {
        const ImVec2 tri[] = { {6.0f, 4.0f}, {11.0f, 8.0f}, {6.0f, 12.0f} };
        pen.fill(tri, 3);
        break;
    }
    case Icon::ChevronLeft: {
        const ImVec2 tri[] = { {10.0f, 4.0f}, {5.0f, 8.0f}, {10.0f, 12.0f} };
        pen.fill(tri, 3);
        break;
    }
    case Icon::Play: {
        const ImVec2 tri[] = { {4.5f, 2.5f}, {13.0f, 8.0f}, {4.5f, 13.5f} };
        pen.fill(tri, 3);
        break;
    }
    case Icon::Pause:
        pen.rect_fill(4.0f, 3.0f, 6.8f, 13.0f);
        pen.rect_fill(9.2f, 3.0f, 12.0f, 13.0f);
        break;
    case Icon::StepBack: {
        const ImVec2 tri[] = { {12.5f, 3.5f}, {6.0f, 8.0f}, {12.5f, 12.5f} };
        pen.fill(tri, 3);
        pen.rect_fill(3.5f, 3.5f, 5.0f, 12.5f);
        break;
    }
    case Icon::StepFwd: {
        const ImVec2 tri[] = { {3.5f, 3.5f}, {10.0f, 8.0f}, {3.5f, 12.5f} };
        pen.fill(tri, 3);
        pen.rect_fill(11.0f, 3.5f, 12.5f, 12.5f);
        break;
    }
    case Icon::First: {
        const ImVec2 tri1[] = { {13.5f, 3.5f}, {8.0f, 8.0f}, {13.5f, 12.5f} };
        const ImVec2 tri2[] = { {8.5f, 3.5f}, {3.0f, 8.0f}, {8.5f, 12.5f} };
        pen.fill(tri1, 3);
        pen.fill(tri2, 3);
        pen.rect_fill(1.8f, 3.5f, 3.0f, 12.5f);
        break;
    }
    case Icon::Last: {
        const ImVec2 tri1[] = { {2.5f, 3.5f}, {8.0f, 8.0f}, {2.5f, 12.5f} };
        const ImVec2 tri2[] = { {7.5f, 3.5f}, {13.0f, 8.0f}, {7.5f, 12.5f} };
        pen.fill(tri1, 3);
        pen.fill(tri2, 3);
        pen.rect_fill(13.0f, 3.5f, 14.2f, 12.5f);
        break;
    }
    case Icon::Loop: {
        pen.arc(8.0f, 8.0f, 5.0f, 200.0f, 520.0f);
        const ImVec2 arrow[] = { {1.8f, 5.5f}, {5.2f, 5.5f}, {3.2f, 8.5f} };
        pen.fill(arrow, 3);
        break;
    }
    case Icon::Snapshot: {
        const ImVec2 cam[] = {
            {2.0f, 5.0f}, {5.0f, 5.0f}, {6.0f, 3.5f}, {10.0f, 3.5f},
            {11.0f, 5.0f}, {14.0f, 5.0f}, {14.0f, 12.5f}, {2.0f, 12.5f}
        };
        pen.closed(cam, 8);
        pen.circle(8.0f, 8.7f, 2.3f);
        break;
    }
    case Icon::ShowSnapshot: {
        const ImVec2 eye[] = {
            {1.5f, 8.0f}, {4.0f, 5.0f}, {8.0f, 3.8f}, {12.0f, 5.0f},
            {14.5f, 8.0f}, {12.0f, 11.0f}, {8.0f, 12.2f}, {4.0f, 11.0f}, {1.5f, 8.0f}
        };
        pen.line(eye, 9);
        pen.rect(6.0f, 6.5f, 10.0f, 9.5f);
        break;
    }
    case Icon::Grid: {
        pen.rect(2.0f, 2.0f, 14.0f, 14.0f);
        const ImVec2 v1[] = { {6.0f, 2.0f}, {6.0f, 14.0f} };
        const ImVec2 v2[] = { {10.0f, 2.0f}, {10.0f, 14.0f} };
        const ImVec2 h1[] = { {2.0f, 6.0f}, {14.0f, 6.0f} };
        const ImVec2 h2[] = { {2.0f, 10.0f}, {14.0f, 10.0f} };
        pen.line(v1, 2);
        pen.line(v2, 2);
        pen.line(h1, 2);
        pen.line(h2, 2);
        break;
    }
    case Icon::MaskVis:
        pen.rect(2.0f, 2.0f, 14.0f, 14.0f);
        pen.ellipse(8.0f, 8.0f, 3.8f, 3.8f);
        break;
    case Icon::Checker:
        pen.rect(2.0f, 2.0f, 14.0f, 14.0f);
        pen.rect_fill(2.0f, 2.0f, 8.0f, 8.0f);
        pen.rect_fill(8.0f, 8.0f, 14.0f, 14.0f);
        break;
    case Icon::Region:
        pen.rect(2.5f, 2.5f, 13.5f, 13.5f);
        pen.rect(5.5f, 5.5f, 10.5f, 10.5f);
        break;
    case Icon::ExtendedViewer: {
        pen.rect(5.0f, 5.0f, 11.0f, 11.0f);
        struct TickSpec { float x, y, dx, dy; };
        const TickSpec ticks[] = {
            {2.0f, 2.0f, 1.0f, 1.0f},
            {14.0f, 2.0f, -1.0f, 1.0f},
            {2.0f, 14.0f, 1.0f, -1.0f},
            {14.0f, 14.0f, -1.0f, -1.0f}
        };
        for (const auto& t : ticks) {
            const ImVec2 l[] = { {t.x + t.dx * 3.0f, t.y}, {t.x, t.y}, {t.x, t.y + t.dy * 3.0f} };
            pen.line(l, 3);
        }
        break;
    }
    case Icon::Plus: {
        const ImVec2 vert[] = { {8.0f, 3.0f}, {8.0f, 13.0f} };
        const ImVec2 horiz[] = { {3.0f, 8.0f}, {13.0f, 8.0f} };
        pen.line(vert, 2);
        pen.line(horiz, 2);
        break;
    }
    case Icon::Minus: {
        const ImVec2 horiz[] = { {3.0f, 8.0f}, {13.0f, 8.0f} };
        pen.line(horiz, 2);
        break;
    }
    case Icon::Close: {
        const ImVec2 d1[] = { {4.0f, 4.0f}, {12.0f, 12.0f} };
        const ImVec2 d2[] = { {12.0f, 4.0f}, {4.0f, 12.0f} };
        pen.line(d1, 2);
        pen.line(d2, 2);
        break;
    }
    case Icon::Link: {
        pen.arc(5.5f, 8.0f, 3.0f, 90.0f, 270.0f);
        pen.arc(10.5f, 8.0f, 3.0f, -90.0f, 90.0f);
        const ImVec2 l1[] = { {5.5f, 5.0f}, {8.0f, 5.0f} };
        const ImVec2 l2[] = { {5.5f, 11.0f}, {8.0f, 11.0f} };
        const ImVec2 l3[] = { {8.0f, 5.0f}, {10.5f, 5.0f} };
        const ImVec2 l4[] = { {8.0f, 11.0f}, {10.5f, 11.0f} };
        pen.line(l1, 2);
        pen.line(l2, 2);
        pen.line(l3, 2);
        pen.line(l4, 2);
        break;
    }
    case Icon::MountainSmall: {
        const ImVec2 tri[] = { {4.0f, 12.0f}, {8.0f, 7.0f}, {12.0f, 12.0f} };
        pen.fill(tri, 3);
        break;
    }
    case Icon::MountainLarge: {
        const ImVec2 tri1[] = { {1.0f, 13.0f}, {7.0f, 4.0f}, {13.0f, 13.0f} };
        const ImVec2 tri2[] = { {8.0f, 13.0f}, {11.5f, 8.0f}, {15.0f, 13.0f} };
        pen.fill(tri1, 3);
        pen.fill(tri2, 3);
        break;
    }
    case Icon::Fullscreen: {
        const ImVec2 c1[] = { {2.0f, 6.0f}, {2.0f, 2.0f}, {6.0f, 2.0f} };
        const ImVec2 c2[] = { {10.0f, 2.0f}, {14.0f, 2.0f}, {14.0f, 6.0f} };
        const ImVec2 c3[] = { {14.0f, 10.0f}, {14.0f, 14.0f}, {10.0f, 14.0f} };
        const ImVec2 c4[] = { {6.0f, 14.0f}, {2.0f, 14.0f}, {2.0f, 10.0f} };
        pen.line(c1, 3);
        pen.line(c2, 3);
        pen.line(c3, 3);
        pen.line(c4, 3);
        break;
    }
    case Icon::Channel:
        pen.dot_colored(6.0f, 6.5f, 3.6f, IM_COL32(226, 58, 58, 255));
        pen.dot_colored(10.0f, 6.5f, 3.6f, IM_COL32(58, 200, 88, 255));
        pen.dot_colored(8.0f, 10.0f, 3.6f, IM_COL32(64, 120, 235, 255));
        break;
    case Icon::Exposure: {
        pen.circle(8.0f, 8.0f, 5.5f);
        const ImVec2 half[] = { {8.0f, 2.5f}, {13.5f, 8.0f}, {8.0f, 13.5f} };
        pen.fill(half, 3);
        break;
    }
    case Icon::Chat: {
        const ImVec2 bubble[] = {
            {2.0f, 3.0f}, {14.0f, 3.0f}, {14.0f, 11.0f}, {7.0f, 11.0f},
            {4.0f, 14.0f}, {4.5f, 11.0f}, {2.0f, 11.0f}
        };
        pen.closed(bubble, 7);
        pen.dot(5.3f, 7.0f, 0.95f);
        pen.dot(8.0f, 7.0f, 0.95f);
        pen.dot(10.7f, 7.0f, 0.95f);
        break;
    }
    case Icon::Globe: {
        pen.circle(8.0f, 8.0f, 6.0f);
        pen.ellipse(8.0f, 8.0f, 2.6f, 6.0f);
        const ImVec2 eq[] = { {2.0f, 8.0f}, {14.0f, 8.0f} };
        const ImVec2 n[] = { {3.2f, 4.8f}, {12.8f, 4.8f} };
        const ImVec2 s[] = { {3.2f, 11.2f}, {12.8f, 11.2f} };
        pen.line(eq, 2);
        pen.line(n, 2);
        pen.line(s, 2);
        break;
    }
    case Icon::Code: {
        const ImVec2 left[] = { {5.0f, 4.0f}, {1.8f, 8.0f}, {5.0f, 12.0f} };
        const ImVec2 right[] = { {11.0f, 4.0f}, {14.2f, 8.0f}, {11.0f, 12.0f} };
        const ImVec2 slash[] = { {9.4f, 2.8f}, {6.6f, 13.2f} };
        pen.line(left, 3);
        pen.line(right, 3);
        pen.line(slash, 2);
        break;
    }
    case Icon::Sparkle:
        for (const auto& piece : SPARK_PIECES) {
            pen.fill(piece.pts, piece.count);
        }
        break;
    case Icon::Monitor: {
        pen.rrect(1.8f, 2.5f, 14.2f, 11.0f, 1.2f);
        const ImVec2 stem[] = { {8.0f, 11.0f}, {8.0f, 13.8f} };
        const ImVec2 base[] = { {5.0f, 13.8f}, {11.0f, 13.8f} };
        pen.line(stem, 2);
        pen.line(base, 2);
        break;
    }
    case Icon::Sun: {
        pen.circle(8.0f, 8.0f, 2.8f);
        for (int i = 0; i < 8; ++i) {
            float a = FRAC_PI_4 * (float)i;
            float c = std::cos(a), s = std::sin(a);
            const ImVec2 ray[] = { {8.0f + 4.6f * c, 8.0f + 4.6f * s}, {8.0f + 6.4f * c, 8.0f + 6.4f * s} };
            pen.line(ray, 2);
        }
        break;
    }
    case Icon::Moon:
        pen.arc(8.0f, 8.0f, 5.8f, -20.0f, 250.0f);
        pen.arc(10.6f, 2.4f, 4.6f, 52.0f, 178.0f);
        break;
    case Icon::PenAdd:
    case Icon::PenDelete: {
        const ImVec2 nib[] = { {6.0f, 2.5f}, {9.0f, 8.0f}, {6.0f, 12.0f}, {3.0f, 8.0f} };
        const ImVec2 slit[] = { {6.0f, 2.5f}, {6.0f, 7.0f} };
        const ImVec2 h[] = { {10.0f, 11.5f}, {14.5f, 11.5f} };
        pen.closed(nib, 4);
        pen.line(slit, 2);
        pen.line(h, 2);
        if (icon == Icon::PenAdd) {
            const ImVec2 v[] = { {12.25f, 9.25f}, {12.25f, 13.75f} };
            pen.line(v, 2);
        }
        break;
    }
    case Icon::PenConvert: {
        const ImVec2 caret[] = { {2.5f, 13.0f}, {8.0f, 3.5f}, {13.5f, 13.0f} };
        pen.line(caret, 3);
        pen.dot(8.0f, 3.5f, 1.4f);
        break;
    }
    case Icon::MaskFeather: {
        const ImVec2 nib[] = { {6.0f, 2.5f}, {9.0f, 8.0f}, {6.0f, 12.0f}, {3.0f, 8.0f} };
        const ImVec2 slit[] = { {6.0f, 2.5f}, {6.0f, 7.0f} };
        const ImVec2 feather[] = { {10.5f, 4.0f}, {12.5f, 6.5f}, {13.5f, 10.0f}, {12.5f, 13.5f} };
        pen.closed(nib, 4);
        pen.line(slit, 2);
        pen.line(feather, 4);
        break;
    }
    }
}

void paintCentered(ImDrawList* dl, ImVec2 center, float size, Icon icon, ImU32 color, float width) {
    float half = size * 0.5f;
    paint(dl, ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), icon, color, width);
}

// --- keyframe_shapes and keyframe() ---
// after EffectCraft crates/ui-egui/src/icons.rs keyframe_shapes(), keyframe(), keyframe_bounds()
std::vector<std::vector<ImVec2>> keyframeShapes(ImVec2 center, float size, KeyHalf left, KeyHalf right) {
    float r = size / 2.0f;
    std::vector<std::vector<ImVec2>> out;
    out.reserve(2);

    auto doHalf = [&](float side, KeyHalf h) {
        std::vector<ImVec2> pts;
        switch (h) {
        case KeyHalf::Linear:
            if (side < 0.0f) {
                pts = { ImVec2(center.x, center.y - r), center, ImVec2(center.x, center.y + r), ImVec2(center.x - r, center.y) };
            } else {
                pts = { ImVec2(center.x, center.y - r), ImVec2(center.x + r, center.y), ImVec2(center.x, center.y + r), center };
            }
            break;
        case KeyHalf::Hold:
            if (side < 0.0f) {
                pts = {
                    ImVec2(center.x - r * 0.8f, center.y - r * 0.8f),
                    ImVec2(center.x, center.y - r * 0.8f),
                    ImVec2(center.x, center.y + r * 0.8f),
                    ImVec2(center.x - r * 0.8f, center.y + r * 0.8f)
                };
            } else {
                pts = {
                    ImVec2(center.x, center.y - r * 0.8f),
                    ImVec2(center.x + r * 0.8f, center.y - r * 0.8f),
                    ImVec2(center.x + r * 0.8f, center.y + r * 0.8f),
                    ImVec2(center.x, center.y + r * 0.8f)
                };
            }
            break;
        case KeyHalf::Bezier:
            if (side < 0.0f) {
                pts = {
                    ImVec2(center.x - r * 0.75f, center.y - r),
                    ImVec2(center.x, center.y - r * 0.35f),
                    ImVec2(center.x, center.y + r * 0.35f),
                    ImVec2(center.x - r * 0.75f, center.y + r)
                };
            } else {
                pts = {
                    ImVec2(center.x, center.y - r * 0.35f),
                    ImVec2(center.x + r * 0.75f, center.y - r),
                    ImVec2(center.x + r * 0.75f, center.y + r),
                    ImVec2(center.x, center.y + r * 0.35f)
                };
            }
            break;
        case KeyHalf::Auto: {
            const int n = 10;
            pts.push_back(ImVec2(center.x, center.y - r * 0.8f));
            pts.push_back(ImVec2(center.x, center.y + r * 0.8f));
            for (int i = 1; i < n; ++i) {
                float t = PI * (float)i / (float)n;
                pts.push_back(ImVec2(center.x + side * r * 0.8f * std::sin(t), center.y + r * 0.8f * std::cos(t)));
            }
            break;
        }
        }
        out.push_back(std::move(pts));
    };

    doHalf(-1.0f, left);
    doHalf(1.0f, right);
    return out;
}

void drawKeyframe(ImDrawList* dl, ImVec2 center, float size, KeyHalf left, KeyHalf right, ImU32 fill) {
    auto shapes = keyframeShapes(center, size, left, right);
    for (const auto& pts : shapes) {
        if (pts.size() >= 3) {
            dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
            dl->AddPolyline(pts.data(), (int)pts.size(), fill, ImDrawFlags_Closed, 0.5f);
        }
    }
}

RectBounds keyframeBounds(ImVec2 center, float size, KeyHalf left, KeyHalf right) {
    auto shapes = keyframeShapes(center, size, left, right);
    ImVec2 bMin(1e9f, 1e9f), bMax(-1e9f, -1e9f);
    for (const auto& poly : shapes) {
        for (const auto& pt : poly) {
            bMin.x = std::min(bMin.x, pt.x);
            bMin.y = std::min(bMin.y, pt.y);
            bMax.x = std::max(bMax.x, pt.x);
            bMax.y = std::max(bMax.y, pt.y);
        }
    }
    // Expand by 0.25 for 0.5 hairline stroke
    bMin.x -= 0.25f; bMin.y -= 0.25f;
    bMax.x += 0.25f; bMax.y += 0.25f;
    return RectBounds{ bMin, bMax };
}

// --- Interactive icon button ---
// after EffectCraft crates/ui-egui/src/widgets.rs icon_button()
bool iconButton(const char* str_id, Icon icon, bool active, ImVec2 size, ImU32 tint) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // Background and stroke
    static const ImU32 C_DEEP = IM_COL32(0x00, 0x51, 0x84, 255);
    static const ImU32 C_BLUE = IM_COL32(0x00, 0x7b, 0xb7, 255);
    static const ImU32 C_HOVER = IM_COL32(0x30, 0x30, 0x30, 255);
    static const ImU32 C_TEXT = IM_COL32(0xe0, 0xe0, 0xe0, 255);
    static const ImU32 C_DIM = IM_COL32(0x80, 0x80, 0x80, 255);

    if (active) {
        dl->AddRectFilled(bb.Min, bb.Max, C_DEEP, 2.0f);
        dl->AddRect(bb.Min, bb.Max, C_BLUE, 2.0f);
    } else if (held) {
        dl->AddRectFilled(bb.Min, bb.Max, C_BLUE, 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, C_HOVER, 2.0f);
    }

    ImU32 iconColor = (tint != 0) ? tint : (active ? C_TEXT : (hovered ? C_TEXT : C_DIM));
    // Draw icon centered, size 16x16
    paintCentered(dl, ImVec2(bb.Min.x + size.x * 0.5f, bb.Min.y + size.y * 0.5f), 16.0f, icon, iconColor);

    return pressed;
}

} // namespace Icons
