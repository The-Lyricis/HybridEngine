# Render ECS Asset Chain

Updated: 2026-03-10
Scope: `TDA572/engine/source/runtime/modules/render/runtime`, `TDA572/engine/source/editor`

## Purpose

This document records the current state of the Mesh / Material / ECS / editor render chain and the concrete implementation issues that were hit while building it.

This is not a plan file.
It is an engineering chain record.

## Current Chain Structure

The current main chain is:

- extract render data from Scene / ECS
- organize frame data in `RenderPacket`
- feed Mesh / Material through `AssetID`
- let `RenderSystem` resolve CPU assets and manage GPU caches
- submit draw calls through the forward pass

## Current Implementation State

### 1. RenderSystem split

`RenderSystem` has already been split into:

- `buildRenderPacket(...)`
- `executePasses(...)`
- `executeForwardPass(...)`

Benefits:

- CPU extraction and GPU submission are separated
- shadow / outline / post-process can grow on top of this structure
- `renderFrame(...)` no longer keeps expanding

### 2. Mesh / Material asset path

The current main path is asset-driven:

- Mesh enters the render path through `AssetID`
- Material enters the render path through `AssetID`
- `RenderSystem` owns `MeshGPUCache` and `MaterialGPUCache`
- the default cube has already been moved onto this asset path
- the old cube fallback path is now compatibility-only logic

### 3. Material and shader path

Current material support:

- albedo
- metallic
- roughness
- ao
- emissive
- normal map

Current shader rules:

- `MeshVertex::tangent` is `vec4`
- `tangent.w` stores handedness
- normal-map sampling includes tangent-length protection
- the default MR texture is neutral
- MR uses multiplicative modulation
- emissive is accumulated independently and is no longer multiplied by albedo

### 4. Lighting path

Current lighting support:

- one directional light
- multiple point lights

Directional-light rule:

- direction is derived from `TransformComponent`
- there is no long-lived duplicate `direction` field in `DirectionalLightComponent`

## Editor and Runtime Integration

### 1. Picking

Scene picking uses a dedicated EntityID attachment.

Rules:

- GPU background value is `0`
- entity write value is `rawEntityID + 1`
- CPU readback decodes `0` as invalid and subtracts `1` from non-zero values
- editor-side invalid state uses `kInvalidEntityID`

### 2. Play / Edit semantics

Current behavior:

- in Edit mode, Scene viewport gizmo / inspector / picking target the editor scene
- in Play mode, these actions target the runtime scene
- on Stop, the editor context switches back to the editor scene immediately

The following rules are also enforced:

- scene changes during Play exit Play first
- shutdown exits Play first
- Stop clears selection and pending-pick state before later UI work touches stale runtime data

## Problems Hit and Fixes

### 1. Valid entity `0` could not be picked

Problem:

- in Edit mode, clicking an object could always read back `0`
- in Play mode, the same object could appear selectable

Root cause:

- the first valid `entt` entity can be `0`
- the picking buffer also used `0` for background / no hit

Fix:

- GPU writes `raw + 1`
- background stays `0`
- CPU decodes on readback

### 2. Helper / gizmo passes could corrupt EntityID

Problem:

- editor-only passes shared the same framebuffer with the forward pass
- helper shaders did not write EntityID
- draw-buffer state could therefore damage the picking attachment

Fix:

- forward pass writes `COLOR0 + COLOR1`
- gizmo / helper passes write `COLOR0` only

### 3. Play-mode editing modified the wrong scene

Problem:

- the Scene viewport looked like it was editing the runtime scene
- but gizmo / inspector were still writing into the editor scene
- the visible result was: no change during Play, but the change appeared after Stop

Fix:

- Play mode switches editor-context `active_scene` to the runtime scene
- Stop switches it back to the editor scene
- Play-mode temporary edits no longer dirty the editor document

### 4. Stop could touch a destroyed runtime scene in the same frame

Problem:

- Stop destroyed the runtime scene
- later UI work in the same frame still touched old scene / entity state
- this caused assertions or crashes

Fix:

- `exit_play_mode` clears selection and pending-pick state immediately
- editor context is synchronized right away
- later UI work in the same frame no longer sees the old runtime scene

## Current Open Issues

- frame/light data still uses per-uniform uploads
- prepare / sort is still thin
- outline / shadow / post-process are still pending work

## Future Improvement Plan

I plan to continue in this order:

1. I will implement Frame UBO + Light UBO first.
2. I will continue to fill in the prepare / sort layer.
3. After those two steps are stable, I will continue with shadow, outline, post-process, and any higher-level pipeline abstraction.
