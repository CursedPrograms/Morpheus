// Morpheus - panel layout: EffectCraft's docked panel layout ported from
// crates/ui-egui/src/dock.rs and crates/ui-egui/src/panels/.
// Default workspace: Tools header across the top, then:
//   left col (300 px):  Project / Effect Controls tabs
//   centre:             Composition / Layer tabs over Timeline / Render Queue tabs
//   right col (300 px): Preview (short) over Properties / Align / Audio tabs
// Atelier greys + #007bb7 blue, solid, no gradients.
// after EffectCraft (crates/ui-egui/src/dock.rs workspace_root "Default")
#pragma once
#include <string>
#include <functional>

// Forward declaration for the project model.
namespace Project { struct ProjectData; }

namespace Panels {

// App-level actions the panels can request.
struct Actions {
    std::function<void()>              onNew;
    std::function<void()>              onOpen;
    std::function<void()>              onSave;
    std::function<void()>              onSaveAs;
    std::function<void()>              onQuit;
    std::function<void(int)>           onSelectComp;
};

// Initialise panel state (call once after ImGui is set up).
void init();

// Draw all panels for the current frame.  proj may be nullptr while no project is open.
void draw(Project::ProjectData* proj, const Actions& actions);

}  // namespace Panels
