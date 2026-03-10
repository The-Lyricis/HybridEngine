# Resource System

Updated: 2026-03-10
Scope: `TDA572/engine/source/runtime/modules/asset`, `TDA572/engine/source/editor/services/asset`

## Purpose

This document describes the current resource-system structure, the responsibility split between runtime and editor layers, and the current rules for import, registration, loading, caching, and default resources.

## Current Structure

The current resource system is split into two layers:

- runtime resource layer: `RuntimeResourceSystem`
- editor resource layer: `EditorResourceSystem`

They share the same core asset infrastructure:

- `IVirtualFileSystem`
- `AssetRegistry`
- `AssetMetaStore`
- `AssetManager`
- asset loaders

## Core Responsibilities

### 1. `IVirtualFileSystem`

Responsible for logical-path to physical-path mapping.

Current rule:

- logical paths such as `asset:Scenes/Demo.scene` should be used
- mount points resolve them to real filesystem locations

### 2. `AssetRegistry`

Responsible for asset metadata indexing.

Main responsibilities:

- maintain `AssetID -> AssetMetadata`
- maintain logical-path to `AssetID` mapping
- generate new unique `AssetID`
- provide a common lookup point for SceneSerializer, importers, and loaders

### 3. `AssetMetaStore`

Responsible for `.meta` file IO.

Main responsibilities:

- scan and load existing `.meta` files at startup
- save new asset metadata
- remove stale metadata files
- validate logical-path fields such as `source_path` and `cooked_path`

### 4. `AssetManager`

Responsible for runtime load state, caching, and default-resource fallback.

Main responsibilities:

- `loadSync<T>(id)`
- `loadAsync<T>(id)`
- loaded-object cache
- in-flight state management
- fallback to default resources on load failure
- explicit `unload(id)` support

### 5. `Loader`

Each asset type enters runtime through a corresponding loader.

Current key loaders include:

- `GLTexture2DLoader`
- `MeshCookedLoader`
- `MaterialFileLoader`
- `SceneLoader`

## Current Implementation State

### 1. RuntimeResourceSystem

Current responsibilities:

- create and configure the VFS
- create `AssetRegistry`
- scan and load `.meta`
- create `AssetManager`
- register default loaders
- create default resources
- register built-in resident resources such as the built-in cube mesh

### 2. EditorResourceSystem

Current responsibilities:

- initialize importers
- scan `Assets/` at startup and fill in missing meta or cooked outputs
- respond to file changes and queue import work
- process import work per frame
- support move, delete, and reimport flows
- clear runtime cache after successful import

Typical call sites include:

- `bootstrapImportOnce()`
- `processImportQueue(...)`

### 3. Default resources

Current default resources include at least:

- default Texture
- default Material
- built-in Mesh

These are used for:

- load-failure fallback
- keeping the system runnable before the full asset path is ready

## Current Load Chain

Typical chain:

1. upper layer obtains an `AssetID`
2. it calls `AssetManager::loadSync<T>(id)`
3. `AssetManager` queries metadata from `AssetRegistry`
4. the matching loader is selected
5. the loader reads the logical-path target through VFS
6. a runtime object is created
7. the object is cached and returned

If loading fails:

- the system falls back to the registered default resource

## Relationship to the Render System

The render system now directly depends on the main resource path:

- `RenderSystem` loads Mesh / Material / Texture through `AssetManager`
- `RenderSystem` manages GPU-side caches itself
- the resource system owns CPU-side asset objects and default resources
- the render system resolves and uploads them to GPU

## Problems Hit and Fixes

### 1. OBJ material sub-assets were incomplete

Problem:

- OBJ import could produce `.mat.meta`
- but not always a complete `.mat` source file
- or the material-to-texture dependency chain was not fully established

Fix:

- OBJ import now writes real `.mat` files
- referenced texture assets are imported too
- texture dependencies are written into the material relationship

### 2. Reimport could keep stale runtime cache alive

Problem:

- editor import completed
- but `AssetManager` still kept old runtime objects cached

Fix:

- `EditorResourceSystem` now calls `unload` for returned asset ids after successful import

### 3. Texture upload orientation did not match the OBJ UV convention

Problem:

- texture data, UVs, and material ids were correct
- but rendering still showed obvious mismatch or black regions

Root cause:

- the OpenGL upload path did not vertically flip image data to match the current OBJ/UV convention

Fix:

- `GLTexture2DLoader` now vertically flips RGBA data before upload

## Current Open Issues

- GPU resource release and cross-thread cleanup are still thin
- more asset types still need more complete default-resource and error-resource handling
- async loading for GPU resources still needs caution
- the hot-reload chain can still be strengthened later

## Future Improvement Plan

I plan to continue with these steps:

1. I will keep improving hot-reload and cache-invalidation behavior.
2. I will continue to define clearer default-resource rules for more asset types.
3. If the project later needs more complex async loading, I will design a dedicated GPU-thread and destruction-queue path for it.
