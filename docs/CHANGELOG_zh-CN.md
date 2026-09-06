# Hybrid Engine 更新日志

### v0.0.6 Current

- 加入 File/系统关闭统一的脏文档退出流程、原子更新启动场景的 Project Settings，以及版本化 Game View 分辨率设置。
- 加入线程安全的有界日志缓冲与 Editor Console，并为合并执行的后台导入提供可观察、可重试的 Tasks 面板。
- 用稳定 `RenderViewId` 目标池替代固定编辑器 Framebuffer，Editor 直接消费 `RenderFrameResult`。
- 将单体 Runtime 拆为 Core、Runtime、Render、Engine 目标，加入仅 Windows 构建的 Editor 开关与跨平台 Player。
- 加入显式幂等生命周期、Layer 独占所有权、单次输入处理、固定步长时钟，以及由 Editor 管理的 Play Session。
- 加入可 drain 的固定线程池 JobSystem，以及基于 generation 的安全异步资产加载和返回值式 Registry 查询。
- 以多视图帧请求/结果替代 Editor 专用渲染入口，引入 `.hyproj` v2 启动场景，并增加 headless CTest 覆盖。
- 渲染系统继续朝着更清晰的 Pass / Pipeline 拆分推进，为后续的描边、阴影和后处理扩展做准备。
- 添加了 `ShaderLibrary` 的初始实现，用于集中管理 Shader。
- 编辑器新增了场景创建流程，并在层级面板中加入了点光源创建入口。
- 物理与刚体逻辑仍在持续迭代中，`physics_system.cpp` 及相关组件在同一天内进行了多次更新。
- 系统更新日志系统，DEBUG 级别日志更新, 增加日志规范, 统一日志系统。

### v0.0.5

日期：2026-03-10

- 添加了第一版物理系统与 Play 按钮流程，并进一步扩展了 AABB 碰撞、碰撞体注册以及碰撞体 Gizmos 可视化。
- 引入 `scene_document`，用于分离编辑器场景与运行时场景处理，并将 Play 模式切换到基于 document 的流程上。
- 完成了场景保存/加载流程，并修复了多个 Play 模式退出与选择状态相关问题。
- 将编辑器拆分为独立的 Scene 与 Game 视口，并大幅重构了项目面板与层级面板的工作流。
- 改进了场景序列化，加入根节点序列化支持，并修复了层级反序列化顺序问题。
- 添加了编辑器侧资源热重载，并支持将 OBJ 资源直接拖入场景。

### v0.0.4

日期：2026-02-28

- 进一步推进资源管线，加入 cooked texture 输出、自动生成 `.meta` 文件，以及基于事件驱动的导入流程，同时明确了 editor/runtime 的职责拆分。
- 将 Transform 升级为四元数旋转，并加入父子层级数据，以支持更稳定的场景树结构。
- 扩展了编辑器交互能力，加入拖拽、层级树编辑，以及选择 / 缩放 / 旋转工具支持。
- 重新整理了 editor 与 runtime 的源码结构，使模块边界更加清晰。
- 集成 `tinyobjloader`，完成了 OBJ 导入、材质加载、运行时渲染以及场景资源序列化整条链路。
- 为项目面板加入场景切换能力，使导入的场景资源可以直接进入可编辑、可运行流程。

### v0.0.3

日期：2026-02-20

- 建立了第一版较完整的资源系统基础，包括 `RuntimeResourceSystem`、资源元数据索引、VFS 路径映射，以及最终确定的 `alias:relative` 逻辑路径规则。
- 让渲染系统更接近资源驱动流程，加入纹理加载、网格/材质资源流、默认材质回退机制，并整理了渲染系统结构。
- 添加了基础光照组件以及一个平行光测试场景。
- 将编辑器 UI 拆分为独立面板，并将层级面板与 Inspector 面板接入场景系统。
- 添加了对象拾取、更完善的编辑器相机控制，以及 ImGuizmo 坐标轴拖拽支持。
- 加入了多平台 CMake 工作流，并更新了若干 Windows 构建脚本。

### v0.0.2

日期：2026-02-13

- 添加日志系统，包括基于 `spdlog` 的 core/client 通道以及宏辅助封装。
- 添加事件系统，包括 `Event`、`Dispatcher`、应用程序/输入事件类型，以及 Layer/Overlay 的逆序分发机制。
- 建立了第一版完整输入链路，包括 `InputLayer` 采样与 `LayerStack` 分发，覆盖键盘、鼠标、滚轮与字符输入。
- 基于 GLFW 与 GLAD 建立窗口系统与主循环。
- 添加了第一版基于 Framebuffer 的渲染测试，以及基础的 `EditorCamera` 移动、拖拽与缩放控制。

### v0.0.1

日期：2026-01-28

- 初始化项目框架。
- 仅完成基础 CMake 配置、第三方依赖接入以及早期引擎结构搭建。
