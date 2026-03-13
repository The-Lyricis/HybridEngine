# Hybrid Engine Changelog


### v0.0.6 Current

- Rendering work continued toward a clearer pass / pipeline split to support later outline, shadow, and post-process expansion.
- Initial `ShaderLibrary` work was added to centralize shader management.
- The editor gained a new scene creation flow and a hierarchy entry for point light creation.
- Physics and rigidbody logic were still under active iteration, with multiple same-day updates in `physics_system.cpp` and related components.
- System update log system, DEBUG level log update, add log specifications, unify log system.

### v0.0.5

Date: 2026-03-10

- Added the first physics-system and Play button flow, then expanded it with AABB collision, collider registration, and collider gizmos.
- Introduced `scene_document` to separate editor and runtime scene handling, and moved Play mode onto that document-driven flow.
- Completed scene save/load pipeline work and fixed multiple Play mode exit and selection issues.
- Split the editor into separate Scene and Game viewports and substantially rebuilt the project and hierarchy panel workflows.
- Improved scene serialization with root serialization support and hierarchy deserialization-order fixes.
- Added editor-side asset hot reload and support for dragging OBJ assets directly into the scene.
- Updated the system docs for render, resource, and render/ECS/asset integration on 2026-03-10.

### v0.0.4

Date: 2026-02-28

- Pushed the asset pipeline forward with cooked texture output, auto-generated `.meta` files, and an event-driven import path with a clearer editor/runtime split.
- Upgraded transforms to quaternion rotation and added parent-child hierarchy data for a stable scene tree.
- Expanded editor interaction with drag and drop, hierarchy tree editing, and select / scale / rotate tool support.
- Reorganized editor and runtime source layout into cleaner module boundaries.
- Integrated `tinyobjloader` and completed the OBJ import, material loading, runtime rendering, and scene resource serialization chain.
- Added project-panel scene switching so imported scene assets became editable and runnable content.

### v0.0.3

Date: 2026-02-20

- Built the first real resource-system baseline with `RuntimeResourceSystem`, asset metadata indexing, VFS path mapping, and the final `alias:relative` logical-path rule.
- Moved rendering closer to an asset-driven path with texture loading, mesh/material asset flow, default material fallback, and a cleaner render-system structure.
- Added basic light components and a directional-light test scene.
- Split the editor UI into dedicated panels and connected hierarchy and inspector views to the scene system.
- Added item picking, stronger editor-camera controls, and ImGuizmo axis dragging.
- Added a multi-platform CMake workflow and several Windows build script updates.

### v0.0.2

Date: 2026-02-13

- Added the logging system with `spdlog` core/client channels and macro helpers.
- Added the event system with `Event`, `Dispatcher`, application/input event types, and reverse Layer/Overlay dispatch.
- Added the first complete input path with `InputLayer` sampling and `LayerStack` dispatch for keyboard, mouse, scroll, and char input.
- Established the window system and main loop using GLFW and GLAD.
- Added the first framebuffer-based render test and basic `EditorCamera` movement, drag, and zoom controls.

### v0.0.1

Date: 2026-01-28

- Initial project scaffold.
- Basic CMake setup, third-party dependencies, and early engine structure only.

## Sources Used

This reconstruction is based on:

- `git log` from 2026-02-14 through 2026-03-11
- [render_system.md](/f:/Project/Engine/TDA572/docs/systems/render_system.md)
- [resource_system.md](/f:/Project/Engine/TDA572/docs/systems/resource_system.md)
- [render_ecs_asset_chain.md](/f:/Project/Engine/TDA572/docs/systems/render_ecs_asset_chain.md)
- [Readme.md](/f:/Project/Engine/TDA572/Readme.md)
