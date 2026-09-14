# Hybrid 跨平台渲染开发主计划

> 状态基线：`90d93e4`（2026-09-12）  
> 本文是平台层、RHI、RenderGraph、Shader 与编辑器渲染适配的当前执行依据。

## 1. 目标与范围

Hybrid 的运行时和编辑器业务层不得绑定 GLFW、OpenGL、Metal 或 Vulkan。窗口、输入、GPU 资源、渲染命令和编辑器图片显示均通过稳定接口访问，具体实现由平台及图形后端提供。

目标后端：

| 平台 | 首选后端 | 兼容后端 | 首批范围 |
| --- | --- | --- | --- |
| macOS 13+ / Apple Silicon | Metal | OpenGL 回归 | Editor + Player |
| Windows | Vulkan（完成后评估默认） | OpenGL | Editor + Player |
| Linux | Vulkan | OpenGL | Player，后续 Editor |

不使用 MoltenVK 作为 macOS 默认实现。Vulkan 在 Metal Scene View 对齐后开始，避免同时维护两个未稳定的新后端。

## 2. 开发模式

当前渲染重构启用快速迭代模式：新 RHI 合约是唯一目标，不为旧 `Framebuffer`、`RenderCommand`、`VertexArray`、旧 `Shader` 或字符串 Uniform API 新增兼容层。迁移一个子系统时，应同步更新其调用方并删除被替代路径。

允许短期功能降级，但必须在提交说明中列明，并在对应里程碑结束前恢复。项目资产、序列化格式及用户工程数据不属于可随意破坏的内部接口。

## 3. 目标架构

```text
Editor / Player / Runtime Business
                │
                ├── Platform API: IWindow / NativeWindowHandle / Input
                │
                ├── Render Pipeline: RenderGraph / IRenderFeature / Pass
                │                         │
                │                         └── RHI handles + ICommandList
                │                                      │
                └── Editor UI Shell                     ├── OpenGL
                          │                              ├── Metal
                          └── IImGuiRenderBackend        └── Vulkan
```

依赖方向固定为：

`HybridCore -> HybridRuntime -> HybridRHI -> HybridRender -> HybridEngine -> Editor/Player`

- 平台实现位于 `runtime/platform/<platform-or-library>`。
- RHI 公共合约位于 `modules/render/rhi`。
- 后端代码位于 `modules/render/backend/<backend>`。
- Pass 只能依赖 RHI、RenderGraph 和运行时数据，不得包含后端头文件。
- 编辑器面板只持有 `EditorImageHandle`，不得保存原生 GPU ID。

## 4. 稳定公共契约

### 平台层

- `IWindow` 负责窗口尺寸、Framebuffer 尺寸、Content Scale、焦点、光标模式、事件轮询与原生句柄。
- `WindowDesc.graphics_backend` 必须在创建窗口前确定。OpenGL 创建上下文窗口，Metal/Vulkan 创建 `GLFW_NO_API` 窗口。
- 平台工厂负责实现选择；业务入口不得出现 `_WIN32`、`__APPLE__` 实例化分支。
- 文件对话框和平台服务只接收 `NativeWindowHandle`。

### RHI

- `IRenderDevice` 由 `RenderSystem` 独占，不使用静态全局渲染 API。
- GPU 对象使用带代际校验的强类型 Handle。
- `ICommandList` 是 Pass 提交绘制、复制与 Readback 的唯一途径。
- Pipeline 完整声明 Shader、顶点布局、拓扑、Cull、Depth、Blend 与 attachment format。
- 创建失败返回结构化 `RhiResult/RhiStatus`，不允许静默 `nullptr`。
- 后端不可用时启动失败；只有显式 `--allow-render-fallback` 才能回退。

### 资源绑定

| Set | 语义 | 内容 |
| --- | --- | --- |
| 0 | Frame | 相机、投影、时间、Viewport |
| 1 | View / Light | 灯光、阴影、视图级参数 |
| 2 | Material | 材质参数及纹理 |
| 3 | Draw | Model、Tint、Entity ID、逐 Draw 参数 |

Pass 不得按字符串查找 Uniform。当前 OpenGL RHI 中的名字仅是迁移期 Shader 反射占位，M2 由 Slang 反射产物替代。

### RenderGraph

- Pass 显式声明 read、write、read-write 资源。
- 外部资源由视图 Render Target 导入；临时纹理由图按视图创建、Resize 和销毁。
- 自定义 `IRenderFeature` 只能通过注入点和图资源扩展渲染流程。
- 后续编译器负责依赖排序、生命周期分析、资源别名、屏障和可合并 Render Pass。

## 5. 当前完成度

### 已完成

- `IWindow`、GLFW 平台实现、原生窗口句柄及后端创建前选择。
- `IRenderDevice`、`ICommandList`、`ISwapchain`、强类型 Handle 和结构化错误。
- Null RHI、OpenGL RHI、OpenGL Swapchain 与后端工厂。
- RenderGraph、动态命名资源、Blackboard、Feature 注入点和临时纹理物化。
- RenderSystem 持有设备、Swapchain 和每视图图资源池。
- PostProcess、Selection Overlay、ScenePass、SelectionMask 和 ShadowPass 的 RHI Command List 路径。
- SelectionMask 与 ShadowPass 共享 Scene Draw/Material UBO 定义；Shadow 的四级深度资源通过 RenderGraph 导入。
- ScenePass 的 Frame/Draw/Material UBO、MRT、深度、Opaque/Transparent Pipeline 和 Entity ID。
- Mesh 的 RHI Vertex/Index Buffer 创建、失效和销毁。
- ImGui 渲染桥、EditorTextureService、EditorImageHandle 的 OpenGL 实现。
- macOS 输入/窗口适配和 `.app` 图标。
- RHI 单元测试、OpenGL Contract Tests、生命周期测试和源码边界检查。

### 当前已知功能缺口

- 新 ScenePass 尚未接回材质纹理、完整 PBR、LightBlock 和级联阴影。
- SelectionMask、Shadow 已移除旧渲染调用，但尚未接回贴图 Alpha Mask；Skybox、Gizmo 仍有旧渲染接口，Grid 目前还是空 Pass。
- Render Target 仍通过旧 Framebuffer 适配外部图资源。
- ShaderLibrary 仍会创建旧 OpenGL Shader 对象。
- RenderGraph 尚未实现资源别名、屏障和 Render Pass 合并。
- Metal、Slang 和 Vulkan 尚未开始实现。

## 6. 固定实施顺序

### M1：旧接口清零与 OpenGL 基线

1. ScenePass 接入 RHI 纹理、Sampler、MaterialBlock、LightBlock 和 ShadowBlock。
2. SelectionMask 与 ShadowPass 复用 RHI Mesh/Draw/Material 数据。
3. Skybox、Grid、World Gizmo、Overlay Gizmo 迁移到 Command List。
4. 用 RHI Render Target/Attachment 替换旧 Framebuffer 外部适配。
5. 删除旧 `RenderCommand`、`VertexArray`、`Shader`、`UniformBuffer` 和 Renderer API。
6. 强化源码检查，使旧接口和后端 API 无法重新进入业务层。

M1 完成条件：Runtime/Editor 业务层对旧接口扫描为零，OpenGL Editor/Player 工作流可运行，RHI Contract Tests 全部通过。

### M2：Slang 与 Metal 最小垂直链路

1. 固定版本 Slang SDK；`.slang`/HLSL 成为唯一 Shader 源。
2. 构建期输出 OpenGL GLSL、Metal MSL/metallib、Vulkan SPIR-V 和统一反射数据。
3. Player 只加载编译产物；Editor 热重载调用同一编译器。
4. `HybridRhiSmoke` 完成 Metal 窗口、Swapchain、清屏、三角形、Resize、Readback。
5. `HybridImGui_Metal` 完成 Docking、菜单、面板、图标和空画布。

M2 不要求 Metal 完整场景，macOS 默认仍为 OpenGL。

### M3：Metal Scene View 对齐

依次实现 Mesh/Camera、材质纹理、灯光、MRT/Depth、Shadow/Skybox/PostProcess、Picking、Selection、Gizmo、Game View、热重载和资源重建。

所有验收通过后，macOS 默认切换为 Metal，OpenGL 保留为显式回归后端。

### M4：Vulkan

- Windows/Linux Surface、Device、Swapchain、Descriptor 与同步。
- 直接消费 Slang SPIR-V 和反射数据。
- 接入 ImGui Vulkan 后端。
- 复用 RHI Contract Tests、256×256 图像回归和场景基准。
- 稳定后再决定 Windows 默认后端。

## 7. 多端开发分工边界

| 工作流 | 可修改区域 | 不应修改 |
| --- | --- | --- |
| RHI Contract | `render/rhi`、公共测试 | 任一具体平台窗口实现 |
| OpenGL | `backend/opengl`、对应测试 | Metal/Vulkan 后端内部 |
| Metal | `backend/metal`、`imgui/metal`、Smoke | OpenGL 实现 |
| Vulkan | `backend/vulkan`、`imgui/vulkan`、Smoke | Metal 实现 |
| Platform | `runtime/platform` | Render Pass 业务逻辑 |
| RenderGraph/Pass | `runtime/pipeline`、`runtime/passes` | 原生 API 与后端头文件 |
| Editor UI | `editor/services/render`、面板 | 原生纹理 ID |
| Shader | `.slang`、编译器、反射 | 多套手写后端 Shader |

公共 RHI 合约变更必须先更新 Null 后端和 Contract Tests，再同步具体后端。后端开发不得通过在业务层增加条件编译解决差异。

## 8. 构建目标

- `HybridRHI`
- `HybridRHI_OpenGL`
- `HybridRHI_Metal`
- `HybridRHI_Vulkan`
- `HybridPlatform_GLFW`
- `HybridImGui_OpenGL`
- `HybridImGui_Metal`
- `HybridImGui_Vulkan`
- `HybridRhiSmoke`

链接约束：`glad` 仅属于 OpenGL 后端；Metal/QuartzCore/Foundation 仅属于 Metal 后端；Vulkan SDK 仅属于 Vulkan 后端；GLFW 不得成为 `HybridRender` 的公共依赖。

## 9. 测试与提交门槛

每个跨平台提交至少完成：

1. 构建受影响目标。
2. 运行非 GPU 单元测试与源码边界检查。
3. 修改具体后端时运行对应 RHI Contract Test。
4. 修改实际 Pass 时运行生命周期测试或后端 Smoke。
5. `git diff --check` 无错误。

里程碑验收还包括：句柄失效、资源生命周期、Resize、Readback、固定 256×256 像素/容差图像对比，以及 Editor 的启动、Docking、右键、Retina、退出和资源清理。

## 10. 下一执行项

当前立即执行 M1.1：为 ScenePass 增加 RHI Texture/Sampler 资源和完整 MaterialBlock，恢复材质贴图与 Alpha Mask；SelectionMask 和 ShadowPass 复用这些绑定。随后接入 LightBlock 与级联阴影，再迁移 Skybox、Grid、Gizmo，最后清除旧 Render Target/Shader 接口。
