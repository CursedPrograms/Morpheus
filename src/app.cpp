// Morpheus - the app: project management, menus, panel orchestration, scripted test hooks.
// Atelier look: greys + #007bb7 blue, solid, no gradients.
#include "app.h"
#include "panels.h"
#include "project.h"
#include "lang.h"
#include "splash.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

#include <cstring>
#include <filesystem>
#include <string>

namespace App {

static SDL_Window* s_win = nullptr;
static std::string s_prefs;
static bool        s_quit = false;

// Current project; nullptr when no project is open.
static Project::Project* s_proj = nullptr;

// Open a file dialog and return the chosen path (empty on cancel).
static std::string fileOpen(const char* filter) {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof ofn;
    ofn.hwndOwner    = nullptr;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameA(&ofn) ? std::string(buf) : std::string();
#else
    return {};
#endif
}

static std::string fileSave(const char* filter, const char* defExt) {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize   = sizeof ofn;
    ofn.hwndOwner     = nullptr;
    ofn.lpstrFilter   = filter;
    ofn.lpstrFile     = buf;
    ofn.nMaxFile      = MAX_PATH;
    ofn.lpstrDefExt   = defExt;
    ofn.Flags         = OFN_OVERWRITEPROMPT;
    return GetSaveFileNameA(&ofn) ? std::string(buf) : std::string();
#else
    return {};
#endif
}

// Update the OS window title to reflect the current project.
static void setTitle() {
    std::string t = "Morpheus";
    if (s_proj) {
        t += " - ";
        t += s_proj->name.empty() ? "Untitled" : s_proj->name;
        if (s_proj->dirty) t += " *";
    }
    SDL_SetWindowTitle(s_win, t.c_str());
}

// Create a new project and make it current.
static void newProject() {
    delete s_proj;
    s_proj = new Project::Project(Project::Project::makeNew());
    setTitle();
}

static void openProject(const std::string& path) {
    auto* p = new Project::Project();
    if (Project::load(*p, path)) {
        delete s_proj;
        s_proj = p;
        setTitle();
    } else {
        delete p;
    }
}

static void saveProject(const std::string& path) {
    if (!s_proj) return;
    if (Project::save(*s_proj, path)) {
        s_proj->path  = path;
        s_proj->dirty = false;
        // Use the filename (without extension) as the project name.
        const auto stem = std::filesystem::u8path(path).stem().u8string();
        if (!stem.empty()) s_proj->name = stem;
        setTitle();
    }
}

void init(SDL_Window* win, const std::string& prefDir,
          void (*status)(const char*, float)) {
    s_win   = win;
    s_prefs = prefDir;

    if (status) status("Initialising panels...", 0.3f);
    Panels::init();

    if (status) status("Loading language...", 0.6f);
    // Load a saved language preference if present.
    const std::string langFile = prefDir + "lang.txt";
    if (FILE* f = fopen(langFile.c_str(), "rb")) {
        char code[8] = {};
        if (fgets(code, sizeof code, f)) {
            // Trim newline.
            for (char* p = code; *p; ++p)
                if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
            Lang::load(code);
        }
        fclose(f);
    }

    if (status) status("Ready", 1.0f);
    setTitle();
}

void frame() {
    // Allow Ctrl+Q / Alt+F4 to quit.
    if (ImGui::IsKeyPressed(ImGuiKey_Q) &&
        ImGui::GetIO().KeyCtrl)
        s_quit = true;

    // Draw all panels; pass the current project (may be nullptr).
    Panels::draw(s_proj);
}

bool wantsQuit() { return s_quit; }
void requestQuit() { s_quit = true; }

void script(const std::string& name) {
    // Built-in test scripts for --capture.
    if (name == "skeleton") {
        // Phase 1 test: just open the window; a screenshot is enough.
        SDL_Log("script skeleton: window open, capturing screenshot.");
    } else if (name == "new_project") {
        // Phase 2 test: create a new project, verify it shows up.
        newProject();
        SDL_Log("script new_project: created project '%s'.", s_proj ? s_proj->name.c_str() : "(null)");
    } else {
        SDL_Log("script '%s': unknown (skipping).", name.c_str());
    }
}

bool busy() { return false; }

}  // namespace App
