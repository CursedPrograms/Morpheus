// Morpheus - the app: project, panels, menus, scripted test hooks.
#pragma once
#include <string>

struct SDL_Window;

namespace App {
// Initialise; status callback is for the splash (nullptr = no splash).
void init(SDL_Window* win, const std::string& prefDir,
          void (*status)(const char* what, float progress) = nullptr);

// Draw one frame (call every loop iteration).
void frame();

bool wantsQuit();
void requestQuit();

// Drive the named test script (called on frame 2 in --capture mode).
void script(const std::string& name);

// True while an async job is still running (--capture waits for false).
bool busy();
}
