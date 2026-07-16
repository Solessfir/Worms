# Worms

## Gameplay
Local 2-player turn-based artillery game. Each player controls one Worm, aims with the mouse, charges shot power, and fires a projectile with gravity arc at the other Worm. Terrain blocks take damage and break apart from explosions. Last Worm standing wins.

## Stack
* **Standard** `CPP 20`
* **Build Tool:** `Premake`
* **Backend:** `Raylib`
* **Logging:** `spdlog`

## Download

Pre-built binaries available on the [Releases page](https://github.com/Solessfir/Worms/releases):
* **Windows:** `Worms_Win64.zip`
* **Linux:** `Worms_Linux_x86_64.tar.gz`

## Build Prerequisites
- **Windows:** Visual Studio 2022 or higher
- **Linux:** `g++`, `make`, `wayland-scanner`, plus GL + Wayland dev libraries (e.g. `libgl1-mesa-dev libwayland-dev wayland-protocols libxkbcommon-dev`)

## Building on Windows
1. Run `Setup.bat`
2. Open `Worms.sln`
3. Build

## Building on Linux
1. Run `./Setup.sh`
2. Debug build: `make config=debug -j$(nproc)`
    * Release: `make -j$(nproc)`
3. Run: `./bin/{Debug/Release}-linux-x86_64/Worms/Worms`

## Controls
* Mouse - aim
* LMB (hold + release) - charge and fire
* A/D - walk
* Space - jump
* `Esc` to exit

## Adding Custom Levels
New levels can be added to `assets/levels/level_#.txt`

### Example Format:
```

R    R
YYYYYY
GGGGGG
OOOOOO
```

* **G** - Green
* **O** - Orange
* **R** - Red
* **W** - Wall (Obstacle)
* **Empty** - gap, no block

## Audio:
* Music: https://suno.com/
* Sound:
    * https://pixabay.com/sound-effects/
    * https://freesound.org/browse/

## Notable Reference
* Tsoding Music Visualizer based on Raylib:
    * https://www.youtube.com/playlist?list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT
    * https://github.com/tsoding/musializer

## Build Warnings on Windows

```
LNK4075: ignoring '/INCREMENTAL' due to '/FORCE' specification
LNK4088: image being generated due to /FORCE option; image may not run
```
Expected, the game runs fine.

`raylib` and the `Win32` API both have a function called `CloseWindow`. We pass `/FORCE:MULTIPLE` so the linker just picks raylib's instead of erroring out ([raylib#729](https://github.com/raysan5/raylib/issues/729)).

So these warnings are side effect of `/FORCE:MULTIPLE`
