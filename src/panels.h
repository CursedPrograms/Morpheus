// Morpheus - panel layout: the seven EffectCraft-style panels (Project, Composition, Timeline,
// Effect Controls, Tools, Preview, Info) docked in a fixed multi-column layout.
// Each panel is an ImGui child window inside the main window's client area.
#pragma once
#include <string>
#include <functional>

// Forward declaration for the project model (phase 2).
namespace Project { struct Project; }

namespace Panels {

// App-level actions the panels can request (phase 2+).
struct Actions {
    std::function<void()>              onNew;       // File > New Composition
    std::function<void()>              onOpen;      // File > Open
    std::function<void()>              onSave;      // File > Save
    std::function<void()>              onSaveAs;    // File > Save As
    std::function<void()>              onQuit;      // File > Quit
    std::function<void(int)>           onSelectComp;// click a comp in the Project panel
};

// Initialise panel state (call once after ImGui is set up).
void init();

// Draw all panels for the current frame.  proj may be nullptr while no project is open.
void draw(Project::Project* proj, const Actions& actions);

}  // namespace Panels
