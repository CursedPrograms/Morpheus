// Morpheus - the splash: drawn with Dear ImGui's draw list (sharp at any DPI).
// Three layer parallelograms form the logo, then the name "Morpheus" and credits.
// Atelier greys + Atelier blue #007bb7; solid fills, no gradients.
#include "splash.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>

#ifndef MORPHEUS_VERSION
#define MORPHEUS_VERSION "0.1.0"
#endif

namespace Splash {

static ImFont *s_title = nullptr, *s_small = nullptr;

void setFonts(ImFont* title, ImFont* small) { s_title = title; s_small = small; }

// Colour palette: Atelier greys + Atelier blue
static const ImU32
    C_BG    = IM_COL32(0x1a, 0x1a, 0x1a, 255),
    C_LINE  = IM_COL32(0x33, 0x33, 0x33, 255),
    C_TEXT  = IM_COL32(0xd6, 0xd6, 0xd6, 255),
    C_DIM   = IM_COL32(0x9a, 0x9a, 0x9a, 255),
    C_BLUE  = IM_COL32(0x00, 0x7b, 0xb7, 255),
    C_GREY  = IM_COL32(0x9e, 0x9e, 0x9e, 255),
    C_DARK  = IM_COL32(0x3a, 0x3a, 0x3a, 255),
    C_GHOST = IM_COL32(0xd6, 0xd6, 0xd6, 150);

// Four corners of a parallelogram centred at c, w wide, h high, leaning right by lean pixels.
static void layerPts(ImVec2 c, float w, float h, float lean, ImVec2 out[4]) {
    out[0] = {c.x - w * 0.5f + lean * 0.5f, c.y - h * 0.5f};
    out[1] = {c.x + w * 0.5f + lean * 0.5f, c.y - h * 0.5f};
    out[2] = {c.x + w * 0.5f - lean * 0.5f, c.y + h * 0.5f};
    out[3] = {c.x - w * 0.5f - lean * 0.5f, c.y + h * 0.5f};
}

// Dashed line from a to b (simulates "marching ants" at rest).
static void dashed(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float dash = 7, gap = 5;
    for (float t = 0; t < len; t += dash + gap) {
        const float e = t + dash < len ? t + dash : len;
        dl->AddLine(
            {a.x + dx * t / len, a.y + dy * t / len},
            {a.x + dx * e / len, a.y + dy * e / len},
            col, 1.0f);
    }
}

// Draw text via the draw list (lets us render outside an ImGui window).
static void txt(ImDrawList* dl, ImFont* f, float sz, ImVec2 p, ImU32 col, const char* s) {
    if (!f) f = ImGui::GetFont();
    dl->AddText(f, sz, p, col, s);
}

static float txtW(ImFont* f, float sz, const char* s) {
    if (!f) f = ImGui::GetFont();
    return f->CalcTextSizeA(sz, FLT_MAX, 0, s).x;
}

void draw(ImDrawList* dl, float x, float y, const char* status, float progress) {
    const ImVec2 tl{x, y}, br{x + W, y + H};
    dl->PushClipRect(tl, br, true);
    dl->AddRectFilled(tl, br, C_BG);

    // Three stacked layer parallelograms: dark bottom, grey middle, blue top (the "dream layers" logo).
    const float lw = 280, lh = 90, lean = 80, cx = x + 500;
    struct { float cy; ImU32 col; } layers[] = {
        {y + 255, C_DARK}, {y + 180, C_GREY}, {y + 105, C_BLUE}
    };
    ImVec2 pts[4];
    for (const auto& l : layers) {
        layerPts({cx, l.cy}, lw, lh, lean, pts);
        dl->AddConvexPolyFilled(pts, 4, l.col);
    }
    // The top layer also shown mid-air with a dashed ghost outline.
    layerPts({cx + 18, y + 105 - 24}, lw, lh, lean, pts);
    for (int i = 0; i < 4; ++i)
        dashed(dl, pts[i], pts[(i + 1) % 4], C_GHOST);

    // Name: "Morpheus" in grey with the last two letters in blue.
    const float ts = 50, tx = x + 40, ty = y + 92;
    const char* p1 = "Morphe";
    const char* p2 = "us";
    const float p1w = txtW(s_title, ts, p1);
    txt(dl, s_title, ts, {tx, ty}, C_TEXT, p1);
    txt(dl, s_title, ts, {tx + p1w, ty}, C_BLUE, p2);
    txt(dl, nullptr, 16, {tx + 2, ty + 62}, C_DIM, "Version " MORPHEUS_VERSION);
    txt(dl, nullptr, 16, {tx + 2, ty + 84}, C_DIM, "Motion graphics and compositing by Cursed Entertainment");

    // Small credits.
    const float cs = 13, cy = y + 306;
    txt(dl, s_small, cs, {tx, cy},      C_DIM, "After EffectCraft (crates/project, crates/time, crates/keyframe).");
    txt(dl, s_small, cs, {tx, cy + 17}, C_DIM, "Built with Dear ImGui, SDL2 and stb.");
    txt(dl, s_small, cs, {tx, cy + 34}, C_DIM, "\xc2\xa9 Cursed Entertainment");

    // Loading status text and progress bar.
    if (status)
        txt(dl, nullptr, 16, {tx, y + 368}, C_TEXT, status);
    if (progress >= 0.0f) {
        const float p = progress > 1.0f ? 1.0f : progress;
        dl->AddRectFilled({x, br.y - 3}, br, C_LINE);
        dl->AddRectFilled({x, br.y - 3}, {x + W * p, br.y}, C_BLUE);
    }
    dl->AddRect(tl, br, C_LINE);
    dl->PopClipRect();
}

}  // namespace Splash
