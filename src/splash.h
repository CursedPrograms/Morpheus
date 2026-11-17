// Morpheus - the splash: shown while loading and in Help > About.
#pragma once

struct ImDrawList;
struct ImFont;

namespace Splash {
constexpr float W = 640, H = 400;
// Set the fonts loaded from the system font; pass nullptr to use ImGui's default.
void setFonts(ImFont* title, ImFont* small);
// Draw the splash W x H, top-left at (x, y).
// status: loading line at the bottom left (nullptr = none).
// progress 0..1: coloured bar along the bottom (below 0 = none).
void draw(ImDrawList* dl, float x, float y, const char* status, float progress);
}
