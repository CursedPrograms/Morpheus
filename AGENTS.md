# Morpheus: notes for coding agents

Morpheus is Atelier's motion graphics and compositing app (After Effects' kind): a C++ port of **EffectCraft**
(`D:\cc\_src\effectcraft`, Rust, MIT OR Apache-2.0, so its code and ideas may be ported; credit it in comments as
"after EffectCraft (crates/...)"). You build a composition out of layers (solids, shapes, text, footage, other
compositions), animate their properties with keyframes, stack effects on them and render the result. Named after
the Greek god of dreams who takes any shape.

Read EffectCraft's own `AGENTS.md`, `README.md`, `ROADMAP.md` and `docs/` first: they say what every crate does.
EffectCraft is very big (about 450,000 lines); port it in the phases below, the core first, and keep each step
small enough to build, test and commit.

## The stack (as the other Atelier apps)

C++17, CMake, SDL2 + OpenGL 3 + Dear ImGui, stb_image / stb_truetype, FFmpeg's libraries or the ffmpeg program for
video in and out (EffectCraft decodes in Rust; we may use FFmpeg instead), miniaudio for audio. Copy the setup from
the Atelier apps already here:

- `D:\cc\Mosaic-Mask\cpp`: CMakeLists.txt fetching SDL2 and Dear ImGui, miniaudio, the splash (`src/splash.*`),
  language packs (`src/lang.*`, `lang/`), build.bat / .ps1 / .sh and install.bat / .ps1 / .sh.
- `D:\cc\Chimera\cpp`: a big Atelier window (menus, tool strip, panels, undo, `--script NAME --capture shot.bmp`
  test hooks), and its layers, blend modes, shape layers (`src/shape.*`) and text layers (`src/text.*`), which
  Morpheus can reuse.

The look: **EffectCraft's UI exactly**: its layout, colours, icons, fonts, spacing and behaviour (Craft ports keep the
original's UI; see `D:\cc\Atelier\DESIGN.md`, "Craft ports"). From Atelier it takes only the splash screen, the 12
languages (English plus ja, zh, es, ko, fr, de, ru, sr, af, Yucatec Maya, Pennsylvania Dutch), opening empty, the
installer and shortcut names, and Morpheus's own logo.

## The phases (in order; commit each step that builds and works)

1. **The skeleton**: the window in Atelier's look with EffectCraft's panels laid out (Project, Composition,
   Timeline, Effect Controls, Tools, Preview, Info), splash, languages, build and install scripts, `Morpheus.exe`.
2. **The project model** (`crates/project`, `crates/time`, `crates/keyframe`): a project of compositions; a
   composition has a size, frame rate and length, and layers (solid, shape, text, image, footage, a nested
   composition, null, camera, light); each layer has a transform (anchor, position, scale, rotation, opacity) and
   in / out points. Saved and opened as `.morpheus` JSON.
3. **Rendering a frame** (`crates/render`, `crates/raster`): a composition at time t, its layers composited with
   their transforms and blend modes, on the GPU or the CPU, shown in the Composition panel.
4. **The Timeline**: layers as bars, the playhead, scrubbing, in / out trimming, the layer's properties folded
   under it, play in real time (caching frames).
5. **Keyframes** (`crates/keyframe`): stopwatch per property, keyframes on the timeline, linear / hold / Bezier
   interpolation, easing, the graph editor; motion paths drawn in the Composition panel.
6. **Shapes and text layers** (`crates/path`, `crates/text`), animated; trim paths, repeaters.
7. **Effects** (`crates/effects`): the Effect Controls panel and the effects one family at a time (blur, colour
   correction, distort, generate, keying, stylize...), each setting a slider and keyframable.
8. **Masks and track mattes**, layer styles.
9. **Footage**: video and audio in, the render queue: out to MP4 / WebM / PNG sequence / GIF.
10. **Expressions** (`crates/expr`), **3D** cameras and lights, **Lottie** in and out (`crates/lottie`).

## Testing

Every feature gets a test hook: `Morpheus.exe --script NAME --capture shot.bmp` (drives it as a user would, prints
a line, saves a screenshot and quits), and `--render project.morpheus out.png --frame N` renders one frame offline
for checking. Build, run the test, look at the screenshot, then commit.

## Port it properly (this overrides speed)

Morpheus is a port of EffectCraft, not something inspired by it. For every feature:

- **Icons: use EffectCraft's exact icons.** EffectCraft draws its icons in code on a 16x16 grid (`crates/ui-egui/src/icons.rs`, 853 lines, original artwork, MIT OR Apache-2.0). Port that file's drawing one to one onto ImDrawList so every icon (tools, panels, timeline switches, transport) looks exactly like EffectCraft's, used in the same places. Never draw your own or use letters as icons. The app logo stays Morpheus's own.
- **Read the original first.** Open the EffectCraft files that do it and port them function by function: the same logic,
  the same edge cases, the same settings and limits, names in C++ style. Note at each function where it came from:
  `// after EffectCraft (crates/.../file.rs fn_name)`.
- **Port its tests too.** Every `#[test]` in the files you port becomes a C++ test (`cpp/tests`, built by CMake and
  run by `Morpheus.exe --selftest`). A feature is done only when the original's tests pass in C++, plus the screenshot
  check. Run all the tests before every commit; never delete or weaken a test to make it pass.
- **Nothing simplified.** No placeholders, stubs, "first cut", "simplified" or "not a full ..." versions. If EffectCraft
  does it, Morpheus does it. If something truly can't be ported (a Rust-only dependency, another platform), write why
  under "Kiro asks" in REVIEW.md and port everything around it.
- **Say what was ported.** Each commit message names the EffectCraft files and functions it ports and how many of their
  tests pass (e.g. "edit.rs heal, join_gain_env; 4/4 tests").
- When a phase is done, compare it with EffectCraft file by file and list anything left in REVIEW.md before moving on.

## House rules

- Comments say what and why, in plain words. 4-space indents. A short comment on each function.
- MSVC traps: `near`, `far`, `small`, `min`, `max` are Windows macros (define NOMINMAX; don't use them as names).
- Don't write `\n` inside C++ string literals through a shell heredoc; edit files directly.
- Commit each finished step to `main` with a full-sentence message ending in a blank line and
  `Co-Authored-By: Kiro <noreply@kiro.dev>`. Push to GitHub once a phase works
  (`git push -u origin main`; the repo is https://github.com/CursedPrograms/Morpheus). If the remote isn't there yet,
  commit and carry on; don't create the repo yourself.
- Don't commit build output (`build/`, `dist/`).
- Read `REVIEW.md` in this folder before each new step: Claude reviews every commit and writes fixes there. Do
  everything under "Open" first.
- If you're blocked, write why at the bottom of REVIEW.md under "Kiro asks" and stop.
- Installer shortcuts (Start Menu, desktop, Linux .desktop Name=) are named "Atelier - <Name>" so the suite sorts together; installing and uninstalling also remove the old plain-named shortcut.

## The repo (once phase 1 works, and kept up to date after each phase)

Make it look like the user's other Atelier repos (copy from `D:\cc\Mosaic-Mask`, `D:\cc\Chimera`, `D:\cc\Arachne`):

- **README.md**: what Morpheus is, a screenshot of the window (`images/`), what works so far and what's coming, minimum
  requirements (a table, as Arachne's), how to build and install, credits (after its Craft original, with its
  licence), and the Cursed footer: `---`, then centred "© Cursed Entertainment 2026" and the itch.io logo link
  (`logo-wide-grey.png`, width 250), as in the other READMEs. Never write "our" / "ours".
- **A GitHub Pages site**: `index.html` at the root, `site/style.css`, `site/site.js` (CSS and JS in their own files,
  never inline), using the main CSS from the cursedentertainment repo as the others do, solid colours, no gradients
  or shadows, works on phones, the shared footer (RIFT's `site/footer.*`), a QR code pointing at the page.
- `.github/FUNDING.yml` (copy KIDA-Robot-v01's) and `link.bf` (the same file as the other repos).
- **The description and topics**: set them with `gh repo edit CursedPrograms/Morpheus --description "..." --add-topic ...`
  (topics like atelier, cpp, the kind of app, imgui, sdl2, windows) and turn on Pages from main, root
  (`gh api repos/CursedPrograms/Morpheus/pages -X POST -f "source[branch]=main" -f "source[path]=/"`).
