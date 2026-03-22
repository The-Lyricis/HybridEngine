# Render System

Updated: 2026-03-21
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/editor`

## Purpose

This document describes the current render-system structure, pass naming, runtime/editor integration points, and the current platform/backend scope.

Detailed notes about the Mesh / Material / ECS / editor data path are tracked in:

- `docs/systems/render_ecs_asset_chain.md`

## Current Structure

The render system is organized around packet extraction plus pass dispatch.

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

The builtin shader registrations follow pass or responsibility naming.

Current builtin shader names are:

- `Scene`
- `ColliderDebug`
- `SelectionMask`
- `SelectionOverlay`

## Centralized Render Protocols

The render runtime now keeps long-lived shared protocols in dedicated headers instead of scattering them through passes or `RenderSystem` internals.

Current protocol headers are:

- `render_uniforms.h`
  - Frame and Light UBO block names
  - UBO binding indices
  - `FrameUBOData` and `LightUBOData` layouts
- `render_bindings.h`
  - Scene material texture slots
  - Scene material uniform names
  - Selection overlay sampler slots and sampler uniform names
- `render_targets.h`
  - framebuffer attachment semantics such as scene color, scene EntityID, and selection mask attachment indices
- `render_shaders.h`
  - builtin shader registration names and shader file mapping
- `selection_overlay_style.h`
  - selection overlay style data such as visible color, occluded color, fill color, and depth epsilon

Rule:

- only cross-module, long-lived render contracts belong in these headers
- pass-local constants or temporary debug values should stay local

## Frame and Light Upload Path

The scene shader now uses two global UBOs:

- `FrameBlock`
- `LightBlock`

The UBO protocol is centralized in:

- `render_uniforms.h`

Current responsibility split:

- `RenderSystem` owns the UBO objects and updates them before pipeline execution
- `ScenePass`, `SelectionMaskPass`, and `GizmoPass` consume `FrameBlock`
- `ScenePass` consumes `LightBlock`
- per-draw data still uses regular uniforms:
  - `u_Model`
  - `u_TintColor`
  - `u_EntityID`

This keeps frame-level and light-level data centralized while leaving per-draw and per-material data unchanged.

## Platform and Backend Scope

The current scope is intentionally staged.

### Phase 1

Current target:

- runtime cross-platform
- render backend remains OpenGL-only

Meaning:

- runtime systems should avoid being Windows-specific where possible
- the render backend is still allowed to assume OpenGL as the only supported graphics API

### Phase 2

Next platform goal:

- isolate editor-only Windows platform services behind explicit platform interfaces

Examples:

- file dialog services
- show-in-explorer / open-directory services
- platform icon or shell integration services

### Phase 3

Render-backend preparation goal:

- move direct OpenGL calls out of runtime pass code and back into backend or render-command abstractions

Current status:

- `runtime/modules/render/runtime` no longer contains direct `gl*` calls
- draw-buffer control, attachment clear/readback/copy, state toggles, and selection-overlay attachment binding now go through backend or render abstractions

This is the real prerequisite for any future second graphics backend.

Important rule:

- new runtime render work should avoid adding fresh OpenGL calls directly into pass code unless strictly necessary

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

This preserves the intended editor behavior:

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

- prepare / sort layering is still thin
- shader-binding registration is now centralized, but `configureShaderBindings()` should not grow into a large catch-all registration function
- the asset texture upload path still contains an OpenGL-specific texture loader and remains a known backend-coupling point
- runtime still exposes framebuffer renderer IDs for some editor-facing usage, and that boundary should not spread back into runtime pass logic

## Future Improvement Plan

I plan to continue with these steps:

1. I will keep the runtime target OpenGL-only while continuing to remove avoidable platform-specific assumptions.
2. I will keep the new render protocols stable and avoid turning the protocol headers into general constant dumps.
3. I will isolate editor platform services behind interfaces before attempting broader editor portability.
4. I will keep backend-facing work focused on remaining coupling points such as asset-side GPU upload and renderer-ID leakage, rather than reopening runtime pass GL usage.

## Related Documents

- `docs/systems/render_ecs_asset_chain.md`
- `docs/plans/Render_ECS_Asset.md`
