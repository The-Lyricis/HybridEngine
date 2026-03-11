# Hybrid Engine Development Plan

## Technology Choices

| Component | Choice | Notes |
| --- | --- | --- |
| Build System | CMake 3.20+ | Cross‑platform, extensible |
| Graphics API | OpenGL 4.5+ | Low learning cost, mature toolchain |
| Window System | GLFW | Lightweight, stable |
| Math Library | GLM | GLSL compatible |
| UI Framework | ImGui | Best practice for editor UI |
| Logging | spdlog | High performance, easy to extend |
| Serialization | JSON (nlohmann_json) | Easy to use, mature ecosystem |
| Scripting | <mark>TBD</mark> | <mark>TBD</mark> |

## Engine Structure Design

```
Hybrid Engine
│
├─ Runtime
│  ├─ Core
│  │  ├─ Log System
│  │  ├─ Event System
│  │  ├─ Time / Frame
│  │  └─ Config / Serialization
│  ├─ Function
│  │  ├─ Window & Context (GLFW + OpenGL)
│  │  ├─ Input System
│  │  ├─ Render System (Shader/Buffer/Texture/Camera)
│  │  ├─ Scene System (Entity/Component)
│  │  ├─ Physics System ()
│  │  └─ Script System (C++/Lua)
│  ├─ Platform
│  │  └─ Platform Abstraction
│  └─ Resource
│     ├─ Asset Loader
│     └─ Serializer
│
└─ Editor
   ├─ ImGui UI
   ├─ Scene Editor
   ├─ Inspector
   └─ Asset Browser
```

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
