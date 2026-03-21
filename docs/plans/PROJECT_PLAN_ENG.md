# Hybrid Engine Development Plan

## Technology Choices

| Component | Choice | Notes |
| --- | --- | --- |
| Build System | CMake 3.20+ | Cross-platform, extensible |
| Graphics API | OpenGL 4.5+ | Current backend target for the runtime stage |
| Window System | GLFW | Lightweight, stable |
| Math Library | GLM | GLSL compatible |
| UI Framework | ImGui | Best practice for editor UI |
| Logging | spdlog | High performance, easy to extend |
| Serialization | JSON (nlohmann_json) | Easy to use, mature ecosystem |
| Scripting | <mark>TBD</mark> | <mark>TBD</mark> |

## Engine Structure Design

```text
Hybrid Engine
|
+- Runtime
|  +- Core
|  |  +- Log System
|  |  +- Event System
|  |  +- Time / Frame
|  |  \- Config / Serialization
|  +- Function
|  |  +- Window & Context
|  |  +- Input System
|  |  +- Render System
|  |  +- Scene System
|  |  +- Physics System
|  |  \- Script System
|  +- Platform
|  |  \- Platform Services / Platform Abstraction
|  \- Resource
|     +- Asset Loader
|     \- Serializer
|
\- Editor
   +- ImGui UI
   +- Scene Editor
   +- Inspector
   \- Asset Browser
```

## Architecture Constraints

These constraints are cross-system rules, not render-only rules.

### 1. Dependency Direction

The intended dependency direction is:

- platform / window
- runtime core
- scene / asset / input
- render abstraction
- render backend

Important rules:

- runtime must not depend on editor
- scene must not own GPU resources
- asset must not depend back on the render backend
- render passes should not depend directly on platform window implementations

### 2. Lifecycle and Ownership

The engine should make ownership explicit for:

- Engine
- Window / GraphicsContext
- RenderSystem / RenderPipeline
- Scene
- AssetManager
- GPU resources such as Framebuffer / Shader / Texture / Buffer
- EditorLayer / RuntimeLayer

The goal is to avoid:

- scattered resource creation
- implicit initialization order
- fragile destruction order
- stale resources after reload or mode switches

### 3. Data Layering

The engine should keep these data layers separate:

- CPU gameplay/editor data
  - Scene / ECS components
  - asset metadata
  - editor state
- render-extracted data
  - FrameContext
  - RenderPacket
  - frame/light/draw-item data
- GPU resource data
  - MeshGPU
  - MaterialGPU
  - UBOs
  - framebuffer attachments

Important rule:

- ECS and scene data should not carry raw GPU handles

### 4. Platform and Backend Strategy

The current staged strategy is:

#### Phase 1

- runtime cross-platform
- render backend remains OpenGL-only

#### Phase 2

- isolate editor Windows-specific services behind platform interfaces

Examples:

- file dialogs
- show-in-explorer / open-directory
- shell / icon integration

#### Phase 3

- move direct OpenGL usage out of runtime pass code and back into backend abstractions

This is the real prerequisite for any future second graphics backend.

### 5. Prepare / Execute Separation

The engine should keep preparing render data separate from executing GPU work.

Prepare should contain work such as:

- scene extraction
- culling
- packet build
- UBO update
- resource readiness checks

Execute should contain work such as:

- pass dispatch
- framebuffer / shader / state binding
- draw / dispatch / copy / readback

### 6. Protocols and Conventions

The engine should gradually centralize shared render/resource protocols such as:

- vertex layout conventions
- material parameter semantics
- texture slot conventions
- default resource rules
- shader file naming and organization
- framebuffer attachment semantics
- EntityID / SelectionMask conventions
- UBO block names and binding rules

### 7. Debugging and Diagnostics

The engine should keep improving diagnostics such as:

- system-specific logging categories
- GPU resource creation / destruction logging
- render statistics
- pass timing visibility
- framebuffer/attachment inspection tools
- shader compilation diagnostics
- editor debug toggles for picking / selection / gizmo paths

### 8. Minimal Shared Abstraction

The engine should not overbuild abstractions too early.

Rule:

- only abstract capabilities that are already recurring and clearly vary across platform/backend boundaries

The goal is to avoid building a large speculative RHI or platform layer too early.

## Overall Plan

##### Phase 1 (3 weeks)

- Phase 1.1 (Foundation): logging / events / window / ImGui / engine main loop
- Phase 1.2 (Initial rendering): Shader / Buffer / Camera / Renderer2D
- Phase 1.3 (Input system): keyboard & mouse input / Action Mapping

##### Phase 2 (2 weeks)

- Phase 2.1 (Resource system)
- Phase 2.2 (Initial scene): ECS / scene serialization / hierarchy editing

##### Phase 3 (2 weeks)

- Phase 3.1 (Physics system): bounding volumes / collision
- Phase 3.2 (Audio system)
- Phase 3.3 (Advanced rendering): shading / texture

##### Phase 4 (After course)

- Phase 4.1 (Scripting system): Mono/C# hot reload / field reflection
- Phase 4.2 (Editor polish): Gizmo / Profiler / asset browser

## Current Medium-Term Focus

The current medium-term focus is:

1. keep the runtime path structurally compatible with cross-platform execution while staying OpenGL-only
2. isolate editor platform services instead of spreading more Windows-specific code
3. reduce direct OpenGL usage in runtime pass code over time
4. continue stabilizing render, asset, and scene boundaries before adding a second graphics backend

## Detailed Plan (consistent with Overall Plan)

### Phase 1.1: Foundation

- [x] CMake build system setup
- [x] Logging system (spdlog)
- [x] Event system (Event + Dispatcher + window events)
- [x] Runtime window system (GLFW + GLAD + OpenGL context)
- [x] ImGui Layer (unified Runtime wrapper)
- [x] Engine main loop (run / update / render / exit)
- Milestone: window display + log output + basic UI

### Phase 1.2: Initial Rendering

- [x] Shader management (compile/link/cache)
- [x] Buffer/VertexArray/Framebuffer
- [ ] Texture loading
- [ ] 2D renderer
- [x] Camera system

### Phase 1.3: Input System

- [x] Keyboard/mouse input
- [x] Input events
- [ ] Action/Axis Mapping

### Phase 2.1: Resource System

- [x] Asset Loader (textures/models/materials)
- [x] Resource path & cache policy
- [ ] Hot reload (optional)

### Phase 2.2: Initial Scene

- [x] ECS basics
- [x] Transform/Renderer Component
- [x] Scene serialization (JSON)
- [x] Scene Hierarchy

### Phase 3.1: Physics System

- [ ] Bounding volumes / collision detection
- [x] Basic rigid body (optional)

### Phase 3.2: Audio System

- [ ] Play/pause/stop API
- [ ] Resource loading & management (optional)

### Phase 3.3: Advanced Rendering

- [x] Basic lighting/materials
- [ ] Texture sampling/management
- [x] RenderPass organization (optional)

### Phase 4.1: Scripting System

- [ ] Mono integration
- [ ] C# assembly loading / hot reload
- [ ] Field reflection & serialization
- [ ] Script lifecycle

### Phase 4.2: Editor Polish

- [ ] Gizmo system
- [x] Inspector panel
- [x] Asset browser
- [x] Profiler panel
