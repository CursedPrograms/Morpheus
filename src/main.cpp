// Morpheus - entry point: SDL2 window, OpenGL 3 context, Dear ImGui, splash, main loop.
// After EffectCraft architecture; Chimera's main.cpp pattern adapted for Morpheus.
//
//   Morpheus                                   opens the app normally
//   Morpheus --script NAME --capture out.bmp   scripted test + screenshot, then quit
//   Morpheus --render project.morpheus out.png --frame N   offline frame render (phase 3+)
//   Morpheus --selftest                        run all self-tests and exit (0=all pass)
#include "app.h"
#include "splash.h"
#include "selftest.h"
#include "render.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// True if the file exists and is readable.
static bool fileExists(const char* p) {
    if (FILE* f = fopen(p, "rb")) { fclose(f); return true; }
    return false;
}

// Load a system font and return the ImFont*, or nullptr on failure.
static ImFont* addFont(const char* file, float size) {
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2190, 0x21FF, 0x2600, 0x26FF, 0};
    ImFont* font = io.Fonts->AddFontFromFileTTF(file, size, nullptr, ranges);
    // Windows: supplement with the symbol font for any glyphs Segoe UI lacks.
    static const char* sym = "C:/Windows/Fonts/seguisym.ttf";
    static const ImWchar symRanges[] = {0x2190, 0x21FF, 0x2600, 0x26FF, 0};
    if (fileExists(sym)) {
        ImFontConfig merge;
        merge.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(sym, size, &merge, symRanges);
    }
    return font;
}

// Load the best available system font for the UI and the splash.
static void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    const char* fonts[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc"
    };
    for (const char* f : fonts) {
        if (!fileExists(f)) continue;
        addFont(f, 16.0f);
        const char* light = fileExists("C:/Windows/Fonts/segoeuil.ttf")
                            ? "C:/Windows/Fonts/segoeuil.ttf" : f;
        ImFont* title = addFont(light, 50.0f);
        ImFont* small = addFont(f, 13.0f);
        Splash::setFonts(title, small);
        return;
    }
    io.Fonts->AddFontDefault();
}

// ---------- splash window state ----------
static SDL_Window* g_splash  = nullptr;
static Uint32      g_splashT = 0;
static bool        g_skip    = false;   // user clicked / pressed Escape
static bool        g_earlyQ  = false;   // user closed the splash (Alt+F4)

// Render one splash frame and poll its events.
static void splashSay(const char* status, float progress) {
    if (!g_splash) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) g_earlyQ = true;
        if (e.type == SDL_MOUSEBUTTONDOWN ||
            (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            g_skip = true;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, status, progress);
    ImGui::Render();
    int w, h;
    SDL_GL_GetDrawableSize(g_splash, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(g_splash);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    AttachConsole(ATTACH_PARENT_PROCESS);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        int hCrt = _open_osfhandle((intptr_t)hOut, _O_TEXT);
        if (hCrt != -1) {
            FILE* fp = _fdopen(hCrt, "w");
            if (fp) {
                *stdout = *fp;
                setvbuf(stdout, nullptr, _IONBF, 0);
            }
        }
    } else {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr && hErr != INVALID_HANDLE_VALUE) {
        int hCrt = _open_osfhandle((intptr_t)hErr, _O_TEXT);
        if (hCrt != -1) {
            FILE* fp = _fdopen(hCrt, "w");
            if (fp) {
                *stderr = *fp;
                setvbuf(stderr, nullptr, _IONBF, 0);
            }
        }
    } else {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }
#endif
    // --selftest: run all tests headlessly and exit.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--selftest") {
            SelfTest::registerAll();
            int failures = SelfTest::run();
            return (failures == 0) ? 0 : 1;
        }
    }

    // --render project.morpheus out.png --frame N: offline frame render
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--render") {
            std::string projFile;
            std::string outFile = "out.png";
            int64_t frameNum = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') projFile = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') outFile = argv[++i];
            for (int j = 1; j < argc; ++j) {
                if (std::string(argv[j]) == "--frame" && j + 1 < argc) {
                    frameNum = std::stoll(argv[j + 1]);
                }
            }
            if (!projFile.empty()) {
                Project::Project p;
                if (!Project::load(p, projFile)) {
                    printf("Failed to load project: %s\n", projFile.c_str());
                    return 1;
                }
                if (p.compositions.empty()) {
                    printf("Project has no compositions: %s\n", projFile.c_str());
                    return 1;
                }
                const auto& comp = (p.activeComp >= 0 && (size_t)p.activeComp < p.compositions.size())
                    ? p.compositions[(size_t)p.activeComp] : p.compositions[0];
                Project::Tick t = comp.frameRate.tick_of(frameNum);
                Raster::Image img = Render::render_frame(&p, comp.id.v, t, 1.0);
                std::vector<uint8_t> rgba = img.to_rgba8();
                if (stbi_write_png(outFile.c_str(), (int)img.width, (int)img.height, 4, rgba.data(), (int)img.width * 4)) {
                    printf("Rendered frame %lld to %s (%ux%u)\n", (long long)frameNum, outFile.c_str(), img.width, img.height);
                    return 0;
                } else {
                    printf("Failed to write image: %s\n", outFile.c_str());
                    return 1;
                }
            }
        }
    }

    std::string capture, scriptName, projectPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--capture" && i + 1 < argc) capture = argv[++i];
        else if (a == "--script" && i + 1 < argc) scriptName = argv[++i];
        else if (a == "--render" && i + 1 < argc) projectPath = argv[++i]; // phase 3+
        else if (!a.empty() && a[0] != '-') projectPath = a;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Start hidden; the splash shows while loading.
    const Uint32 winFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN;
    SDL_Window* win = SDL_CreateWindow("Morpheus",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1600, 960, winFlags);
    if (!win) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(win, 900, 560);
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);

    // Window icon (a simple BMP next to the exe).
    char* base = SDL_GetBasePath();
    const std::string basePath = base ? std::string(base) : std::string();
    if (base) SDL_free(base);
    SDL_Surface* icon = SDL_LoadBMP((basePath + "assets/icon.bmp").c_str());
    if (icon) SDL_SetWindowIcon(win, icon);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // no imgui.ini
    loadFont();

    // Splash window: a small borderless window sharing the main GL context.
    if (capture.empty()) {
        g_splash = SDL_CreateWindow("Morpheus",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    (int)Splash::W, (int)Splash::H,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (g_splash && SDL_GL_MakeCurrent(g_splash, gl) != 0) {
            SDL_DestroyWindow(g_splash);
            g_splash = nullptr;
            SDL_GL_MakeCurrent(win, gl);
        }
        if (g_splash && icon) SDL_SetWindowIcon(g_splash, icon);
    }
    if (icon) SDL_FreeSurface(icon);

    ImGui_ImplSDL2_InitForOpenGL(g_splash ? g_splash : win, gl);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_splashT = SDL_GetTicks();
    splashSay("Loading...", 0.1f);

    // Locate the prefs folder.
    char* pp = SDL_GetPrefPath("Cursed Entertainment", "Morpheus");
    App::init(win, pp ? std::string(pp) : std::string(), g_splash ? splashSay : nullptr);
    if (pp) SDL_free(pp);

    // Keep the splash up for at least 1.5 s, or until clicked.
    if (g_splash) {
        for (;;) {
            const Uint32 elapsed = SDL_GetTicks() - g_splashT;
            if (g_skip || g_earlyQ || elapsed >= 1500) break;
            splashSay("Ready", 1.0f);
        }
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyWindow(g_splash);
        g_splash = nullptr;
        SDL_GL_MakeCurrent(win, gl);
        ImGui_ImplSDL2_InitForOpenGL(win, gl);
    }

    if (capture.empty()) {
        SDL_ShowWindow(win);
        SDL_MaximizeWindow(win);
        SDL_RaiseWindow(win);
    }
    if (g_earlyQ) App::requestQuit();
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // ---------- main loop ----------
    int frames = 0, quietFrames = 0;
    while (!App::wantsQuit()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) App::requestQuit();
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_CLOSE &&
                e.window.windowID == SDL_GetWindowID(win))
                App::requestQuit();
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        App::frame();
        ImGui::Render();
        int w, h;
        SDL_GL_GetDrawableSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frames;

        // --capture mode: run the script on frame 2, then save a screenshot and quit.
        if (!capture.empty()) {
            if (frames == 2 && !scriptName.empty()) {
                App::script(scriptName);
            }
            quietFrames = App::busy() ? 0 : quietFrames + 1;
            if (frames >= 8 && quietFrames >= 4) {
                std::vector<unsigned char> px((size_t)w * h * 4);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                // Flip vertically (OpenGL origin is bottom-left; BMP is top-left).
                std::vector<unsigned char> flipped(px.size());
                for (int y = 0; y < h; ++y)
                    memcpy(&flipped[(size_t)y * w * 4],
                           &px[(size_t)(h - 1 - y) * w * 4],
                           (size_t)w * 4);
                SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(
                    flipped.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
                SDL_SaveBMP(s, capture.c_str());
                SDL_FreeSurface(s);
                printf("Captured: %s\n", capture.c_str());
                break;
            }
        }

        SDL_GL_SwapWindow(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
