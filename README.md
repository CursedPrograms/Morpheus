[![Twitter: @NorowaretaGemu](https://img.shields.io/badge/X-@NorowaretaGemu-blue.svg?style=flat)](https://x.com/NorowaretaGemu)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

<br>
<div align="center">
  <a href="https://ko-fi.com/cursedentertainment">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi" style="width: 20%;"/>
  </a>
</div>
<br>
<div align="center">
  <img alt="C++" src="https://img.shields.io/badge/c++%20-%23323330.svg?&style=for-the-badge&logo=c%2B%2B&logoColor=white"/>
  <img alt="CMake" src="https://img.shields.io/badge/cmake%20-%23323330.svg?&style=for-the-badge&logo=cmake&logoColor=white"/>
  <img alt="OpenGL" src="https://img.shields.io/badge/opengl%20-%23323330.svg?&style=for-the-badge&logo=opengl&logoColor=white"/>
</div>
<div align="center">
  <img alt="PowerShell" src="https://img.shields.io/badge/PowerShell-%23323330.svg?&style=for-the-badge&logo=powershell&logoColor=white"/>
  <img alt="Shell" src="https://img.shields.io/badge/Shell-%23323330.svg?&style=for-the-badge&logo=gnu-bash&logoColor=white"/>
  <img alt="Batch" src="https://img.shields.io/badge/Batch-%23323330.svg?&style=for-the-badge&logo=windows&logoColor=white"/>
</div>
<br>

# Morpheus

**A motion graphics and compositing app for Windows (and Linux), written in C++.** Build a composition out of layers
(solids, shapes, text, footage, nested compositions), animate their properties with keyframes, stack effects on them,
and render the result. Part of **[Atelier](https://github.com/CursedPrograms/Atelier)**, Cursed Entertainment's
creative suite. Named after the Greek god of dreams who takes any shape.
**[Website](https://cursedprograms.github.io/Morpheus/)**

![Morpheus: the app window showing the seven panels — Project, Composition, Timeline, Effect Controls, Tools, Preview, Info](images/window.bmp)

## What works

- **The skeleton (phase 1):** the full Atelier window with all seven panels (Project, Composition, Timeline,
  Effect Controls, Tools, Preview, Info), the menu bar, the splash screen, 12 languages and the dark theme.
- **The project model (phase 2):** compositions with size, frame rate and duration; layers (solid, shape, text,
  image, footage, nested composition, null, camera, light) each with a transform (anchor, position, scale, rotation,
  opacity) and in/out points. Saved and opened as `.morpheus` JSON.

## Coming next

Rendering a frame (phase 3), the Timeline with scrubbing and playback (phase 4), keyframes and the graph editor
(phase 5), shapes and text (phase 6), effects (phase 7), masks and mattes (phase 8), footage in and the render queue
(phase 9), expressions and 3D cameras (phase 10).

## Minimum requirements

| | |
| --- | --- |
| **System** | Windows 10 or 11, 64-bit (Linux builds from source) |
| **Processor** | any 64-bit processor |
| **Memory** | 4 GB RAM |
| **Graphics** | any graphics card with OpenGL 3.3 (2010 or newer) |
| **Disk** | 20 MB |

## Build

Windows: Visual Studio 2022 (C++) and CMake, then `build.bat` (or `build.ps1`). It fetches Dear ImGui into
`external/` once and builds `build/Release/Morpheus.exe`.

Linux: `./build.sh` (needs cmake, a C++17 compiler and libsdl2-dev).

```
build.bat          # Windows
./build.sh         # Linux
```

After building, `install.bat` (or `install.ps1` / `install.sh`) puts the exe and its assets where the OS expects apps.

The code, in `src/`:
- `main.cpp`: SDL2, OpenGL 3 and Dear ImGui; the splash window; the main loop and `--capture` / `--render` hooks
- `app.cpp`: project management, menu actions, the scripted test hooks
- `panels.cpp`: the seven panels in the Atelier layout and look
- `project.cpp`: the project model; JSON save and load
- `splash.cpp`: the splash shown while loading and in Help > About
- `lang.cpp`: the 12 language packs (Language menu)

## Credits

The panel layout and project model are ported from
[EffectCraft](https://github.com/storytold/effectcraft) (MIT OR Apache-2.0).
Built with [Dear ImGui](https://github.com/ocornut/imgui), [SDL2](https://www.libsdl.org) and
[stb](https://github.com/nothings/stb).

<br>
<div align="center">
© Cursed Entertainment 2026
</div>
<br>
<div align="center">
<a href="https://cursed-entertainment.itch.io/" target="_blank">
    <img src="https://github.com/CursedPrograms/cursedentertainment/raw/main/images/logos/logo-wide-grey.png"
        alt="CursedEntertainment Logo" style="width:250px;">
</a>
</div>
