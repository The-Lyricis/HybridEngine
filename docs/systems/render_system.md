# Render System

Updated: 2026-03-19
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/editor`

## Purpose

This document describes the current render-system structure, pass naming, and the main runtime/editor integration points.

Detailed notes about the Mesh / Material / ECS / editor data path are tracked in:

- `docs/systems/render_ecs_asset_chain.md`

## Current Structure

The render system is now organized around packet extraction plus pass dispatch.

Current high-level flow:

- `renderFrame(...)`
- `buildRenderPacket(...)`
- `RenderPipeline::execute(...)`
- pass execution through `RenderContext`

## Current Pass Set

The current pass naming is:

- `ScenePass`
- `PickingPass`
- `SelectionMaskPass`
- `SelectionOverlayPass`
- `GizmoPass`
- `ShadowPass`
- `PostProcessPass`

The current pipeline order is:

- `Scene`
- `Picking`
- `SelectionMask`
- `SelectionOverlay`
- `Gizmo`
- `Shadow`
- `PostProcess`

## Core Responsibilities

### 1. `renderFrame(...)`

Frame-level entry point responsible for:

- initialization guard
- render-target sizing
- scene/game viewport routing
- packet build and pipeline dispatch

### 2. `buildRenderPacket(...)`

CPU-side extraction stage responsible for:

- collecting camera data
- collecting light data
- collecting draw items
- keeping ECS traversal separate from GPU submission

### 3. `RenderPipeline::execute(...)`

Pass dispatch stage responsible for:

- selecting passes from `RenderFlags`
- preserving pass order
- keeping pass execution independent from frame extraction

### 4. Pass execution through `RenderContext`

Each pass consumes explicit context data such as:

- `framebuffer`
- `scene_framebuffer`
- `selection_framebuffer`
- `scene_shader`
- `collider_debug_shader`
- `editor_selection`

This keeps pass inputs explicit instead of pulling state directly from `RenderSystem` internals.

## Shader Naming

The builtin shader registrations now follow pass or responsibility naming.

Current builtin shader names are:

- `Scene`
- `ColliderDebug`
- `SelectionMask`
- `SelectionOverlay`

This replaces older names that mixed material, debug, and pass semantics.

## Picking Rules

The picking path still uses a dedicated EntityID attachment.

Rules:

- GPU background value is `0`
- entity write value is `rawEntityID + 1`
- CPU readback decodes:
  - `0` -> invalid
  - non-zero -> `value - 1`

This keeps framebuffer background state separate from valid `entt` entity `0`.

## Selection Highlight Rules

The current selection highlight system is editor-specific.

The active selection flow is:

- `SelectionMaskPass` renders the selected set into a projected union mask
- `SelectionOverlayPass` extracts the outline from that projected mask
- depth is used only to classify visible vs occluded style

Important rule:

- contour source comes from the projected selected-union mask
- `SceneDepth` and `SelectedDepth` do not define the contour source
- depth only controls visible vs occluded styling

This was chosen to preserve the intended editor behavior:

- multiple selected objects contribute to one union outline
- internal seams between selected objects do not produce outlines
- internal crossing edges caused by non-selected objects do not cut the contour source

## Play-Mode Lifecycle Rules

The current rules are:

- scene switches during Play exit Play first
- shutdown exits Play first if Play is still active
- Stop immediately synchronizes editor context so UI does not touch a destroyed runtime scene
- Play-mode editing targets the runtime scene, not the editor scene

## Current Open Issues

- frame/light data still uses per-uniform uploads instead of UBOs
- prepare / sort layering is still thin
- pass documentation is now cleaner than implementation layering; the next work should focus on data preparation, not more renaming

## Future Improvement Plan

I plan to continue with these steps:

1. I will move frame and light data to UBOs first.
2. I will continue to strengthen prepare / sort / execute boundaries.
3. After the data-upload path is stable, I will extend the editor overlay path further instead of mixing more work back into the scene pass.

## Related Documents

- `docs/systems/render_ecs_asset_chain.md`
- `docs/plans/Render_ECS_Asset.md`
