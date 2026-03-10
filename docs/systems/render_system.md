# Render System

Updated: 2026-03-10
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/editor`

## Purpose

This document describes the current render-system overview, its main responsibilities, and how it connects to the editor and scene systems.

More detailed notes about the Mesh / Material / ECS / editor path are tracked in:

- `docs/systems/render_ecs_asset_chain.md`

## Current Structure

The render system no longer uses one large frame function that handles everything.
It now follows a clearer split between data extraction and pass execution.

Current call chain:

- `renderFrame(...)`
- `buildRenderPacket(...)`
- `executePasses(...)`
- `executeForwardPass(...)`

## Core Responsibilities

### 1. `renderFrame(...)`

Frame-level entry point that is responsible for:

- initialization guard
- framebuffer size sync
- render-packet build
- pass execution

### 2. `buildRenderPacket(...)`

CPU-side extraction stage that is responsible for:

- collecting camera data
- collecting light data
- collecting draw items
- keeping ECS extraction separate from direct GPU submission

### 3. `executePasses(...)`

Pass dispatch stage that is responsible for selecting passes by `RenderFlags`.

The current main path is still forward rendering, while these pass points stay open for later growth:

- picking
- gizmo
- outline
- shadow
- post process

### 4. `executeForwardPass(...)`

GPU submission stage that is responsible for:

- binding framebuffers
- binding shaders and resources
- resolving mesh/material GPU data
- submitting draw calls

## Current Implementation State

The current mainline already includes:

- render-flow split
- Mesh / Material asset-path integration
- GPU-side caches inside `RenderSystem`
- Scene / Game dual viewport support
- picking attachment path
- Play / Edit mode scene semantics

## Picking Rules

The current picking path uses a dedicated EntityID attachment.

Rules:

- GPU background value is `0`
- entity write value is `rawEntityID + 1`
- CPU readback decodes:
  - `0` -> invalid
  - non-zero -> `value - 1`

This prevents the semantic conflict between framebuffer background `0` and valid `entt` entity `0`.

## Play-Mode Lifecycle Rules

The current rules are:

- scene switches during Play must exit Play first
- shutdown exits Play first if Play is still active
- Stop must immediately sync editor context so UI does not keep touching a destroyed runtime scene

## Pitfalls and Notes

### 1. Extraction and submission should not collapse back into one function

If `renderFrame(...)` again starts to own:

- ECS traversal
- resource resolution
- state binding
- draw-call submission

then future features will become harder to maintain.

The current split should stay.

### 2. Picking invalid state must stay separate from entity ids

This has already caused a real bug in the project.

Rule that should stay:

- `0` must not mean both "background/no hit" and "valid entity 0"

## Current Open Issues

- frame/light data is still uploaded through per-uniform updates
- prepare / sort layering is still thin
- outline / shadow / post-process are still pending work

## Future Improvement Plan

I plan to continue with these steps:

1. I will move frame data and light data to UBOs first.
2. I will continue to strengthen the prepare / sort / execute boundaries.
3. After the UBO step is stable, I will continue with outline, shadow, post-process, and only then decide whether a higher-level pipeline abstraction is needed.

## Related Documents

- `docs/systems/render_ecs_asset_chain.md`
- `docs/plans/Render_ECS_Asset.md`
