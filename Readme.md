# Hybrid Engine
[简体中文版Readme](README_zh-CN.md)

A personal engine project for studying modern engine architecture, system implementation, and technical exploration.

## Overview

Hybrid Engine is a personal project focused on learning, researching, and practicing modern engine architecture. The goal of this project is not simply to build a runnable engine prototype, but to gradually understand the responsibilities, collaboration patterns, and architectural boundaries of core engine modules through actual implementation, while also exploring technical areas worth developing in greater depth.

At the current stage, the project is mainly focused on building a solid foundation for the core framework, including rendering, resource management, scene systems, editor workflows, input, events, and physics. Once the overall architecture becomes more stable, I plan to dive deeper into several selected directions, such as rendering pipeline design, physics-system implementation, engine performance optimization and profiling, as well as editor interaction and workflow logic, while gradually moving toward cross-platform and multi-API support.

For me, this project is both a long-term learning process in low-level engine development and a continuing practice platform for validating architectural ideas, accumulating engineering experience, and exploring technical details.

![Editor Preview](docs/images/editor_preview.png)

## Requirements

The current primary development environment is:

- Windows 10 or Windows 11
- CMake 3.20 or later
- A compiler with C++17 support
- Visual Studio 2022 / MSVC is the recommended toolchain
- Git with submodule support
- A GPU and driver environment capable of running OpenGL 4.5

Notes:

- The repository already contains some traces of cross-platform build support, such as a macOS build script and CI workflow, but the current editor codebase is still primarily Windows-first.
- The editor depends on bundled third-party libraries under `engine/3rdparty`, so submodules must be initialized before the first configure step.

## Build and Run

Initialize submodules first:

```bash
git submodule update --init --recursive
```

Configure and build manually:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Or use the Windows helper script:

```bat
build_windows.bat
```

Run the editor:

```bat
bin\HybridEditor.exe
```

Runtime notes:

- The build process copies the editor executable, shaders, and editor resources into the `bin/` directory.
- On first launch, the engine automatically initializes `bin/GameProject/` as the project root (currently a hardcoded path) and creates a default `GameProject.hyproj` if it does not already exist.
- Runtime logs are written to `bin/hybrid_engine.log`.

## Documentation

Project documentation is located under `docs/`:

**Development Log**

- [Changelog](docs/CHANGELOG_zh-CN.md): development history and version milestones

**Detailed Documentation**

- [Render System](docs/systems/render_system.md): current render-system architecture and pass layout
- [Resource System](docs/systems/resource_system.md): runtime/editor asset pipeline, registry, cache, and loading flow
- [Render ECS Asset Chain](docs/systems/render_ecs_asset_chain.md): integration notes for meshes, materials, ECS, and editor flow within the render path
- [Event System](docs/systems/event_system.md): event dispatch and input/event bridge notes
- [Log System](docs/systems/log_system.md): logging structure and usage

## Features and Current Status

The following parts are already implemented or basically usable:

- Core runtime foundation, including logging, events, input, the window system, and the main loop
- Basic rendering workflow, including the OpenGL backend, framebuffer-based rendering path, and the split between Scene and Game viewports
- Scene system, including EnTT-based entities and components, scene serialization, hierarchy data, and the `scene_document` flow
- Editor basics, including the docking UI, Hierarchy, Inspector, Scene View, Game View, Project Panel, gizmo interaction, and object picking
- Asset-system foundation, including VFS logical paths, the asset registry, `.meta` files, cooked cache output, and loading for textures, meshes, materials, and scenes
- Import workflow, including OBJ import, material generation, texture import, drag-and-drop placement into the scene, and initial hot-reload support
- Basic lighting support in the current render path, including directional lights and point lights
- An early physics baseline, including AABB collision, collider registration, rigidbody iteration, and collider gizmo debugging

The following areas are still under active development:

- The render-pipeline split is still being refined, and the higher-level pass orchestration structure has not been fully settled yet
- Shadow, outline, and post-processing paths have been started but are not complete yet
- The physics system is still in an early iteration phase and remains unstable overall
- The editor workflow is already usable for experiments and feature validation, but there is still significant room for improvement in polish, reliability, and tooling depth
- Audio and scripting systems have not been implemented yet

## Roadmap

My near-term focus will mainly be on the following directions:

- Continue refining the render-pipeline structure, including pass execution flow, shader management, and the data upload path
- Improve physics behavior, rigidbody flow, and editor-side debugging support
- Strengthen scene-editing reliability across edit/play mode transitions
- Continue improving the stability of asset hot reload, cache invalidation, and the import workflow

Planned future work includes:

- More complete rendering features, such as shadows, outlines, and post-processing
- A more polished editor workflow and better panel-level usability
- Broader coverage for default resources and error paths in the asset system
- Exploration of an audio system
- Scripting support and runtime extensibility after the core architecture becomes more stable

For the broader plan, see [docs/plans/PROJECT_PLAN_ENG.md](docs/plans/PROJECT_PLAN_ENG.md).

## Closing Notes

Hybrid Engine is currently being developed as a long-term personal project for learning and research.

Special thanks to [@SaluteBEE](https://github.com/SaluteBEE) for support, collaboration, and discussion.

Contact:

- Portfolio: [Portfolio](https://the-lyricis.github.io/)
- Gmail: picchick1623@gmail.com