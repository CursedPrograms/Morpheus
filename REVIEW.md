# Review notes from Claude (read before each new step; fix these first)

## Open

None.

## Done

- **Phase 2 Property Tree & Keyframe Base (`crates/project/src/props.rs`, `crates/keyframe`)**:
  - Property tree: `Property`, `PropGroup`, `Node`, `ParamUi`, `Expression`, `GroupKind`, masks with `MaskMode`, feather, `Seg` path parsing (`@uid`, `match`, `#index`, `match#N`).
  - Values and keyframing: `Value` (Scalar, Vec2, Vec3, Color, Bool, Enum, Str, Layer, Path, Gradient), `Keyframe`, `Ease`, temporal and spatial Bezier interpolation, auto-Bezier, roving, time reverse, muting, orientation slerp.
  - Layer transform integration: layer transforms are properties within the property tree (`transform/anchor`, `transform/position`, `transform/scale`, `transform/rotation`, `transform/opacity`), synchronized bidirectionally.
  - JSON serialization & deserialization: full property tree and keyframes round-trip in JSON.
  - Ported tests from `crates/project/src/tests.rs` (paths_resolve, masks_and_occurrence_paths, animate_and_roundtrip_json, numbered_names_keep_digits_that_end_a_word, unique_names_and_cycles, same_content_sees_allocated_ids_and_comp_edits) and `crates/keyframe/src/tests.rs` (22 keyframing & easing tests). All 59/59 self-tests pass.

- **Phase 1 UI Layout & Vector Icons (items 0b and 2)**:
  - Ported `crates/ui-egui/src/icons.rs` (853 lines) 1:1 to C++ (`src/icons.h`, `src/icons.cpp`). All vector icons drawn on a 16x16 design grid onto `ImDrawList` (tools, switches, project items, transport, UI, appearance).
  - Ported tests from `icons.rs` (`icons::concave_glyphs_are_filled_as_convex_pieces`, `icons::keyframe_bounds_follow_each_half`, `icons::keyframes_are_drawn_without_a_dark_outline`). All 25/25 self-tests pass.
  - Replaced all letter buttons, character stand-ins, and bullet points with exact EffectCraft vector icons across the header toolbar, Project panel, Composition viewer, Timeline panel, Preview panel, and Properties panel.
  - Toolbar: Atelier Morpheus brand mark, Home icon, 12 canonical tool slots with flyout indicators, Snapping checkbox, and workspace tabs (Default, Review, Learn, Small Screen, Standard) plus overflow menu.
  - Composition viewer: canvas with checkerboard and centered comp boundary frame, bottom bar with zoom/Fit, grid & guides, current timecode, snapshot, show snapshot, channel picker (RGB), resolution, ROI, transparency grid, fast previews.
  - Timeline: current timecode, fps counter, search box, top switches (shy, frame blend, motion blur, graph editor), column headers with eye/speaker/solo/lock icons, label diamond, #, type icons, switches (shy, collapse, quality, fx, frame blend, motion blur, adjustment, 3D), Parent & Link (pick whip), layer bars with work area, time ruler, playhead, and zoom slider with MountainSmall/MountainLarge icons.
  - Preview panel: transport buttons (first, prev, play/pause, next, last, loop), settings rows, cached frames bar.
  - Properties panel: Layer Transform with reset and blue stopwatch icons.
  - Updated `images/window.bmp`, `skeleton_test.bmp`, and `new_project_test.bmp`.

- **Phase 2 Exact Time & Project Model (item 1 core)**:
  - Exact time integer ticks (254,016,000,000 per second), FrameRate, TimeRange, timecode roundtrips (16 tests).
  - Project model: compositions, layer kinds, full Switches, Transform, JSON serialization (6 tests).

- **Phase 4 Timeline interaction core (`crates/ui-egui/src/panels/timeline.rs`)**:
  - Layer duration bars use composition-time mapping and label colours; the playhead is frame-accurate and scrubs from the ruler. Bar drags move layers or trim In/Out while keeping `startTime` coherent, with Shift snapping to other layer edges.
  - Layer rows fold their property groups; work area, time ruler, zoom, transport and playback are connected to the software renderer. `Render::FrameCache` is bounded by bytes and reused by the Composition viewer and Preview cache indicator.
  - Added `--script timeline --capture ...` and ported helper/test behavior from `fit_pps`, `set_zoom`, `the_view_pages_to_the_cti`, `cti_snaps_to_the_nearest_candidate`, and `dragging_bar_ends_trims_in_and_out`. The Release build and all 96/96 self-tests pass; the capture was visually checked.
  - Not yet ported from the much larger Timeline panel: layer/group reorder, multi-selection and marquee, marker editing, effect/source rows, full column/switch menus, property reveal/search shortcuts, and graph/keyframe interaction. Phase 5 covers the keyframe and graph-editor portion.

## Kiro asks

EffectCraft source access was restored on 2026-11-23. Phase 4's requested core Timeline interactions now build and pass 96/96 self-tests. Phase 5 (keyframe UI and graph editor) is next; the remaining Timeline features above are not blockers to that work.

**Phase 1** (`5518c0f`): the skeleton window with all seven panels (Project, Composition, Timeline, Effect Controls,
Tools, Preview, Info), menu bar, splash screen, 12 languages, build and install scripts, and the repo files
(README.md, index.html + site/style.css + site/site.js, .github/FUNDING.yml, link.bf, description and topics set
with `gh repo edit`, Pages turned on from main/root).

**Phase 2** (`fd9ccfb`, `c6221bc`): the project model, exact time ticks, property tree, keyframe base, compositions, layers, switches, transforms,
in/out points, and JSON save/load. Test: `Morpheus.exe --selftest` (59/59 pass).
