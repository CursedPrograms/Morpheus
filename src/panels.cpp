// Morpheus - panel layout matching the EffectCraft Default workspace.
// Ported from crates/ui-egui/src/dock.rs (workspace_root "Default"),
// crates/ui-egui/src/header.rs (tool bar), and each panel in crates/ui-egui/src/panels/.
//
// Default layout (after EffectCraft crates/ui-egui/src/dock.rs workspace_root):
//   [Header / Tools bar - full width, ~38 px]
//   [Left 300px | Centre flex | Right 300px]
//   Left:   Project / Effect Controls tabs (full height)
//   Centre: Composition / Layer tabs (top ~56%) over Timeline / Render Queue tabs
//   Right:  Preview tab (top ~76px) over Properties / Align / Audio tabs
//
// Label colours from EffectCraft (after crates/ui-egui/src/theme.rs):
//   None=grey, Red, Yellow, Aqua, Pink, Lavender, Peach, Sea Foam, Blue, Green, Purple, Orange, Brown, Fuchsia, Cyan, Sandstone, Dark Green
//
// Atelier colours: dark greys, #007bb7 accent, solid, no gradients.
//
// after EffectCraft (crates/ui-egui/src/dock.rs, crates/ui-egui/src/header.rs,
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

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

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
// These 16 entries correspond to Label::None..Label::Sandstone in order.
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

// Draw a slim horizontal divider line.
static void hLine(ImDrawList* dl, float x0, float x1, float y) {
    dl->AddLine({x0,y}, {x1,y}, C_LINE, DIVIDER);
}
// Draw a slim vertical divider line.
static void vLine(ImDrawList* dl, float x, float y0, float y1) {
    dl->AddLine({x,y0}, {x,y1}, C_LINE, DIVIDER);
}

// Draw a tab strip for `n` tabs; returns the index of the active one after interaction.
// after EffectCraft crates/ui-egui/src/dock.rs TAB_STRIP / craft-ui tabs
static int tabStrip(const char* tabs[], int n, int active,
                    float x, float y, float w, float rowH,
                    ImDrawList* dl, ImGuiIO& io) {
    // Tab bar background
    dl->AddRectFilled({x,y},{x+w, y+rowH}, C_TAB_BG);
    hLine(dl, x, x+w, y+rowH);

    const float tabW  = std::max(60.0f, w / n);
    int result = active;
    for (int i = 0; i < n; ++i) {
        const float tx = x + i * tabW;
        const float ty = y;
        const float tw = (i == n-1) ? (x+w-tx) : tabW;  // last tab fills remainder
        const bool on = (i == active);
        const ImVec2 tMin{tx, ty}, tMax{tx+tw, ty+rowH};
        dl->AddRectFilled(tMin, tMax, on ? C_TAB_ACT : C_TAB_BG);
        if (on) {
            // Active tab: blue top line
            dl->AddLine({tx,ty},{tx+tw,ty}, C_BLUE, 2.0f);
        }
        // Label
        ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
        const float lx = tx + (tw-ts.x)*0.5f;
        const float ly = ty + (rowH-ts.y)*0.5f;
        dl->AddText({lx,ly}, on ? C_TEXT : C_DIM, tabs[i]);
        // Click detection
        if (ImGui::IsMouseClicked(0)) {
            const ImVec2& mp = io.MousePos;
            if (mp.x >= tMin.x && mp.x < tMax.x && mp.y >= tMin.y && mp.y < tMax.y)
                result = i;
        }
        // Vertical divider between tabs
        if (i < n-1)
            vLine(dl, tx+tw, ty+2, ty+rowH-2);
    }
    return result;
}

// A panel menu hamburger icon ("≡") in the tab strip corner.
static void panelMenuButton(float rx, float cy, ImDrawList* dl) {
    for (int i = -1; i <= 1; ++i)
        dl->AddLine({rx-10, cy+i*4}, {rx-2, cy+i*4}, C_DIM, 1.0f);
}

// ---------- panel content helpers ----------
// Each function draws the contents of one named panel.

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
    const ImVec2 winPos = ImGui::GetWindowPos();

    // --- Thumbnail / details header (~74 px) ---
    const float headH = 74.0f;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p0, {p0.x+avW, p0.y+headH}, C_APP_BG);
    if (!proj->compositions.empty() && proj->activeComp >= 0) {
        const auto& comp = proj->compositions[(size_t)proj->activeComp];
        // Thumbnail placeholder rectangle
        dl->AddRectFilled({p0.x+8, p0.y+10}, {p0.x+104, p0.y+64}, C_WELL);
        dl->AddRect({p0.x+8, p0.y+10}, {p0.x+104, p0.y+64}, C_LINE);
        // Details text
        dl->AddText({p0.x+112, p0.y+12}, C_TEXT, comp.name.c_str());
        char dims[64];
        snprintf(dims, sizeof dims, "%d x %d (1.00)", comp.width, comp.height);
        dl->AddText({p0.x+112, p0.y+28}, C_DIM, dims);
        char fps[64];
        snprintf(fps, sizeof fps, "%.3g fps", comp.fps());
        dl->AddText({p0.x+112, p0.y+42}, C_DIM, fps);
        // Duration display using Tick formatting
        const Project::Tick dur = comp.duration;
        const Project::FrameRate fr = comp.frameRate;
        std::string tc = Project::format_timecode_ae(fr.frame_at(dur), fr, false);
        char durBuf[64]; snprintf(durBuf, sizeof durBuf, "\xce\x94 %s", tc.c_str());
        dl->AddText({p0.x+112, p0.y+56}, C_DIM, durBuf);
    } else {
        // "Select an item to see its details" hint (after EffectCraft project.rs details_header)
        ImVec2 hs = ImGui::CalcTextSize(L("Select an item to see its details"));
        dl->AddText({p0.x + (avW-hs.x)*0.5f, p0.y + (headH-hs.y)*0.5f},
                    C_FAINT, L("Select an item to see its details"));
    }
    ImGui::Dummy({avW, headH});
    hLine(dl, p0.x, p0.x+avW, p0.y+headH);

    // --- Search bar ---
    static char s_search[128] = {};
    ImGui::SetNextItemWidth(avW - 10);
    ImGui::InputTextWithHint("##projsearch", L("Search project"), s_search, sizeof s_search);
    hLine(dl, ImGui::GetWindowPos().x, ImGui::GetWindowPos().x+avW,
          ImGui::GetCursorScreenPos().y);

    // --- Column header: Name | Label | Type | Frame Rate ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs draw_header / COLUMNS
    const float hdrH = 18.0f;
    const ImVec2 hdrP = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(hdrP, {hdrP.x+avW, hdrP.y+hdrH}, C_APP_BG);
    static const char* colHdrs[] = { L("Name"), L("Type"), L("Frame Rate") };
    static const float colX[] = { 8.0f, avW*0.55f, avW*0.75f };
    for (int i = 0; i < 3; ++i)
        dl->AddText({hdrP.x+colX[i], hdrP.y+2}, C_DIM, colHdrs[i]);
    // Label diamond icon (matches EffectCraft keyframe icon for the Label column)
    {
        const float lx = hdrP.x + avW*0.47f + 4;
        const float ly = hdrP.y + hdrH*0.5f;
        dl->AddRectFilled({lx-4, ly-4},{lx+4, ly+4}, C_DIM);
    }
    ImGui::Dummy({avW, hdrH});
    hLine(dl, hdrP.x, hdrP.x+avW, hdrP.y+hdrH);

    // --- Item list ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs visible_rows
    const float listH = avH - headH - 22 - hdrH - 24 - 10;
    ImGui::BeginChild("##projlist", {avW, listH}, false,
                      ImGuiWindowFlags_NoScrollbar);
    if (proj->compositions.empty()) {
        ImGui::TextDisabled("%s", L("No compositions."));
    } else {
        for (int i = 0; i < (int)proj->compositions.size(); ++i) {
            const auto& comp = proj->compositions[(size_t)i];
            const bool active = (i == proj->activeComp);
            ImGui::PushID(i);
            const ImVec2 rp = ImGui::GetCursorScreenPos();
            // Label swatch (12x12 square, red for index 1, grey for others)
            const ImU32 swCol = labelColor(i % 17 + (i > 0 ? 1 : 0));
            dl->AddRectFilled({rp.x+2, rp.y+3}, {rp.x+14, rp.y+ROW_H-3}, swCol);
            ImGui::Dummy({16, ROW_H}); ImGui::SameLine(0,0);
            // Name + type, using Selectable for the whole row
            char rowBuf[256];
            snprintf(rowBuf, sizeof rowBuf, "  %s###item%d", comp.name.c_str(), i);
            if (active) ImGui::PushStyleColor(ImGuiCol_Text, col4(C_BLUE));
            if (ImGui::Selectable(rowBuf, active, 0, {avW-16, ROW_H})) {
                if (g_actions && g_actions->onSelectComp) g_actions->onSelectComp(i);
            }
            if (active) ImGui::PopStyleColor();
            // Type column
            const ImVec2 rp2 = {rp.x + avW*0.55f, rp.y};
            dl->AddText({rp2.x, rp2.y+2}, C_DIM, L("Composition"));
            // Frame rate column
            char fpsBuf[32];
            snprintf(fpsBuf, sizeof fpsBuf, "%.3g", comp.fps());
            dl->AddText({rp.x + avW*0.75f, rp.y+2}, C_DIM, fpsBuf);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    // --- Bottom bar: Interpret | New Folder | New Comp | bpc | Delete ---
    // after EffectCraft crates/ui-egui/src/panels/project.rs bottom bar buttons
    hLine(dl, ImGui::GetWindowPos().x, ImGui::GetWindowPos().x+avW,
          ImGui::GetCursorScreenPos().y);
    ImGui::Dummy({1, 3});
    if (ImGui::SmallButton(L("New Folder"))) {}
    ImGui::SameLine();
    if (ImGui::SmallButton(L("New Comp"))) {
        if (g_actions && g_actions->onNew) g_actions->onNew();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("8 bpc");
    ImGui::SameLine(avW - 60);
    if (ImGui::SmallButton(L("Delete"))) {}
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

    // --- Viewer bottom bar (shows below the canvas) ---
    // after EffectCraft crates/ui-egui/src/panels/viewer.rs viewer bottom bar
    const float barH = 24.0f;
    const float canvasH = avail.y - barH - 1;

    // Canvas area: checkerboard transparency grid
    // after EffectCraft crates/ui-egui/src/panels/viewer.rs transparency grid
    const float cs = 16;
    for (float y = 0; y < canvasH; y += cs)
        for (float x = 0; x < avail.x; x += cs) {
            const bool odd = ((int)(x/cs) + (int)(y/cs)) & 1;
            dl->AddRectFilled(
                {p.x+x, p.y+y},
                {p.x+x+(x+cs>avail.x?avail.x-x:cs),
                 p.y+y+(y+cs>canvasH?canvasH-y:cs)},
                odd ? IM_COL32(0x28,0x28,0x28,255)
                    : IM_COL32(0x1e,0x1e,0x1e,255));
        }
    // Composition name centred in the canvas
    const char* label = (proj && proj->activeComp >= 0)
        ? proj->compositions[(size_t)proj->activeComp].name.c_str()
        : L("No composition");
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({p.x+(avail.x-ts.x)*0.5f, p.y+(canvasH-ts.y)*0.5f}, C_DIM, label);
    ImGui::Dummy({avail.x, canvasH});

    hLine(dl, p.x, p.x+avail.x, p.y+canvasH);

    // --- Viewer bottom bar ---
    // Fit popup, grid, current time, snapshot, channels, resolution, transparency, fast previews
    // after EffectCraft crates/ui-egui/src/panels/viewer.rs bottom bar
    const ImVec2 bb = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(bb, {bb.x+avail.x, bb.y+barH}, C_APP_BG);

    // Zoom / Fit
    ImGui::PushStyleColor(ImGuiCol_Button, col4(C_APP_BG));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col4(C_DEEP));
    if (ImGui::SmallButton(L("Fit (100%)"))) {}
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    // Current time display using Tick
    std::string tcStr = "0:00:00:00";
    if (proj && proj->activeComp >= 0) {
        const auto& comp = proj->compositions[(size_t)proj->activeComp];
        const Project::FrameRate fr = comp.frameRate;
        tcStr = Project::format_timecode_ae(0, fr, false);
    }
    ImGui::TextColored(col4(C_BLUE), "%s", tcStr.c_str());
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    // Channels (R/G/B/A/RGB)
    ImGui::TextDisabled("RGB");
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::TextDisabled(L("Auto (Full)"));
    ImGui::PopStyleColor(2);
}

// after EffectCraft crates/ui-egui/src/panels/timeline.rs
static void drawTimeline(Project::Project* proj) {
    if (!proj || proj->activeComp < 0) {
        ImGui::TextDisabled("%s", L("No composition active."));
        return;
    }
    const auto& comp = proj->compositions[(size_t)proj->activeComp];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW = ImGui::GetContentRegionAvail().x;
    const float avH = ImGui::GetContentRegionAvail().y;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    // --- Timeline header row ---
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs current time + toolbar
    const float hdrH = 28.0f;
    dl->AddRectFilled(p0, {p0.x+avW, p0.y+hdrH}, C_APP_BG);

    // Current-time display (blue timecode)
    const Project::FrameRate fr = comp.frameRate;
    std::string tc = Project::format_timecode_ae(0, fr, false);
    ImGui::SetCursorScreenPos({p0.x+6, p0.y+5});
    ImGui::TextColored(col4(C_BLUE), "%s", tc.c_str());
    ImGui::SameLine();
    // fps label (e.g. "00090 (29.97 fps)")
    char fpsBuf[64]; snprintf(fpsBuf, sizeof fpsBuf, " (%s fps)", fr.label().c_str());
    ImGui::TextDisabled("%s", fpsBuf);

    // Right-side toolbar icons: shy, frame-blend, motion-blur, graph-editor
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs toolbar switches
    ImGui::SameLine(avW - 120);
    if (ImGui::SmallButton("S")) {}  // shy layers toggle
    ImGui::SameLine();
    if (ImGui::SmallButton("FB")) {} // frame blend
    ImGui::SameLine();
    if (ImGui::SmallButton("MB")) {} // motion blur
    ImGui::SameLine();
    if (ImGui::SmallButton("GE")) {} // graph editor

    hLine(dl, p0.x, p0.x+avW, p0.y+hdrH);

    // --- Layer column header ---
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs column header
    const float colHdrH = 18.0f;
    const ImVec2 chp = {p0.x, p0.y+hdrH};
    dl->AddRectFilled(chp, {chp.x+avW, chp.y+colHdrH}, C_APP_BG);
    // Column icons: eye/audio/solo/lock | label | # | type | Layer Name | switches | Parent
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs COLUMNS
    const char* colHdrs[] = { "#", L("Layer Name"), L("Parent & Link") };
    const float colXPos[] = { 60, 110, avW*0.6f };
    for (int i = 0; i < 3; ++i)
        dl->AddText({chp.x+colXPos[i], chp.y+2}, C_DIM, colHdrs[i]);
    hLine(dl, chp.x, chp.x+avW, chp.y+colHdrH);

    // --- Layer rows ---
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs layer rows
    const float layerAreaH = avH - hdrH - colHdrH - 18 - 2;
    const float rulerH = 18.0f;
    ImGui::SetCursorScreenPos({p0.x, p0.y+hdrH+colHdrH});
    ImGui::BeginChild("##tlayers", {avW*0.35f, layerAreaH}, false,
                      ImGuiWindowFlags_NoScrollbar);
    for (int i = 0; i < (int)comp.layers.size(); ++i) {
        const auto& layer = comp.layers[(size_t)i];
        ImGui::PushID(i);
        const ImVec2 rp = ImGui::GetCursorScreenPos();
        // Label colour swatch (small coloured square, left of #)
        const ImU32 swCol = labelColor((i+1) % 17);
        ImGui::GetWindowDrawList()->AddRectFilled(
            {rp.x+2, rp.y+2},{rp.x+12, rp.y+ROW_H-2}, swCol);
        ImGui::Dummy({14, ROW_H}); ImGui::SameLine(0,2);
        // Layer # (1-based, as in AE)
        char num[8]; snprintf(num, sizeof num, "%d", i+1);
        ImGui::TextDisabled("%s", num); ImGui::SameLine(0,2);
        // Eye / audio / solo / lock icon buttons (small clickable text stand-ins)
        ImGui::Text("%s", layer.switches.video ? "O" : " "); ImGui::SameLine(0,2);
        // Layer type icon (first char of kind name)
        const char* kindChar = "S";
        switch (layer.kind) {
            case Project::LayerKind::Text:        kindChar = "T"; break;
            case Project::LayerKind::Shape:       kindChar = "*"; break;
            case Project::LayerKind::Null:        kindChar = "N"; break;
            case Project::LayerKind::Camera:      kindChar = "C"; break;
            case Project::LayerKind::Light:       kindChar = "L"; break;
            case Project::LayerKind::Footage:     kindChar = "F"; break;
            case Project::LayerKind::Composition: kindChar = "P"; break;
            default: break;
        }
        ImGui::TextDisabled("%s", kindChar); ImGui::SameLine(0,2);
        // Layer name
        ImGui::Text("%s", layer.name.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();

    // --- Layer bars (the timeline track area) ---
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs layer bars / work area
    const ImVec2 barAreaP = {p0.x + avW*0.35f, p0.y+hdrH+colHdrH};
    const float barAreaW = avW*0.65f;
    ImGui::SetCursorScreenPos(barAreaP);
    ImGui::BeginChild("##tlbars", {barAreaW, layerAreaH}, false,
                      ImGuiWindowFlags_NoScrollbar);
    // Time ruler
    const ImVec2 rp2 = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        rp2, {rp2.x+barAreaW, rp2.y+rulerH},
        C_APP_BG);
    // Ruler tick marks (every second)
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs time ruler
    const double durSec = comp.durationSeconds();
    const int nSecs = (int)durSec + 1;
    for (int s = 0; s < nSecs && s < 20; ++s) {
        const float rx = (durSec > 0) ? rp2.x + (float)((double)s / durSec) * barAreaW : rp2.x;
        char tbuf[16]; snprintf(tbuf, sizeof tbuf, "%d", s);
        ImGui::GetWindowDrawList()->AddLine({rx, rp2.y+2},{rx, rp2.y+rulerH-2}, C_LINE);
        ImGui::GetWindowDrawList()->AddText({rx+2, rp2.y+2}, C_DIM, tbuf);
    }
    ImGui::Dummy({barAreaW, rulerH});
    // Layer bars in label colours
    for (int i = 0; i < (int)comp.layers.size(); ++i) {
        const auto& layer = comp.layers[(size_t)i];
        const ImU32 barCol = labelColor((i+1) % 17);
        const ImVec2 brp = ImGui::GetCursorScreenPos();
        const float inX  = (durSec > 0) ? brp.x + (float)(layer.inPt.seconds()  / durSec) * barAreaW : brp.x;
        const float outX = (durSec > 0) ? brp.x + (float)(layer.outPt.seconds() / durSec) * barAreaW : brp.x;
        ImGui::GetWindowDrawList()->AddRectFilled(
            {inX, brp.y+1},{outX, brp.y+ROW_H-1}, barCol);
        ImGui::Dummy({barAreaW, ROW_H});
    }
    // Playhead (blue vertical line at t=0 for now)
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs playhead
    {
        const ImVec2 phBase = {barAreaP.x, p0.y+hdrH+colHdrH};
        ImGui::GetWindowDrawList()->AddLine(
            {phBase.x, phBase.y},
            {phBase.x, phBase.y+layerAreaH},
            C_BLUE, 1.5f);
    }
    ImGui::EndChild();

    // --- Toggle Switches / Modes bar at the bottom ---
    // after EffectCraft crates/ui-egui/src/panels/timeline.rs toggle switches/modes
    ImGui::SetCursorScreenPos({p0.x, p0.y+hdrH+colHdrH+layerAreaH});
    ImGui::TextDisabled(L("Toggle Switches / Modes"));
}

// after EffectCraft crates/ui-egui/src/panels/live_preview.rs
static void drawPreviewPanel() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    // Transport controls row
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs transport buttons:
    // |<<  <  play  >  >>|
    const float btnW = 20.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, col4(C_APP_BG));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col4(C_DEEP));
    if (ImGui::Button("|<", {btnW,20})) {}  ImGui::SameLine(0,2);
    if (ImGui::Button("<",  {btnW,20})) {}  ImGui::SameLine(0,2);
    if (ImGui::Button(L("Play"), {40,20})) {} ImGui::SameLine(0,2);
    if (ImGui::Button(">",  {btnW,20})) {}  ImGui::SameLine(0,2);
    if (ImGui::Button(">|", {btnW,20})) {}
    ImGui::PopStyleColor(2);

    hLine(dl, p0.x, p0.x+avW, ImGui::GetCursorScreenPos().y);

    // Shortcut / loop / range / play-from / frame rate / resolution
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs settings rows
    ImGui::TextDisabled("%s", L("Shortcut")); ImGui::SameLine(80); ImGui::TextDisabled("Spacebar");
    static bool s_loop = false;
    ImGui::Checkbox(L("Loop"), &s_loop);

    ImGui::TextDisabled("%s", L("Range"));    ImGui::SameLine(80);
    ImGui::TextDisabled(L("Work Area Extended By Current Frame"));
    ImGui::TextDisabled("%s", L("Play From")); ImGui::SameLine(80);
    ImGui::TextDisabled(L("Current Time"));
    ImGui::TextDisabled("%s", L("Frame Rate")); ImGui::SameLine(80);
    ImGui::TextDisabled("(29.97)");
    ImGui::TextDisabled("%s", L("Resolution")); ImGui::SameLine(80);
    ImGui::TextDisabled(L("Auto"));

    hLine(dl, p0.x, p0.x+avW, ImGui::GetCursorScreenPos().y);

    // Cached-frames progress bar
    // after EffectCraft crates/ui-egui/src/panels/live_preview.rs cached frames bar
    const ImVec2 cbP = ImGui::GetCursorScreenPos();
    ImGui::Dummy({avW, 6});
    dl->AddRectFilled({cbP.x, cbP.y}, {cbP.x+avW*0.12f, cbP.y+5}, C_BLUE); // 12% cached
    hLine(dl, cbP.x, cbP.x+avW, cbP.y+6);
    ImGui::TextDisabled("36 / 300 %s", L("frames cached"));
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

    // "Properties: COMPNAME" header
    // after EffectCraft crates/ui-egui/src/panels/properties.rs header
    const auto& comp = proj->compositions[(size_t)proj->activeComp];
    char hdr[256]; snprintf(hdr, sizeof hdr, "%s: %s", L("Properties"), comp.name.c_str());
    dl->AddText({p0.x+6, p0.y+4}, C_TEXT, hdr);
    ImGui::Dummy({avW, 20});
    hLine(dl, p0.x, p0.x+avW, ImGui::GetCursorScreenPos().y);

    // Layer Transform section
    // after EffectCraft crates/ui-egui/src/panels/properties.rs layer_transform section
    if (ImGui::CollapsingHeader(L("Layer Transform"), ImGuiTreeNodeFlags_DefaultOpen)) {
        // Reset button on the right
        ImGui::SameLine(avW - 42);
        ImGui::SmallButton(L("Reset"));
        // Transform properties: Anchor Point, Position, Scale, Rotation, Opacity
        // Each has a stopwatch icon (phase 5 keyframes), label and value
        // after EffectCraft crates/ui-egui/src/panels/properties.rs Transform props
        static const struct { const char* name; float v[2]; bool point; } xformProps[] = {
            {L("Anchor Point"), {0, 0},     true},
            {L("Position"),     {960, 540}, true},
            {L("Scale"),        {100, 100}, true},
            {L("Rotation"),     {0, 0},     false},
            {L("Opacity"),      {100, 0},   false},
        };
        for (const auto& prop : xformProps) {
            // Stopwatch icon (circle placeholder)
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", prop.name);
            ImGui::SameLine(130);
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

// ---------- header / tool bar ----------
// after EffectCraft crates/ui-egui/src/header.rs show()
// Tools bar: Atelier brand | Home | tool slots | snapping | workspaces | Discord pill
static void drawHeader(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({x,y},{x+w,y+h}, C_HEADER);
    hLine(dl, x, x+w, y+h);

    float cx = x + 10;

    // --- Tool slots ---
    // after EffectCraft crates/ui-egui/src/header.rs tool_slots and Tool::SLOTS:
    // Selection | Hand | Zoom | Rotate | Pan Behind | Shape tools | Pen | Type | ...
    // For phase 1 we show the 15 canonical slots with their abbreviated labels
    // (later phases will draw proper icons via stb_truetype glyphs or inline bitmaps).
    struct ToolDef { const char* label; const char* tip; };
    static const ToolDef TOOLS[] = {
        {"V",  "Selection Tool"},
        {"W",  "Hand Tool"},
        {"Z",  "Zoom Tool"},
        {"C",  "Orbit Camera / Pan Camera / Dolly Camera"},
        {"Y",  "Rotation Tool"},
        {"A",  "Pan Behind Tool"},
        {"Q",  "Shape: Rectangle / Round Rect / Ellipse / Polygon / Star"},
        {"G",  "Pen / Add Vertex / Delete Vertex / Convert Vertex"},
        {"T",  "Type / Vertical Type"},
        {"Bs", "Brush / Clone Stamp / Eraser"},
        {"Rb", "Roto Brush / Refine Edge"},
        {"Pp", "Puppet Pin / Bend / Advanced / Starch / Overlap"},
    };
    static int s_activeTool = 0;
    // Group separators: after camera (2), pan-behind (5), type (8), roto (10)
    // after EffectCraft crates/ui-egui/src/header.rs tool_slots separators
    static const int SEP_AFTER[] = {2, 5, 7, 10};
    static bool s_sepAfter[12] = {};  // tracks which tool indices have a sep after them
    if (!s_sepAfter[2]) {  // only compute once
        for (int sa : SEP_AFTER) if (sa < 12) s_sepAfter[sa] = true;
    }

    for (int i = 0; i < 12; ++i) {
        const bool active = (i == s_activeTool);
        // Separator before groups 2+
        if (i > 0 && s_sepAfter[i-1]) {
            vLine(dl, cx+2, y+5, y+h-5);
            cx += 6;
        }
        const ImVec2 btnMin{cx, y+4};
        const ImVec2 btnMax{cx+24, y+h-4};
        const bool hover = ImGui::IsMouseHoveringRect(btnMin, btnMax);
        dl->AddRectFilled(btnMin, btnMax,
            active ? C_DEEP : (hover ? IM_COL32(0x30,0x30,0x30,255) : C_HEADER));
        if (active)
            dl->AddRect(btnMin, btnMax, C_BLUE);
        const ImVec2 ts = ImGui::CalcTextSize(TOOLS[i].label);
        dl->AddText({cx+(24-ts.x)*0.5f, y+(h-ts.y)*0.5f},
                    active ? C_TEXT : C_DIM,
                    TOOLS[i].label);
        if (hover && ImGui::IsMouseClicked(0)) s_activeTool = i;
        if (hover)
            ImGui::SetTooltip("%s", TOOLS[i].tip);
        cx += 26;
    }
    cx += 8;

    // --- Snapping checkbox ---
    // after EffectCraft crates/ui-egui/src/header.rs snapping_toggle
    static bool s_snap = true;
    {
        const ImVec2 cbMin{cx, y+(h-16)*0.5f};
        const ImVec2 cbMax{cx+16, y+(h+16)*0.5f};
        const bool hover = ImGui::IsMouseHoveringRect(cbMin, cbMax);
        dl->AddRectFilled(cbMin, cbMax, s_snap ? C_BLUE : C_WELL);
        dl->AddRect(cbMin, cbMax, C_LINE);
        if (s_snap)
            dl->AddText({cx+2, y+(h-14)*0.5f}, C_TEXT, "v");
        if (hover && ImGui::IsMouseClicked(0)) s_snap = !s_snap;
    }
    cx += 20;
    const float snx = cx;
    {
        ImVec2 ts = ImGui::CalcTextSize(L("Snapping"));
        dl->AddText({cx, y+(h-ts.y)*0.5f}, s_snap ? C_DIM : C_FAINT, L("Snapping"));
        cx += ts.x + 6;
    }
    cx += 10;

    // --- Workspace tabs (right side) ---
    // after EffectCraft crates/ui-egui/src/header.rs workspace_tabs
    // Order right-to-left in the original: "Default", "Animation", "Standard", "Small Screen"
    // We draw left-to-right for simplicity.
    static const char* WS_TABS[] = { "Default", "Animation", "Standard", "Small Screen" };
    static int s_wsTab = 0;
    // Position them right-aligned
    float wsX = x + w - 10;
    for (int i = 3; i >= 0; --i) {
        ImVec2 ts = ImGui::CalcTextSize(WS_TABS[i]);
        const float tw = ts.x + 16;
        wsX -= tw;
        const ImVec2 trMin{wsX, y+2};
        const ImVec2 trMax{wsX+tw, y+h-2};
        const bool on = (i == s_wsTab);
        const bool hover = ImGui::IsMouseHoveringRect(trMin, trMax);
        dl->AddText({wsX+8, y+(h-ts.y)*0.5f},
                    on ? C_TEXT : (hover ? C_TEXT : C_DIM),
                    WS_TABS[i]);
        if (on)
            dl->AddLine({wsX+8, y+h-4},{wsX+tw-8, y+h-4}, C_BLUE, 2.0f);
        if (hover && ImGui::IsMouseClicked(0)) s_wsTab = i;
        wsX -= 2;
    }
    // "»" overflow menu button
    wsX -= 24;
    dl->AddText({wsX+6, y+(h-14)*0.5f}, C_DIM, "\xc2\xbb");  // UTF-8 »

    (void)snx;  // suppress unused warning
}

// ---------- main draw ----------
void draw(Project::Project* proj, const Actions& actions) {
    g_actions = &actions;

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
            ImGui::MenuItem(L("New Shape Layer"));
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
    ImDrawList*  dl    = ImGui::GetForegroundDrawList();

    // ----- Header / Tools bar (full width, HEADER_H px) -----
    // after EffectCraft crates/ui-egui/src/header.rs show()
    {
        ImGui::SetNextWindowPos({cPos.x, cPos.y});
        // The header is drawn directly onto the window's draw list (no child window needed)
        // so we just call drawHeader which uses GetWindowDrawList().
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
    const float centreW = cW - LEFT_W - RIGHT_W - 2;  // 2 for two 1-px dividers

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
        // after EffectCraft crates/ui-egui/src/dock.rs tabs() / TAB_STRIP
        const char* leftTabs[] = { L("Project"), L("Effect Controls") };
        s_leftTab = tabStrip(leftTabs, 2, s_leftTab,
                             lp.x, lp.y, LEFT_W, TAB_H, ldl, ImGui::GetIO());
        // hamburger menu button
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
        // Comp breadcrumb path (after EffectCraft viewer.rs tab breadcrumb)
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
        // after EffectCraft crates/ui-egui/src/dock.rs Default workspace: tabs([Timeline,RenderQueue])
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
        // after EffectCraft crates/ui-egui/src/dock.rs PREVIEW_FRAME tabs([Preview])
        const char* rightTopTabs[] = { L("Preview"), L("Info"), L("Audio") };
        static int s_previewTab = 0;
        s_previewTab = tabStrip(rightTopTabs, 3, s_previewTab,
                                rp.x, rp.y, RIGHT_W, TAB_H, rdl, ImGui::GetIO());
        panelMenuButton(rp.x+RIGHT_W-2, rp.y+TAB_H*0.5f, rdl);

        ImGui::SetCursorScreenPos({rp.x, rp.y+TAB_H});
        ImGui::BeginChild("##righttop", {RIGHT_W, rightTopH-TAB_H}, false);
        if (s_previewTab == 0)      drawPreviewPanel();
        else if (s_previewTab == 1) drawInfoPanel(proj);
        else                        ImGui::TextDisabled(L("Audio meters in phase 9."));
        ImGui::EndChild();

        hLine(rdl, rp.x, rp.x+RIGHT_W, rp.y+rightTopH);

        // --- Right bottom: Properties / Align / Audio / Effects & Presets ---
        // after EffectCraft crates/ui-egui/src/dock.rs Default workspace:
        //   tabs([Properties,Align,Audio,EffectsPresets])
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
