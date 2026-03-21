<h1>
  <img src="docs/images/hybrid_icon.png" alt="Hybrid Engine Icon" width="42" valign="middle" />
  Hybrid Engine
</h1>

[![README](https://img.shields.io/badge/README-Simplified%20Chinese-374151?style=flat-square)](README_zh-CN.md)
[![C++](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=c%2B%2B&style=flat-square)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](./LICENSE)

> A personal engine project for studying modern engine architecture, runtime/editor boundaries, and practical toolchain design through real implementation work.

![Hybrid Engine Editor Preview](docs/images/editor_preview.png)

## Overview

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6)](#requirements)
[![Renderer](https://img.shields.io/badge/Renderer-OpenGL%204.5-5586A4)](#features--current-status)
[![Editor](https://img.shields.io/badge/Editor-ImGui%20Docking-FF6F61)](#features--current-status)
[![Assets](https://img.shields.io/badge/Assets-OBJ%20%2F%20Meta%20%2F%20Cooked-6A9C89)](#features--current-status)
[![Physics](https://img.shields.io/badge/Physics-Early%20WIP-D97706)](#roadmap)

Hybrid Engine is a long-term implementation playground for understanding how rendering, assets, scene data, editor tooling, input, events, and runtime systems should be structured and how they evolve together.

Current development is mainly focused on:

- editor-driven scene workflow
- asset import, metadata, and cooked-cache pipeline
- render architecture cleanup and pass evolution
- play-mode transitions, scene switching, and early physics iteration

## Status Snapshot

| Platform | Toolchain |
| --- | --- |
| Windows 10 / 11 | Visual Studio 2022 + MSVC + CMake 3.20+ |

| Rendering | Main Entry |
| --- | --- |
| OpenGL 4.5 | `bin\HybridEditor.exe` |

| Current Focus | Dev Log |
| --- | --- |
| Editor, asset pipeline, render architecture, scene workflow | `docs/CHANGELOG.md` |

## Requirements

Current primary development environment:

- Windows 10 or Windows 11
- CMake 3.20 or later
- A compiler with C++17 support
- Visual Studio 2022 / MSVC is the recommended toolchain
- A GPU and driver environment capable of running OpenGL 4.5

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

- The build copies the editor executable, shaders, and editor resources into `bin/`.
- On first launch, the engine automatically initializes `bin/GameProject/` and creates a default `GameProject.hyproj` if it does not already exist.
- Runtime logs are written to `bin/hybrid_engine.log`.

## Documentation

Project documentation lives under `docs/`:

- [Changelog](docs/CHANGELOG.md): development history and milestone notes
- [Development Plan](docs/plans/PROJECT_PLAN_ENG.md): current plan and longer-term structure
- [Render System](docs/systems/render_system.md): current render-system architecture and pass layout
- [Resource System](docs/systems/resource_system.md): runtime/editor asset pipeline, registry, cache, and loading flow
- [Render ECS Asset Chain](docs/systems/render_ecs_asset_chain.md): integration notes for mesh, material, ECS, and editor flow inside the render path
- [Event System](docs/systems/event_system.md): event dispatch and input/event bridge notes
- [Log System](docs/systems/log_system.md): logging structure and usage

## Features & Current Status

Already implemented or broadly usable:

- Core runtime foundation: logging, events, input, window system, and main loop
- Rendering baseline: OpenGL backend, framebuffer path, forward rendering, and Scene / Game viewport split
- Scene system: EnTT-based entities and components, scene serialization, hierarchy data, and the `scene_document` flow
- Editor basics: docking UI, Hierarchy, Inspector, Scene View, Game View, Project Panel, gizmo interaction, and object picking
- Asset-system baseline: VFS logical paths, asset registry, `.meta` files, cooked cache output, and loading for textures, meshes, materials, and scenes
- Import workflow: OBJ import, material generation, texture import, drag-and-drop scene placement, and initial hot-reload support
- Lighting: directional lights and point lights in the current render path
- Physics baseline: AABB collision, collider registration, rigidbody iteration, and collider gizmo debugging

Still under active development:

- The render-pipeline split is still being refined and higher-level pass orchestration is not fully settled yet.
- Shadow, outline, and post-process paths have started but are not complete yet.
- The physics system is still in an early iteration phase and remains unstable overall.
- The editor workflow is already useful for experiments and feature validation, but polish, reliability, and tooling depth still need work.
- Audio and scripting systems have not been implemented yet.

## Roadmap

Near-term focus:

- continue refining render-pipeline structure, including pass execution flow, shader management, and data upload paths
- improve physics behavior, rigidbody flow, and editor-side debugging support
- strengthen scene-editing reliability across edit/play transitions
- keep improving asset hot reload, cache invalidation, and import stability

Planned future work:

- more complete rendering features such as shadows, outline, and post-process
- a more polished editor workflow and better panel-level usability
- broader coverage for default resources and failure paths in the asset system
- audio-system exploration
- scripting support and runtime extensibility after the core architecture becomes more stable

For the broader plan, see [docs/plans/PROJECT_PLAN_ENG.md](docs/plans/PROJECT_PLAN_ENG.md).

## Closing Notes

Hybrid Engine is still a long-term personal learning and research project. If you are also interested in engine architecture, editor tooling, or asset workflows, feel free to reach out.

Special thanks to [@SaluteBEE](https://github.com/SaluteBEE) and [@zong4](https://github.com/zong4) for support, collaboration, and discussion.
Contact:

- Portfolio: [Portfolio](https://the-lyricis.github.io/)
- Email: pigchick1623@gmail.com
