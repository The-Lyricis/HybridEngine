# Render ECS Asset Plan

Updated: 2026-05-11
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/runtime/modules/asset`, `TDA572/engine/source/editor`

## Overall Direction

The render roadmap is now constrained by a staged platform and backend strategy, with the next major direction being a gradual move toward a modern render architecture.

The current staged goal is:

### Phase 1

- runtime cross-platform
- render backend stays OpenGL-only

### Phase 2

- isolate editor Windows-specific services behind platform interfaces

### Phase 3

- move direct OpenGL usage out of runtime pass code and back into backend abstractions

### Phase 4

- introduce explicit pipeline-state descriptions
- introduce a small Render Graph around the current pass set
- move transient render-target ownership and resize handling into graph-managed resources

### Phase 5

- formalize backend-neutral resource bindings
- add frame-resource utilities for dynamic GPU data
- upgrade the lighting path toward Forward+ / clustered forward

This means the near-term render roadmap should optimize for architecture that remains compatible with a future second backend, but it should not attempt to implement that second backend yet.

## Current Baseline

The current baseline already includes:

- asset-driven mesh/material render path
- render packet extraction
- pass-based pipeline execution
- scene/game viewport split
- picking attachment path
- selection highlight path based on projected selected-union mask
- Frame UBO and Light UBO for the scene shader, with shared protocol definitions moved into dedicated runtime headers

The current code pass order is:

- `Shadow`
- `Scene`
- `Skybox`
- `Picking`
- `SelectionMask`
- `SelectionOverlay`
- `WorldGizmo`
- `OverlayGizmo`
- `PostProcess`

Current inactive or placeholder paths:

- `PickingPass` piggybacks on `ScenePass` EntityID output
- `OverlayGizmoPass` is reserved for screen-space editor overlays
- `PostProcessPass` is reserved for the future post-process chain
- `Grid` and `DebugNormals` exist in flags but do not have active pass paths

## Planning Constraints

The following rules should guide future work.

### 1. Runtime render work should avoid new Windows-only assumptions

Runtime code should remain portable where possible.

### 2. Render feature work should not deepen OpenGL coupling inside pass code

If a new feature requires OpenGL-only work, prefer putting that work behind:

- render backend classes
- render command abstractions
- framebuffer/texture abstractions

rather than placing more raw OpenGL calls directly into pass code.

### 3. Editor portability is a separate stage

Editor functionality may still be Windows-first for now, but platform-specific editor capabilities should gradually move behind platform interfaces.

### 4. Modern render architecture should be introduced behind stable behavior

The next render refactors should preserve existing visible behavior while adding clearer architecture underneath.

New concepts should land in this order:

- explicit pass boundaries
- pipeline-state descriptions
- graph-owned transient render targets
- declared pass inputs and outputs
- backend-neutral resource binding layouts
- frame-resource upload and lifetime utilities

### 5. A second graphics backend is not the immediate goal

The OpenGL backend should remain the implementation target while the architecture is cleaned up.

Do not start Vulkan, D3D12, or Metal implementation work until:

- runtime passes are free of direct backend calls
- render states are represented explicitly
- pass dependencies are declared
- resource bindings are not tied to OpenGL texture-slot assumptions at the runtime level

## Current Priority Order

### Priority 1: Runtime stability and portability under OpenGL

Continue improving runtime rendering while keeping the backend OpenGL-only.

This includes:

- scene rendering correctness
- asset-path stability
- viewport correctness
- UBO path stability
- selection overlay stability

### Priority 2: Pass boundary cleanup

Clean up the current pass set before deeper architecture work.

This includes:

- align documentation with the real pass order
- split world overlays from screen overlays
- make `Grid`, `OverlayGizmo`, `PostProcess`, and `DebugNormals` status explicit
- remove accidental coupling between debug feature flags
- keep editor-only rendering out of Game View

### Priority 3: Minimal post-process foundation

Add the first real post-process infrastructure.

This includes:

- full-screen pass helper reuse
- explicit source and destination targets
- tone mapping / gamma correction placeholder
- later HDR scene color support

### Priority 4: Runtime backend decoupling

Gradually reduce direct OpenGL usage and OpenGL-shaped assumptions in runtime render code.

Main targets include:

- asset-side GPU upload
- framebuffer renderer-ID leakage
- texture binding slots exposed above the backend layer
- GPU readback paths
- state toggles that should become pipeline-state descriptions

### Priority 5: Render Graph prototype

Start with the current fixed pass set and add graph declarations without changing visual output.

Each pass should gradually declare:

- read resources
- write resources
- clear/load/store intent
- viewport size dependency
- whether it is editor-only or game-view compatible

### Priority 6: Pipeline State Object and binding model

Move procedural state setup into explicit descriptions:

- shader
- vertex layout
- blend state
- depth/stencil state
- raster state
- render-target formats
- binding layout

### Priority 7: Frame resources and Forward+

After the graph and binding model are stable, add:

- dynamic uniform/storage ring buffers
- upload queue
- delayed deletion queue
- GPU timing markers
- clustered or tiled light lists for Forward+ lighting

## Execution Order

I plan to continue in this order:

1. Stabilize and document the current OpenGL render path.
2. Clean up pass boundaries and inactive pass status.
3. Add the minimal post-process foundation.
4. Add explicit pipeline-state descriptions for existing passes.
5. Add a small Render Graph around the current pass set.
6. Move transient render-target ownership into graph resources.
7. Normalize resource bindings and material inputs.
8. Add frame-resource utilities.
9. Upgrade lighting toward Forward+.
10. Treat a second backend as a later goal.

## Short-Term Follow-Up Work

The most reasonable next steps are:

1. update docs to match the current code pass order
2. split overlay responsibilities into world overlay and screen overlay language
3. implement or formalize the empty `PostProcessPass` path
4. define the first `PipelineStateDesc` shape without forcing all passes to migrate at once
5. define the first `RenderGraph` resource/pass descriptor shape
6. identify remaining OpenGL-shaped assumptions outside runtime passes, especially asset-side GPU upload and renderer-ID exposure

## Notes

This file is a live planning document.
It should stay aligned with `docs/systems/render_system.md`.
