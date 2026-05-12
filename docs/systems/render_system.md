# Render System

Updated: 2026-05-11
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/editor`

## Purpose

This document describes the current render-system structure, pass naming, runtime/editor integration points, current platform/backend scope, and the next modernization direction for the render architecture.

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

- `ShadowPass`
- `ScenePass`
- `SkyboxPass`
- `PickingPass`
- `SelectionMaskPass`
- `SelectionOverlayPass`
- `GridPass`
- `GizmoPass`
- `OverlayGizmoPass`
- `PostProcessPass`

The current code pipeline order is:

- `Shadow`
- `Scene`
- `Skybox`
- `Picking`
- `SelectionMask`
- `SelectionOverlay`
- `Grid`
- `WorldGizmo`
- `OverlayGizmo`
- `PostProcess`

Current notes:

- `PickingPass` is a placeholder because entity picking currently piggybacks on `ScenePass` EntityID output.
- `GridPass` is connected to the pipeline, but its implementation is still a placeholder.
- `PostProcessPass` is connected as a fullscreen pass with tone-mapping and gamma-correction parameters. Both corrections default to disabled, so the current path remains visually passthrough until settings are wired in.
- `OverlayGizmoPass` is reserved for screen-space editor handles and overlays.
- `DebugNormals` is represented in `RenderFlags`, but its pass path is not active in the current pipeline.

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

## Current Frame Flow

The current frame flow is a forward-rendering editor pipeline:

1. `HybridEngine::run()` updates input, layers, scene state, and physics.
2. `RenderSystem::update(dt)` reloads changed shaders on its timer.
3. `RenderSystem::renderFrame(...)` routes the frame to Scene View, Game View, or the default runtime target.
4. `FrameViewResolver` resolves camera, projection, clear color, skybox state, and main directional light.
5. `ShadowFrameBuilder` builds directional-light cascade data when shadows are enabled.
6. `RenderPacketBuilder` extracts lights and draw items from ECS, resolves mesh/material GPU resources, culls visible objects, and sorts draw queues.
7. `RenderSystem` updates `FrameBlock` and `LightBlock` UBOs.
8. `RenderPipeline` executes the enabled passes in the fixed pass order.
9. The editor UI displays the Scene View and Game View color textures.

Scene View uses the editor camera and can run picking, selection highlight, gizmo, and shadow passes. Game View uses the game camera and currently renders only `Scene | Shadow`.

## Render Targets

The main Scene View and Game View framebuffers use:

- color attachment 0: scene color (`RGBA8`)
- color attachment 1: encoded EntityID (`R32UI`)
- depth attachment: scene depth (`Depth32F`)

The selection framebuffer uses:

- color attachment 0: selected-union mask (`R8`)
- depth attachment: selected-object depth (`Depth32F`)

Directional shadow cascades use depth-only framebuffers.

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

- `runtime/modules/render/runtime` should not contain direct `gl*` calls
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
- pass ordering is still fixed and manually maintained by `RenderPipeline`
- pass resource dependencies are implicit rather than declared
- render state is still set procedurally inside passes instead of through pipeline-state objects
- post-processing, grid rendering, overlay gizmos, and debug-normal rendering are not yet complete

## Modernization Direction

The next render-system direction is to move from a fixed sequential pass dispatcher toward a modern, resource-driven architecture while keeping the current OpenGL backend stable.

The target architecture should introduce these concepts gradually:

- Render Graph
  - passes declare their input and output resources
  - graph execution decides ordering from dependencies
  - transient color/depth targets are owned by the graph
  - resize and lifetime management are centralized
- Pipeline State Object
  - shader program, vertex layout, blend state, depth/stencil state, raster state, and render-target formats are grouped as explicit state
  - passes and materials bind stable state objects instead of setting many global states ad hoc
- Resource Binding Model
  - textures, samplers, uniform buffers, storage buffers, and render targets should be described through backend-neutral handles
  - OpenGL texture slots and uniform names should become implementation details behind binding layouts where possible
- Frame Resource System
  - per-frame dynamic uniform/storage data should move toward ring buffers or transient allocators
  - upload and delayed GPU deletion should be centralized
- HDR Post-Process Chain
  - scene color should eventually support HDR formats
  - tone mapping and gamma correction should become the minimum post-process path
  - bloom, FXAA/TAA, and color grading can be layered after the base chain
- Forward+ / Clustered Forward Lighting
  - keep the current forward material path
  - replace simple point-light iteration with tiled or clustered light lists when the renderer outgrows the current UBO light array
- Editor Overlay Split
  - world overlays should cover grid, colliders, lights, camera frustums, and physics/debug lines
  - screen overlays should cover selection outlines, transform handles, icons, and viewport-space editor tools

Important sequencing rule:

- do not begin a second graphics backend before pass dependencies, pipeline state, resource binding, and frame resources are clearer under the current OpenGL backend.

## Future Improvement Plan

Planned refactor order:

1. Align documentation and code naming around the real current pass order and active/inactive pass status.
2. Clean up existing pass boundaries without changing visible behavior:
   - split world overlay and screen overlay responsibilities
   - remove accidental pass coupling such as debug options gating unrelated gizmos
   - keep runtime pass code free of direct OpenGL calls
3. Expand the minimal post-process path:
   - wire tone mapping / gamma correction settings to editor or project configuration
   - prepare HDR scene color support
4. Introduce explicit pipeline-state descriptions for existing passes.
5. Introduce a small Render Graph prototype around the current fixed pass set.
6. Move transient framebuffer creation, resize, and ownership into graph-managed resources.
7. Normalize material and resource bindings around backend-neutral binding layouts.
8. Add frame-resource utilities for dynamic buffers, upload staging, and delayed release.
9. Upgrade lighting toward Forward+ / clustered forward when the resource model is ready.
10. Treat a second backend as a later implementation target after the architecture above is stable.

## Related Documents

- `docs/systems/render_ecs_asset_chain.md`
- `docs/plans/Render_ECS_Asset.md`
