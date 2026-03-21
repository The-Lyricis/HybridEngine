# Render ECS Asset Plan

Updated: 2026-03-21
Scope: `TDA572/engine/source/runtime/modules/render`, `TDA572/engine/source/runtime/modules/asset`, `TDA572/engine/source/editor`

## Overall Direction

The render roadmap is now constrained by a staged platform and backend strategy.

The current staged goal is:

### Phase 1

- runtime cross-platform
- render backend stays OpenGL-only

### Phase 2

- isolate editor Windows-specific services behind platform interfaces

### Phase 3

- move direct OpenGL usage out of runtime pass code and back into backend abstractions

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

## Current Priority Order

### Priority 1: Runtime stability and portability under OpenGL

Continue improving runtime rendering while keeping the backend OpenGL-only.

This includes:

- scene rendering correctness
- asset-path stability
- viewport correctness
- UBO path stability
- selection overlay stability

### Priority 2: Editor platform-service isolation

Create platform interfaces for editor-only functionality such as:

- file dialogs
- show-in-explorer or open-directory behavior
- shell or icon integration

### Priority 3: Runtime backend decoupling

Gradually reduce direct OpenGL usage in runtime pass code.

Main targets included:

- draw-buffer control
- texture copy operations
- GPU readback paths
- direct texture binding in passes
- direct state toggles in passes

Current status:

- runtime pass code no longer contains direct `gl*` calls
- the next backend-decoupling work should focus on remaining coupling points outside runtime pass code

## Execution Order

I plan to continue in this order:

1. I will keep stabilizing the current runtime render path under OpenGL.
2. I will isolate editor platform services before claiming broader editor portability.
3. I will gradually move runtime pass GL calls behind backend abstractions.
4. Only after those steps are in place will I treat a second graphics backend as an active implementation goal.

## Short-Term Follow-Up Work

The most reasonable next steps are:

1. continue prepare / sort cleanup in the render pipeline
2. keep the centralized render protocols stable and avoid turning them into generic constant dumps
3. document the current runtime/backend boundary clearly so future work does not reintroduce pass-level GL coupling
4. identify the next highest-value backend coupling points outside runtime pass code, especially asset-side GPU upload

## Notes

This file is a live planning document.
It should stay aligned with `docs/systems/render_system.md`.
