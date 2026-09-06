# Runtime Architecture

## Target boundaries

The engine is built as a one-way dependency chain:

`HybridCore -> HybridRuntime -> HybridRender -> HybridEngine`

- `HybridCore` owns logging, events, reflection, VFS primitives, and `JobSystem`.
- `HybridRuntime` owns projects, CPU assets, registry/loading, Scene/ECS, input state, and physics.
- `HybridRender` owns windows, the OpenGL context/backend, GPU resources, extraction, RenderGraph, and passes.
- `HybridEngine` owns initialization rollback, shutdown ordering, the frame loop, and the active runtime scene.
- `HybridEditor` is a Windows-only client. Its session controller owns the edit scene, runtime clone, and Play/Pause/Stop state.
- `HybridPlayer` is the cross-platform runtime entry point.

Editor is not a transitive runtime dependency. CPU image assets share a backend-independent pixel format with Render, but Runtime does not create OpenGL objects.

## Lifecycle contract

`HybridEngine::initialize(const EngineConfig&)` is idempotent. Failed initialization calls `shutdown()` to unwind initialized services. Shutdown is also idempotent and follows this order:

1. stop the frame loop and detach layers;
2. close the asset manager to submissions and drain jobs;
3. stop physics;
4. release RenderSystem passes, GPU caches, framebuffers, buffers, shaders, and the renderer API;
5. release runtime asset caches;
6. stop and join `JobSystem` workers;
7. destroy the graphics context, then the window;
8. flush and stop logging.

GPU resources therefore die while the OpenGL context is still valid. `LayerStack` owns layers with `std::unique_ptr`, and input is a service updated exactly once before reverse layer dispatch.

## Frame and render contract

The engine owns one active scene. Fixed simulation uses a configurable accumulator (60 Hz by default), a maximum of four catch-up steps, and drops excess accumulated time with a warning. Variable scene update and rendering execute once per frame.

Rendering receives a `RenderFrameRequest` containing the scene, frame delta, and a list of `RenderViewRequest` values. Each view has a stable `RenderViewId` and declares its size, camera source, flags, optional selection/debug state, and optional picking request. `RenderFrameResult` returns the matching ID, color texture, and optional picking result. `RenderSystem` owns a target pool keyed by view ID, resizes targets on demand, and retires unused targets while the graphics context is alive. Editor submits Scene and Game views; Player submits one Game view.

## Async asset contract

`JobSystem` is a fixed worker pool. It supports futures, exception propagation, `waitIdle()`, and drain-only shutdown. `AssetManager` owns no detached threads and receives the job system explicitly.

Registry lookup returns `AssetMetadata` values in `std::optional`, so worker tasks never retain pointers into registry containers. Concurrent loads of the same asset share one in-flight future. Each asset has a generation ticket; unload/reimport advances it, and an older task may finish but cannot publish its result. File I/O, decoding, and deserialization run on workers. GPU upload remains on the render thread.

Editor imports use the engine-owned `JobSystem`. Workers prepare cooked data and metadata; the editor main thread atomically commits metadata, updates the registry, invalidates runtime/GPU caches, and invokes UI callbacks. A source path has at most one running import. Additional changes are coalesced into a later revision rather than starting a competing job.

## Editor integration contract

`HybridEngine::setExitRequestHandler()` lets Editor intercept both operating-system close requests and `File > Exit`. While confirmation is pending, the GLFW close flag is cleared. Editor owns dirty-document transitions and calls `requestExit()` only after Save or Discard succeeds; Player retains immediate-exit behavior when no handler is installed.

The core log system keeps a bounded, thread-safe in-memory stream in addition to console and file sinks. Editor reads snapshots from this stream, so worker diagnostics can be displayed without calling ImGui outside the main thread.
