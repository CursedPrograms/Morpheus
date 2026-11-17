# Review notes from Claude (read before each new step; fix these first)

## Open

1. New rules from the user, now in AGENTS.md (read its last section, "The repo"): once phase 1 works, push to
   GitHub (the repo exists now: https://github.com/CursedPrograms/Morpheus), and give the repo a README with a screenshot, a
   GitHub Pages site, FUNDING.yml, link.bf, a description and topics, kept up to date after each phase.

## Done

## Kiro asks

Phase 1 and 2 are built, tested, committed and pushed to https://github.com/CursedPrograms/Morpheus.

**Phase 1** (`5518c0f`): the skeleton window with all seven panels (Project, Composition, Timeline, Effect Controls,
Tools, Preview, Info), menu bar, splash screen, 12 languages, build and install scripts, and the repo files
(README.md, index.html + site/style.css + site/site.js, .github/FUNDING.yml, link.bf, description and topics set
with `gh repo edit`, Pages turned on from main/root).  Test: `--script skeleton --capture skeleton_test.bmp` — a
1600×960 screenshot of the app window confirmed in the BMP.

**Phase 2** (`fd9ccfb`): the project model — Project, Composition, Layer (all nine kinds), Transform, in/out
points, JSON save/load (`project.cpp`).  Menu items New/Open/Save/Save As/Quit and their keyboard shortcuts are
now wired to real actions in `app.cpp`; clicking a composition in the Project panel activates it.  Test:
`--script new_project --capture new_project_test.bmp` — creates a project, adds two layers (Solid, Null), saves
to `test_project.morpheus`, reloads it, and prints "comp name=ok, layers=ok, kind=ok" to confirm the round-trip.

**Next:** phase 3 (render a frame on the GPU and show it in the Composition panel).  The main unknown is
deciding between an offscreen FBO approach (render to texture, blit into the ImGui child window) and a direct
OpenGL draw into the panel's clip rect.  FBO is cleaner and will be needed for phase 9 anyway.
