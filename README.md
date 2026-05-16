# Topdown

A custom 2D top-down action game built in C++ with [raylib](https://www.raylib.com/).

The project is a gritty, story-driven top-down shooter with real-time movement, scripted levels, cutscenes, NPC AI, combat, level triggers, soundscapes, and a custom data-driven asset pipeline. The game uses authored 2D levels, Lua scripting, JSON asset definitions, and a fixed internal 1920×1080 render resolution.

The engine is purpose-built for this game rather than being a general-purpose framework. Most systems are procedural/data-oriented C++ built around a central `GameState`.

## Features

- 2D top-down shooter gameplay
- Real-time player movement and aiming
- NPC AI with melee/ranged combat support
- Lua-driven level scripting
- JSON-based assets and save data
- Trigger volumes, props, doors, windows, sound emitters, and effects
- Still-image cutscene system with fades, narration text, and skipping
- Objective marker UI
- raylib-based rendering/audio/input

## Build Requirements

Required:

- CMake 3.15 or newer
- A C compiler
- A C++17 compiler
- Git, because raylib is fetched through CMake `FetchContent`

Linux additionally needs the native development libraries required by raylib/X11. At minimum this project links against:

- X11
- Xcursor

Depending on your distro/raylib setup, you may also need common OpenGL/X11 development packages such as OpenGL, Xrandr, Xinerama, Xi, and Mesa development headers.

On Debian/Ubuntu-style systems, a typical starting point is:

```sh
sudo apt install cmake git build-essential libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl1-mesa-dev
````

On Arch/EndeavourOS-style systems:

```sh
sudo pacman -S cmake git base-devel libx11 libxcursor libxrandr libxinerama libxi mesa
```

## Building

From the root of the repository, run:

```sh
./buildDist.sh
```

This runs:

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target package_game
```

The packaged build is written to:

```sh
cmake-build-release/dist
```

## Running

After building:

```sh
cd cmake-build-release/dist
./topdown
```

The release package expects the `assets/` directory to sit next to the executable, which the packaging step handles automatically.

## Development Notes

Debug/development builds use an absolute `ASSETS_PATH` pointing at the repository `assets/` folder. Release builds use:

```cpp
ASSETS_PATH="./assets/"
```

so the executable can run from the packaged `dist` directory.

raylib 5.5 is fetched automatically by CMake.
