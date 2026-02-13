# Hybrid Engine v0.0.1

## Build and Environment (keep updated)

Prerequisites:

- CMake 3.20+
- A C++17 compiler (MSVC is the primary target)

Initialize submodules:

```
git submodule update --init --recursive
```

Configure and build:

```
cmake -S . -B build
cmake --build build --config Release
```

Or use:

```
build_windows.bat
```

Run (after build):

```
<repo>/bin/TDA572Editor.exe
```

## 

## Version Notes

### v0.0.2 (current)
- Logging: spdlog wrapper with core/client channels and macro helpers.
- Event system: Event/Dispatcher plus application/input event types; Layer/Overlay reverse dispatch; SurfaceIO bridges GLFW callbacks to engine events.
- Input: two-phase flow (InputLayer sampling + LayerStack dispatch) with keyboard, mouse, scroll, and char input supported.
- Window & main loop: WindowSystem (GLFW + GLAD); engine loop runs update → pollEvents → render → swap.
- Rendering (initial): RenderSystem builds an FBO, renders a basic cube, and supports EditorCamera controls (WASDQE, RMB drag, scroll zoom).

## Dependencies

- GLFW
- GLAD
- ImGui
- GLM (submodule, not wired yet)
- Spdlog

### v0.0.1

- Pre-release scaffold.
- Currently only a basic framework and core third-party libraries.

## Dependencies

- GLFW
- GLAD
- ImGui
- GLM
