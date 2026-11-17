// Morpheus - panel layout: the seven EffectCraft-style panels (Project, Composition, Timeline,
// Effect Controls, Tools, Preview, Info) docked in a fixed multi-column layout.
// Each panel is an ImGui child window inside the main window's client area.
#pragma once
#include <string>

// Forward declaration for the project model (phase 2).
namespace Project { struct Project; }

namespace Panels {

// Initialise panel state (call once after ImGui is set up).
void init();

// Draw all panels for the current frame.  proj may be nullptr while no project is open.
void draw(Project::Project* proj);

}  // namespace Panels
