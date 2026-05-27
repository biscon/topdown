# AGENTS.md

## Purpose

This repository contains a custom C++ game engine focused on a **top-down action RPG**.

The codebase also includes remnants of an older **point-and-click adventure engine**. This is being actively cannibalized for reusable systems.

**Important:**
- The top-down engine is the primary target.
- The adventure engine only needs to **compile**, not function correctly.
- Do NOT spend time maintaining or improving adventure systems unless explicitly instructed.
- Over time, the adventure code will be removed.

---

## Repository Map

- `sources/topdown/` contains the primary top-down game systems.
- `sources/topdown/TopdownData.h` is the root top-down data aggregation header.
- `sources/topdown/Topdown*Data.h` headers contain subsystem-specific top-down data models.
- `sources/nav/` contains project-owned Recast/Detour integration and query/build wrappers.
- `sources/recast/`, `sources/detour/`, `sources/rvo2/`, `sources/clipper2/`, and `sources/lua/` are vendored third-party code.
- `sources/scripting/` contains the Lua VM, coroutine wait system, and C++/Lua API bridge.
- `sources/resources/` owns texture, sprite, sound, and music resource handles/lifetimes.
- `sources/audio/`, `sources/render/`, `sources/input/`, `sources/ui/`, `sources/debug/`, `sources/save/`, and `sources/settings/` are shared engine subsystems.
- `assets/levels/<level_id>/` contains top-down level data (`.json`, `.tmj`, `.lua`, local audio/shadow/effect assets).
- `assets/scenes/` contains older adventure/scene data. Treat it as compile-only legacy data unless explicitly instructed.
- `assets/scripts/` contains reusable Lua modules loaded through `require`.
- `assets/shaders/` contains GLSL shader files referenced by rendering/effect systems.

---

## Build and Verification

This is a CMake C++17 project. `CMakeLists.txt` is authoritative for compiler settings and dependency versions.

Useful commands from the repository root:

```sh
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

There is no dedicated test target currently visible in the repository. For most code changes, verify that a Debug build compiles.

Raylib is fetched by CMake `FetchContent`. Do not treat README dependency version text as authoritative if it conflicts with `CMakeLists.txt`.

---

## Core Architecture Principles

### Data-Oriented Design
- Prefer **plain structs + data containers** over polymorphism.
- Avoid inheritance unless explicitly required.
- Keep logic simple, explicit, and debuggable.

### Handles Over Pointers
- Do NOT store raw pointers to elements inside containers.
- Use **integer handles / indices** for cross-references.

### Explicit Over Clever
- Avoid overengineering and unnecessary abstraction.
- Prefer straightforward code over “smart” solutions.

---

## Data Organization

### Authored vs Runtime Separation
- Maintain a clear split between:
    - `Authored` data (loaded from Tiled / assets)
    - `Runtime` data (mutable game state)
- Duplication between these is **intentional and acceptable**.
- This separation simplifies reasoning and future save/load systems.

### Data File Structure
- The `*Data` pattern exists to avoid circular dependencies.
- Do NOT put everything into a single monolithic header.
- Keep `TopdownData.h` as the root aggregation header that includes subsystem data headers and defines only cross-subsystem aggregate/root state.
- Put subsystem-specific top-down data in focused headers such as:
    - `TopdownCoreData.h`
    - `TopdownLevelObjectData.h`
    - `TopdownCollisionData.h`
    - `TopdownRenderData.h`
    - `TopdownPlayerData.h`
    - `TopdownNpcData.h`
    - `TopdownDoorData.h`
    - `TopdownWindowData.h`
    - `TopdownTriggerData.h`
    - `TopdownItemData.h`
    - `TopdownPropData.h`
    - `TopdownNavData.h`
- When adding data for an existing subsystem, extend that subsystem's `Topdown*Data.h` rather than `TopdownData.h`.
- Add a new focused `Topdown*Data.h` when new data does not clearly belong to an existing subsystem.
- Small root-level aggregate structs may remain in `TopdownData.h` only when they intentionally tie multiple subsystems together.

### Units and Coordinates
- World positions are pixel-space `Vector2` values unless local code clearly says otherwise.
- The internal render resolution is fixed at `INTERNAL_WIDTH` x `INTERNAL_HEIGHT` (`1920 x 1080`).
- `dt` values are seconds.
- Most gameplay timers are stored in milliseconds. Name millisecond fields with an `Ms` suffix.
- Prefer radians for runtime orientation when interacting with movement/render math, and degrees only where authored data or UI explicitly uses degrees.

---

## System Design

### File Responsibility
- Prefer **file-scoped systems** (e.g., Level, Character, Combat).
- When a file grows to handle multiple unrelated responsibilities:
    - Split it into focused subsystems.

### Orchestration vs Implementation
- High-level files (e.g., `LevelUpdate.cpp`) should:
    - Orchestrate systems
    - Not contain large amounts of feature-specific logic

- New features that are substantial should:
    - Get their own `.cpp/.h` files
    - Not be injected into unrelated systems

### Subsystem Growth Rule
If a feature requires:
- ~200+ lines update logic
- ~200+ lines rendering
- ~200+ lines loading/setup

→ It likely deserves its **own subsystem files**

---

## Separation of Concerns

### Keep These Separate When Practical:
- Update (simulation)
- Rendering
- Input
- Loading / initialization

Avoid mixing:
- Gameplay logic inside rendering code
- Rendering logic inside update code

---

## Performance Guidelines

This is a game engine. Performance matters, especially in code that runs every frame.

### Avoid Dynamic Allocation in Hot Paths
- Do **not** introduce repeated `malloc`/`free`, heap allocation, or unnecessary container growth in per-frame code.
- Avoid dynamic memory allocation in:
    - update loops
    - rendering loops
    - AI loops
    - collision loops
    - path-following / steering / combat hot paths

If a system is clearly a hotspot:
- prefer preallocation
- reserve container capacity up front
- use fixed upper bounds when practical
- spend more memory if it makes frame-time behavior more stable and predictable

Dynamic allocation is acceptable in non-hot-path code such as:
- level loading
- startup
- asset import
- one-time preprocessing
- editor/debug-only tooling

### Scratch Buffers in Hot Paths
- For per-frame collision, AI, rendering, trigger, and steering work, prefer:
    - runtime-owned scratch vectors with reserved capacity,
    - reused local buffers that do not grow after warm-up,
    - or carefully scoped `thread_local` scratch buffers where ownership is obvious.
- Do not build new temporary vectors inside nested frame loops unless the maximum cost is small, bounded, and clear.
- If adding a vector that is expected to grow during loading/setup, reserve capacity when the expected size is known or easy to estimate.

### Avoid Unnecessary Copies
- Do not copy large structs, vectors, or buffers unnecessarily, especially in per-frame code.
- Prefer mutating runtime data directly when appropriate.
- This is not a pure functional codebase; mutating state is normal and expected.

### Safe Premature Optimization
Avoiding obvious performance traps is encouraged.

Examples of “safe to do early” optimizations:
- avoid heap allocation in hot paths
- avoid repeated temporary container construction each frame
- avoid unnecessary large data copies
- reserve capacity when growth is expected
- keep hot-path logic simple and cache-friendly where reasonable

Do **not** respond to this by writing overcomplicated micro-optimized code everywhere.
The goal is:
- no obvious performance footguns
- no allocator churn in frame loops
- no needless copying in hot paths
- no heroic complexity unless profiling or the problem clearly demands it

### Balance
Keep performance in mind from the start, but do not destroy readability for tiny theoretical wins.

Good:
- simple code
- predictable memory behavior
- stable frame-time friendly patterns

Bad:
- allocator churn every frame
- hidden copies in hot loops
- complex “clever” optimization that makes the system harder to maintain for little gain

---

## Rendering Conventions

- Respect existing rendering structure and passes.
- Do not arbitrarily move rendering into unrelated files.
- Keep world rendering in `LevelRender.cpp` or focused rendering subsystem files.
- Keep debug rendering in debug-specific render files when practical.
- Effect shader definitions live under `sources/render/`; shader assets live under `assets/shaders/`.
- Respect the fixed internal render target and presentation scaling handled by `Main.cpp`.
- If multiple small features exist (e.g., doors, windows):
    - Group them logically if it improves structure (e.g., “interactables”)
    - Avoid fragmentation into too many tiny files

---

## Resources and Assets

- Use `ASSETS_PATH` for runtime asset paths.
- Debug/development builds use an absolute path to the repository `assets/` folder.
- Release builds expect `./assets/` next to the executable; the package target copies assets into `cmake-build-release/dist`.
- Prefer `ResourceScope::Scene` for level-specific assets that should be unloaded on level changes.
- Prefer `ResourceScope::Global` for shared registries, player/common assets, UI assets, and other resources that intentionally survive level changes.
- Do not leak scene-specific assets into global scope just to avoid reloading unless there is a clear reason.
- Do not delete, regenerate, normalize, or replace art/audio/settings files unless explicitly requested.
- Treat untracked binary assets and `settings.json` as user work unless the user says otherwise.

---

## Lua Scripting

Lua is used for level scripts, reusable script modules, cutscene flow, and scripted gameplay events.

When adding or changing a top-down Lua command, check all relevant layers:

- C++ command declaration in `sources/topdown/TopdownScriptCommands.h`
- C++ command implementation in `sources/topdown/TopdownScriptCommands.cpp`
- Lua binding and argument validation in `sources/scripting/ScriptSystemLuaApi.cpp`
- Level scripts or documentation if the command is user-facing

Blocking script commands may yield Lua coroutines and resume through the script wait system. Keep command behavior compatible with coroutine waits, foreground hooks, and pending script starts.

Lua `require` paths are configured in `ScriptSystem.cpp` and include reusable modules under `assets/scripts/`.

---

## AI and Gameplay Logic

- AI behavior should be owned by the **AI mode/system**, not centralized.
- Avoid giant switch-based “god systems” controlling all behavior.
- Keep behavior localized and understandable.

---

## Navigation and Movement

- Pathfinding uses **Recast/Detour** (current system).
- Do NOT reintroduce or extend legacy/homegrown navmesh systems.
- RVO2 is used for **local avoidance**.

---

### State machine updates
When an update function changes a high-level gameplay/AI state, prefer to return immediately rather than also processing the newly-entered state in the same frame. Let the new state run from the top on the next frame unless there is a very strong reason not to. This avoids transition-frame bugs and keeps state flow easier to reason about.

Additional state machine guidance:

- Keep enum responsibilities distinct. In AI code, `AiMode`, `AwarenessState`, and `CombatState` should not silently stand in for each other. Use each enum only for the kind of state it is meant to represent.
- Prefer dedicated enter/leave/reset helpers for meaningful states instead of directly poking enum values and related fields at random call sites. If a state has setup or cleanup, centralize it.
- Active exclusive substates should be updated in one obvious dispatch point. Do not update the same state from multiple places in the same frame.
- Transitioning into a state and updating that state are separate steps. Prefer: enter state now, update it next frame.
- Perception / target-acquisition code should primarily update knowledge and awareness, not repeatedly re-enter, restart, or reset active execution states every frame.
- Separate persistent tactical memory from transient state-local execution data. For example, last known target position may survive a state change, but investigation slot claims and search sweep timers should not.
- Each state should have clear ownership of its local data. Timers, slot claims, watchdog values, path goals, and similar fields should belong to one state and be reset explicitly when leaving that state.
- When a state owns several related fields, prefer grouping them into a dedicated embedded data struct such as `SearchStateData`, `InvestigationStateData`, or `ChaseStateData` instead of scattering them as loose fields on the main runtime struct. Keep these structs plain and embedded; do not introduce heap allocation or OOP state objects for this.
- Prefer state-local data names and helpers that make ownership obvious. It should be easy to see which fields belong to Search, Investigation, Chase, or Attack without relying on comments or tribal knowledge.
- When extending an existing FSM, prefer small hygiene improvements over architectural rewrites: centralize transitions, centralize resets, reduce duplicate dispatch, make ownership of state-local fields explicit, and group related per-state data when it improves clarity.

## Third-Party Libraries

These are vendored directly into the repository:

- Recast/Detour → navigation
- RVO2 → local avoidance
- Clipper2 → polygon operations (not primary nav system anymore)
- Lua → scripting

Guidelines:
- Prefer using these libraries over reinventing functionality.
- Do not replace them without explicit instruction.
- Avoid editing vendored third-party source unless the task explicitly requires it or the project cannot compile otherwise.
- Put project-specific wrappers, conversions, and policy in project-owned files such as `sources/nav/` or `sources/topdown/`.

---

## Refactoring Rules

- Do NOT rewrite large systems unless explicitly asked.
- Preserve working code unless there is a clear benefit to change.
- Prefer **incremental improvements** over large rewrites.

However:

If you detect **clear architectural issues**, you may:
1. Point them out
2. Suggest improvements
3. Then implement (if appropriate)

---

## Code Style Guidelines

- Break large functions into smaller named steps when it improves clarity
- Avoid deeply nested logic where possible
- Use descriptive function names instead of comments where appropriate
- Some comments are fine, but avoid noise

---

## Development Philosophy

The preferred workflow is **iterative and multi-pass**:

1. Implement a working solution
2. Review structure and design
3. Refactor for clarity and maintainability

Do not assume the first implementation is final.

---

## What NOT To Do

- Do not introduce polymorphic hierarchies
- Do not store unstable pointers into containers
- Do not mix unrelated systems into the same file
- Do not optimize prematurely in ways that add lots of complexity
- Do not ignore obvious hot-path performance traps
- Do not “clean up” legacy systems unless asked
- Do not blindly follow existing bad patterns if better structure is obvious

---

## What TO Do

- Follow existing patterns where they are intentional
- Improve structure when it is safe and beneficial
- Keep systems understandable and debuggable
- Keep obvious hot paths allocation-free when practical
- Avoid unnecessary copies in frame-time code
- Think in terms of **long-term maintainability**, not just “making it work”

---

## Final Note

This project favors:
- clarity over abstraction
- structure over cleverness
- iteration over perfection

When in doubt, choose the solution that is easiest to reason about and extend later, while avoiding obvious performance footguns in runtime-critical code.

---
