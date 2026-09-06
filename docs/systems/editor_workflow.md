# Editor Workflow Reliability

## Document transitions and exit

New Scene, Open Scene, `File > Exit`, and the operating-system close button use the same dirty-document transition rules. A clean document transitions immediately. A dirty document offers Save, Discard, and Cancel; a cancelled dialog or failed save cancels the transition. If Play Mode is active, Editor stops the runtime clone before evaluating the editor document.

Project switching remains process-based: Open Project launches another Editor instance. In-process project replacement is outside this milestone.

## Project Settings and Game View

`Edit > Project Settings...` shows the project and resolved Assets, Cache, Build paths. Only `startup_scene` is editable in this milestone, and selection is restricted to registered Scene assets.

Game View supports Free, 16:9, 1280x720, 1920x1080, and Custom modes. Fixed and aspect modes are letterboxed inside the panel. Custom dimensions are clamped to `16..min(8192, GL_MAX_TEXTURE_SIZE)`. The requested render size is independent from the displayed image size and survives editor restart through the versioned Editor state file.

## Import tasks

Bootstrap scans, file-watcher events, and manual reimport enter one scheduler. Preparation runs on the engine worker pool; metadata/registry changes and cache invalidation run on the editor main thread. The status bar reports active work, and the Tasks panel exposes queued/running/completed/failed state, stage, elapsed time, errors, retry, and source-file reveal.

`EditorResourceSystem::shutdown()` stops accepting work, drains submitted jobs, commits completed results on the main thread, and then detaches callbacks. Import workers must not access OpenGL, ImGui, or editor scene state.

## Render views

Scene and Game views use stable IDs and consume textures from `RenderFrameResult`. Frame builders consume `RenderViewRequest` directly; there are no editor-specific runtime render extensions or fixed Scene/Game framebuffer getters.
