# Render System 文档

更新时间：2026-02-13  
适用范围：`TDA572/engine/source/runtime/function/render` 及 `render/opengl`。

## 目录与依赖
- 头文件入口：`render/*`（抽象层），`render/opengl/*`（OpenGL 实现）。
- 依赖库：glad、GLFW、glm、spdlog、imgui（只在上层 UI 用到）。
- 注意：窗口与输入桥接类 `SurfaceIO` 已移至 `runtime/function/window`，不再属于渲染系统。

## 核心类职责
- `RendererAPI`：渲染后端接口，当前实现为 OpenGL。
- `RenderCommand`：静态包装，转发到活跃的 `RendererAPI`。
- `Renderer`：帧级高层入口，组织 begin/submit/end。
- `RenderSystem`：场景级拥有者，管理资源并驱动每帧渲染（在 Editor 内部调用）。
- `GraphicsContext`：图形上下文抽象；`OpenGLContext` 负责 make current、加载 glad、swap。
- 资源抽象：`VertexBuffer` / `IndexBuffer` / `VertexArray` / `Shader` / `Framebuffer`。
- OpenGL 实现：`OpenGLVertexBuffer`、`OpenGLIndexBuffer`、`OpenGLVertexArray`、`OpenGLShader`、`OpenGLFramebuffer`、`OpenGLRendererAPI`。
- 摄像机：`EditorCamera` 供编辑器视口自由飞行控制。

## 初始化与生命周期
1) 窗口创建：`WindowSystem::initialize`（GLFW）。  
2) 图形上下文：`GraphicsContext::Create(window)` -> `OpenGLContext::init()`  
   - 顺序要求：先 `#include <glad/gl.h>`，再定义 `GLFW_INCLUDE_NONE` 后包含 `GLFW/glfw3.h`，避免重复 `gl.h`。  
3) 渲染命令层：`RenderCommand::initialize()` 内部实例化 `RendererAPI`。  
4) 场景资源：`RenderSystem::initialize(void* glfwWindowHandle)` 构建默认帧缓冲、VAO/VBO/IBO、着色器。  
5) 每帧调用：  
   - 输入/逻辑更新  
   - `Renderer::beginFrame(clearColor)`  
   - `RenderSystem::renderFrame(viewportSize, window, dt, viewportActive, input)`  
   - `Renderer::endFrame()`  
   - `GraphicsContext::swapBuffers()`  

## 渲染流程（默认）
1) 确认视口尺寸 -> 若变更则 `Framebuffer::resize`。  
2) 设置视口、清屏色（`RenderCommand` -> `RendererAPI`）。  
3) 更新 `EditorCamera` 以获取 View / Proj。  
4) 绑定 `Framebuffer`，提交网格（当前示例为彩色立方体）。  
5) 如在 Editor 视口，ImGui 使用 `Framebuffer` 的颜色纹理显示。  

## 文件结构速览
- 抽象层：  
  - `buffer.h/.cpp`（工厂仅头声明，具体在 OpenGL 实现）  
  - `vertex_array.h/.cpp`  
  - `shader.h/.cpp`  
  - `framebuffer.h/.cpp`  
  - `renderer_api.h/.cpp`  
  - `render_command.h/.cpp`  
  - `renderer.h/.cpp`  
  - `render_system.h/.cpp`  
  - `graphics_context.h/.cpp`  
  - `editor_camera.h/.cpp`
- OpenGL 实现：`render/opengl/*`

## Include 规范（避免 gl.h 重复）
- 在任何 cpp 里使用 OpenGL：  
  1) `#include <glad/gl.h>`  
  2) `#ifndef GLFW_INCLUDE_NONE ... #define GLFW_INCLUDE_NONE ... #endif`  
  3) `#include <GLFW/glfw3.h>`  
- 头文件避免直接包含 GL 头，使用前向声明 `struct GLFWwindow;`（如 `opengl_context.h`）。

## 扩展点
- 若新增后端（Vulkan/Metal）：实现 `RendererAPI`、`GraphicsContext`、资源抽象对应子类；在 `RendererAPI::Create` 里切换。  
- 若增加多渲染通道/后期：在 `RenderSystem` 中新增/替换 `Framebuffer` 管线即可。  
- 输入耦合：`renderFrame` 目前直接拿 `InputState`，若迁移到事件驱动可解耦到控制层。

## 待办/优化方向
- 完整的错误日志（OpenGL 编译/链接报错回传到日志系统）。  
- 添加网格/材质资源加载与缓存。  
- 抽象渲染队列与批次，减少状态切换。  
- ImGui 渲染通道与场景通道解耦，支持多视口。  
- 支持 MSAA / 可配置的颜色与深度格式。  

## 相关变更记录
- 2026-02-13：`SurfaceIO` 移动至 `runtime/function/window`；修复 GLFW/GLAD 重复包含错误；为主要渲染类添加职责注释。

## Fixed Bugs

### 1. VAO / EBO state pollution during index-buffer creation
- Symptom: when multiple mesh types existed in the same frame, one mesh could render with corrupted topology that looked like bad indices.
- Root cause: `GL_ELEMENT_ARRAY_BUFFER` binding is VAO state. A binding-style `GLIndexBuffer` constructor could overwrite another VAO's EBO while the wrong VAO was bound.
- Fix: `GLIndexBuffer` now uses DSA (`glCreateBuffers` + `glNamedBufferData`) so buffer creation does not depend on the currently bound VAO or target binding state.
- Result: index-buffer creation is side-effect free, and EBO ownership is established only in `VertexArray::setIndexBuffer(...)`.

### 2. Material GPU cache not refreshed after asset reimport
- Symptom: after `.mat` or texture reimport, rendered materials could keep using stale parameters or stale texture bindings until restart.
- Root cause: `RenderSystem::getOrCreateMaterialGPU(...)` returned cached `MaterialGPU` objects without refreshing CPU material data and texture handles.
- Fix: cache hits now refresh `MaterialData` plus all bound texture resources before reuse.
- Result: reimported material and texture changes become visible without restarting the renderer.


## 2026-03-10 Recent Architecture Update

### RenderSystem pipeline
- `renderFrame(...)` has been reduced to frame orchestration only.
- The runtime flow is now split into:
  - `buildRenderPacket(...)`: extract camera, lights and draw items from ECS/scene state
  - `executePasses(...)`: dispatch enabled passes by `RenderFlags`
  - `executeForwardPass(...)`: bind framebuffer / shader / material / mesh and submit draw calls
- Mesh and material submission now go through asset-based GPU caches instead of the old hard-coded cube path.

### Material and lighting path
- Mesh shader now uses `tangent.w` as handedness, with `vec4 tangent` end-to-end.
- Default MR texture is treated as a neutral multiplier, not an additive override.
- Emissive is accumulated independently from albedo.
- Directional light direction is derived from `TransformComponent` world rotation, not stored as a long-lived duplicate field.

### Picking and editor integration
- Entity ID output is now `uint`, matching the `GL_R32UI` attachment.
- Picking uses encoded entity ids so that framebuffer background stays `0` while `entt` entity `0` remains selectable.
- Gizmo/helper passes only write color output and do not overwrite the EntityID attachment.
- Scene switches clear pending pick and selection state.
- In Play mode, Scene-view editing targets the runtime scene; on Stop, the editor context switches back to the editor scene immediately.

### Current risks still worth tracking
- Frame/light uploads are still done through per-uniform updates; UBO migration is the next cleanup target.
- Pass execution is separated, but prepare/sort is still relatively thin and can be expanded later.
- Selection outline, shadowing and post-processing are still placeholders and should be built on top of the current pass split rather than folded back into `renderFrame(...)`.
