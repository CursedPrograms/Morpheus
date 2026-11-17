// Morpheus - panel layout: seven docked panels in the Atelier look.
// Layout: narrow Tools strip on the left, then a three-column split:
//   left column:   Project panel (top) + Info panel (bottom)
//   centre column: Composition viewer (top) + Timeline (bottom)
//   right column:  Effect Controls (top) + Preview (bottom)
// Atelier greys + blue #007bb7, solid, no gradients.
#include "panels.h"
#include "project.h"
#include "lang.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstring>

namespace Panels {

// ---------- colour palette (same as Chimera / other Atelier apps) ----------
static const ImU32
    C_BG    = IM_COL32(0x1a, 0x1a, 0x1a, 255),
    C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
    C_LINE  = IM_COL32(0x3a, 0x3a, 0x3a, 255),
    C_TEXT  = IM_COL32(0xe6, 0xe6, 0xe6, 255),
    C_DIM   = IM_COL32(0x9a, 0x9a, 0x9a, 255),
    C_BLUE  = IM_COL32(0x00, 0x7b, 0xb7, 255),
    C_DEEP  = IM_COL32(0x00, 0x51, 0x84, 255),
    C_WELL  = IM_COL32(0x14, 0x14, 0x14, 255);

static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// Apply the Atelier ImGui style (call once).
static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 0;  s.FrameRounding  = 2;  s.GrabRounding   = 2;
    s.PopupRounding    = 2;  s.ScrollbarRounding = 2;
    s.WindowBorderSize = 0;  s.FrameBorderSize  = 0; s.PopupBorderSize = 1;
    s.WindowPadding    = {8, 8};  s.FramePadding  = {7, 4};  s.ItemSpacing = {6, 5};
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]            = col4(C_TEXT);
    c[ImGuiCol_TextDisabled]    = col4(C_DIM);
    c[ImGuiCol_WindowBg]        = col4(C_PANEL);
    c[ImGuiCol_ChildBg]         = col4(C_PANEL);
    c[ImGuiCol_PopupBg]         = col4(IM_COL32(0x21, 0x21, 0x21, 255));
    c[ImGuiCol_Border]          = col4(C_LINE);
    c[ImGuiCol_Separator]       = col4(C_LINE);
    c[ImGuiCol_FrameBg]         = col4(C_WELL);
    c[ImGuiCol_FrameBgHovered]  = col4(IM_COL32(0x2e, 0x2e, 0x2e, 255));
    c[ImGuiCol_FrameBgActive]   = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_TitleBg]         = col4(C_BG);
    c[ImGuiCol_TitleBgActive]   = col4(C_BG);
    c[ImGuiCol_MenuBarBg]       = col4(C_BG);
    c[ImGuiCol_ScrollbarBg]     = col4(C_PANEL);
    c[ImGuiCol_ScrollbarGrab]   = col4(C_LINE);
    c[ImGuiCol_CheckMark]       = col4(C_BLUE);
    c[ImGuiCol_SliderGrab]      = col4(C_BLUE);
    c[ImGuiCol_SliderGrabActive]= col4(C_DEEP);
    c[ImGuiCol_Button]          = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_ButtonHovered]   = col4(C_DEEP);
    c[ImGuiCol_ButtonActive]    = col4(C_BLUE);
    c[ImGuiCol_Header]          = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_HeaderHovered]   = col4(C_DEEP);
    c[ImGuiCol_HeaderActive]    = col4(C_BLUE);
    c[ImGuiCol_Tab]             = col4(C_BG);
    c[ImGuiCol_TabActive]       = col4(C_PANEL);
    c[ImGuiCol_TabHovered]      = col4(C_DEEP);
    c[ImGuiCol_ModalWindowDimBg]= {0, 0, 0, 0.45f};
}

void init() {
    applyStyle();
}

// ---------- panel header helper ----------
// Draws a slim coloured title bar above a panel's content area.
static void panelHeader(const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  w  = ImGui::GetContentRegionAvail().x;
    const float  h  = 20.0f;
    dl->AddRectFilled(p, {p.x + w, p.y + h}, C_LINE);
    dl->AddText({p.x + 6, p.y + 3}, C_BLUE, label);
    ImGui::Dummy({w, h});
}

// ---------- panel content helpers ----------
// Each function draws the contents of one panel.

static void drawProjectPanel(Project::Project* proj) {
    panelHeader(L("Project"));
    if (!proj) {
        ImGui::TextDisabled("%s", L("No project open."));
        return;
    }
    ImGui::TextDisabled("%s", proj->name.c_str());
    ImGui::Separator();
    for (int i = 0; i < (int)proj->compositions.size(); ++i) {
        const auto& comp = proj->compositions[i];
        const bool active = (i == proj->activeComp);
        if (active) ImGui::PushStyleColor(ImGuiCol_Text, col4(C_BLUE));
        ImGui::Text("  %s", comp.name.c_str());
        if (active) ImGui::PopStyleColor();
    }
}

static void drawCompositionPanel(Project::Project* proj) {
    panelHeader(L("Composition"));
    // Phase 1: empty canvas placeholder until rendering is implemented (phase 3).
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    // Checkerboard background.
    const float cs = 16;
    for (float y = 0; y < avail.y; y += cs)
        for (float x = 0; x < avail.x; x += cs) {
            const bool odd = ((int)(x / cs) + (int)(y / cs)) & 1;
            dl->AddRectFilled({p.x + x, p.y + y},
                              {p.x + x + cs, p.y + y + cs},
                              odd ? IM_COL32(0x28, 0x28, 0x28, 255)
                                  : IM_COL32(0x1e, 0x1e, 0x1e, 255));
        }
    // "No composition" label in the centre.
    const char* label = proj && proj->activeComp >= 0
        ? proj->compositions[proj->activeComp].name.c_str()
        : L("No composition");
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({p.x + (avail.x - ts.x) * 0.5f, p.y + (avail.y - ts.y) * 0.5f},
                C_DIM, label);
    ImGui::Dummy(avail);
}

static void drawTimelinePanel(Project::Project* proj) {
    panelHeader(L("Timeline"));
    if (!proj || proj->activeComp < 0) {
        ImGui::TextDisabled("%s", L("No composition active."));
        return;
    }
    const auto& comp = proj->compositions[proj->activeComp];
    ImGui::TextDisabled("%s  %.2f fps  %.2fs", comp.name.c_str(), comp.fps, comp.duration);
    ImGui::Separator();
    for (const auto& layer : comp.layers)
        ImGui::Text("  %s", layer.name.c_str());
}

static void drawEffectControlsPanel(Project::Project* /*proj*/) {
    panelHeader(L("Effect Controls"));
    ImGui::TextDisabled("%s", L("No layer selected."));
}

static void drawToolsPanel() {
    // Slim vertical strip of tool buttons (icons are single letters for now).
    panelHeader(L("Tools"));
    static const char* tools[] = {"V", "W", "R", "C", "P", "T", "Q"};
    static const char* tips[]  = {"Selection", "Track", "Rotate", "Camera", "Pen", "Type", "Zoom"};
    static int sel = 0;
    for (int i = 0; i < 7; ++i) {
        ImGui::PushID(i);
        if (sel == i) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
        if (ImGui::Button(tools[i], {28, 28})) sel = i;
        if (sel == i) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tips[i]);
        ImGui::PopID();
    }
}

static void drawPreviewPanel() {
    panelHeader(L("Preview"));
    if (ImGui::Button(L("Play")))  { /* phase 4 */ }
    ImGui::SameLine();
    if (ImGui::Button(L("Stop"))) { /* phase 4 */ }
    ImGui::SameLine();
    ImGui::TextDisabled("0:00:00:00");
}

static void drawInfoPanel(Project::Project* proj) {
    panelHeader(L("Info"));
    if (proj)
        ImGui::TextDisabled("%s", proj->name.c_str());
    else
        ImGui::TextDisabled("%s", L("Morpheus"));
    ImGui::TextDisabled("Motion Graphics & Compositing");
}

// ---------- main draw ----------
void draw(Project::Project* proj) {
    // Fill the entire OS window; no title bar, no decoration.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));

    ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_MenuBar;
    ImGui::Begin("##root", nullptr, rootFlags);
    ImGui::PopStyleColor();

    // ----- Menu bar -----
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(L("File"))) {
            ImGui::MenuItem(L("New Composition..."),  "Ctrl+N");
            ImGui::MenuItem(L("Open..."),             "Ctrl+O");
            ImGui::MenuItem(L("Save"),                "Ctrl+S");
            ImGui::MenuItem(L("Save As..."),          "Ctrl+Shift+S");
            ImGui::Separator();
            ImGui::MenuItem(L("Quit"),                "Ctrl+Q");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Edit"))) {
            ImGui::MenuItem(L("Undo"),  "Ctrl+Z");
            ImGui::MenuItem(L("Redo"),  "Ctrl+Shift+Z");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Composition"))) {
            ImGui::MenuItem(L("New Composition..."));
            ImGui::MenuItem(L("Composition Settings..."));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Layer"))) {
            ImGui::MenuItem(L("New Solid..."));
            ImGui::MenuItem(L("New Shape Layer"));
            ImGui::MenuItem(L("New Text Layer"));
            ImGui::MenuItem(L("New Null Object"));
            ImGui::MenuItem(L("New Camera..."));
            ImGui::MenuItem(L("New Light..."));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Effect"))) {
            ImGui::MenuItem(L("(Effects in phase 7)"));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("View"))) {
            ImGui::MenuItem(L("Zoom In"),  "=");
            ImGui::MenuItem(L("Zoom Out"), "-");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("Help"))) {
            ImGui::MenuItem(L("About Morpheus..."));
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    // Layout constants (pixels).
    const float toolsW  = 44;
    const float leftW   = 220;
    const float rightW  = 240;
    const float midW    = totalW - toolsW - leftW - rightW - 6; // 6 for 3 separators
    const float topH    = totalH * 0.60f;
    const float botH    = totalH - topH - 2;

    // ----- Tools strip (vertical, left edge) -----
    ImGui::BeginChild("##tools", {toolsW, totalH}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawToolsPanel();
    ImGui::EndChild();

    ImGui::SameLine(0, 2);
    // Vertical rule drawn with the draw list; SeparatorEx is internal and version-specific.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine({p.x, p.y}, {p.x, p.y + totalH}, C_LINE, 1.0f);
        ImGui::Dummy({1, totalH});
    }
    ImGui::SameLine(0, 2);

    // ----- Left column: Project (top) + Info (bottom) -----
    ImGui::BeginChild("##left_col", {leftW, totalH}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImGui::BeginChild("##project", {leftW, topH}, false);
        drawProjectPanel(proj);
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::BeginChild("##info", {leftW, botH}, false);
        drawInfoPanel(proj);
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 2);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine({p.x, p.y}, {p.x, p.y + totalH}, C_LINE, 1.0f);
        ImGui::Dummy({1, totalH});
    }
    ImGui::SameLine(0, 2);

    // ----- Centre column: Composition viewer (top) + Timeline (bottom) -----
    ImGui::BeginChild("##mid_col", {midW, totalH}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImGui::BeginChild("##composition", {midW, topH}, false);
        drawCompositionPanel(proj);
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::BeginChild("##timeline", {midW, botH}, false);
        drawTimelinePanel(proj);
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 2);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine({p.x, p.y}, {p.x, p.y + totalH}, C_LINE, 1.0f);
        ImGui::Dummy({1, totalH});
    }
    ImGui::SameLine(0, 2);

    // ----- Right column: Effect Controls (top) + Preview (bottom) -----
    ImGui::BeginChild("##right_col", {rightW, totalH}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImGui::BeginChild("##fxctrl", {rightW, topH}, false);
        drawEffectControlsPanel(proj);
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::BeginChild("##preview", {rightW, botH}, false);
        drawPreviewPanel();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace Panels
