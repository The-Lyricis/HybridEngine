# Hybrid Editor 演进计划

## 目标

Hybrid Editor 不以复刻 Unity 或 Unreal 的外观为目标。短期保留用户熟悉的场景、资源和属性概念，长期建立自己的工作流：让用户围绕“当前意图”完成任务，而不是在固定面板之间搬运信息。

ImGui 继续作为近期 UI 渲染层，但不再承担产品架构。命令、选择、工作区、主题和持久化必须独立于 ImGui API，未来才能替换渲染层或引入原生窗口而不重写编辑器逻辑。

## 产品原则

1. **意图优先**：创建、布置、调试、构建是不同工作状态，每个状态只呈现当前所需工具。
2. **一处定义，多处调用**：菜单、快捷键、命令面板、右键菜单和工具栏调用同一个 Action，不各写一套逻辑。
3. **上下文连续**：切换工作区不丢失选择、相机、过滤器、历史和任务状态。
4. **渐进披露**：默认界面安静；高级属性、诊断信息和批处理能力按需展开。
5. **键鼠平权**：任何核心操作既能被发现，也能由命令面板和可重映射快捷键完成。
6. **平台原生习惯**：macOS 使用 Command、Control-click 和系统对话框；Windows 使用 Ctrl 和对应系统语义。
7. **可恢复**：危险操作可撤销，耗时操作有状态，失败操作有原因和重试入口。

## 建议的独立交互模型

### Workspace：按任务组织，而不是按窗口堆叠

编辑器顶层划分为五个可切换工作区：

- **World**：场景构建、实体关系、空间工具和实时预览。
- **Assets**：导入、依赖、变体、批处理和资源质量检查。
- **Logic**：脚本、事件、状态机以及运行时数据检查。
- **Diagnose**：日志、帧分析、渲染图、内存和任务追踪。
- **Ship**：目标平台、Cook、打包、验证和发布结果。

这不是五套互不相关的布局。它们共享 Selection、Command History、Task Center 和项目上下文，只改变主画布及辅助工具。

### Focus Canvas：主画布是任务中心

中央区域不固定等同于 Unity 的 Scene/Game 标签。它是可切换的 Focus Canvas：3D 世界、资产预览、材质图、逻辑图、帧调试器都可以成为主任务。辅助视图作为 Lens 叠加或停靠，而不是永久占据屏幕。

### Context Lens：用上下文镜头替代单一 Inspector

右侧区域由当前选择和当前任务共同决定：

- Quick：高频字段和常用操作。
- Structure：组件、层级和依赖。
- Runtime：运行时值、覆盖值和来源。
- Diagnose：警告、成本、引用者和修复建议。

用户不需要在一个不断增长的 Inspector 中寻找所有信息。

### Command Surface：统一所有操作入口

引入 Action Registry 和 Command Palette。每个 Action 声明：稳定 ID、名称、图标、分类、上下文条件、快捷键、执行函数和撤销策略。菜单、工具栏、右键菜单以及命令搜索只负责展示 Action。

### Task Center：异步工作可见

导入、编译、Cook、资源扫描和着色器构建统一进入 Task Center。底部状态条只显示摘要；展开后可查看阶段、耗时、日志、失败原因、取消与重试。

## 当前问题审计

| 领域 | 当前风险 | 近期处理 |
| --- | --- | --- |
| 次级点击 | 依赖右键释放，触控板和焦点切换时容易丢失 | 按下触发，并支持 macOS Control-click |
| Scene 导航 | 右拖只在光标位于视口时有效 | 右键拖动期间捕获鼠标，释放后恢复 |
| 平台修饰键 | 多选仍按 Ctrl 解释 | macOS 使用 Command，Windows 使用 Ctrl |
| 快捷键提示 | macOS 菜单仍显示 Ctrl | 按平台显示 Cmd/Ctrl，并统一语义 |
| Retina | UI 尺寸和渲染像素尺寸尚未形成显式契约 | 分离逻辑尺寸、Framebuffer 尺寸和 UI Scale |
| 字体与图标 | 默认字体、零散 PNG、无状态规范 | 字体图集、矢量图标源、统一尺寸与状态色 |
| 主题 | 直接使用 `StyleColorsDark` 并散落颜色常量 | Theme Tokens + 组件样式层 |
| 操作入口 | 菜单、工具栏、右键菜单各自调用回调 | Action Registry + Command Router |
| 面板状态 | 布局依赖 ImGui ini，缺少版本迁移 | 版本化 Workspace State |
| 可访问性 | ImGui 内容几乎不暴露语义 | 建立语义元数据；关键对话框逐步原生化 |

## 技术架构

```text
Platform Input
      │
      ▼
Input Gesture Map ──► Action Registry ◄── Command Palette / Menu / Toolbar / Context Menu
                            │
                            ▼
                       Command Router
                     ┌──────┴──────┐
                     ▼             ▼
               Undoable Command   Immediate Action
                     │             │
                     └──────┬──────┘
                            ▼
                   Editor Context / Services
                            │
          ┌─────────────────┼──────────────────┐
          ▼                 ▼                  ▼
      Workspace         Task Center        Notification
          │
          ▼
    UI Renderer (ImGui today, replaceable later)
```

建议新增的核心模块：

- `EditorActionRegistry`：Action 描述、可用性、执行和搜索索引。
- `EditorGestureMap`：平台无关手势与用户快捷键映射。
- `EditorCommandRouter`：统一即时操作、可撤销命令和异步任务。
- `EditorWorkspaceService`：工作区定义、切换、布局和状态恢复。
- `EditorTheme`：颜色、间距、圆角、字体、图标和动画时长 Token。
- `EditorNotificationService`：Toast、状态、错误详情和操作反馈。
- `EditorTaskService`：导入、构建、扫描等异步工作的统一生命周期。

## 分阶段实施

### Phase E0：跨平台交互基线

目标：所有基础操作在 Windows/macOS 上行为一致。

- 完成鼠标按键、Control-click、Command/Ctrl、滚轮、拖放和焦点矩阵。
- Scene 相机捕获、窗口失焦自动释放、弹窗期间禁止捕获。
- 明确逻辑坐标、Framebuffer 坐标和 Retina Scale。
- 为输入状态和 Action 可用性补单元测试。
- 建立 macOS/Windows 编辑器 smoke checklist。

验收：右键菜单、选择、拖拽、相机、保存和撤销在两端通过同一套用例。

### Phase E1：视觉基础设施

目标：从默认 ImGui 观感升级为一致的 Hybrid 视觉语言。

- 引入 Theme Tokens，移除面板中的硬编码颜色和尺寸。
- 建立 4/8 像素间距体系、控件高度、边框、圆角和层级阴影规范。
- 配置多字号字体图集、中英文回退字体和 Retina 清晰度。
- 将工具图标整理为统一矢量源并生成多 DPI 图集。
- 实现 Button、Toolbar、Tree Row、Property Row、Search、Empty State 等基础组件。

验收：不改业务逻辑即可切换主题；100%/200% DPI 下无模糊和错位。

### Phase E2：统一命令与导航

目标：消除操作入口之间的重复逻辑。

- 落地 Action Registry、Gesture Map 和 Command Palette。
- 菜单、工具栏和上下文菜单全部由 Action 描述生成。
- 增加全局搜索：Action、实体、资源、设置和最近文档。
- 快捷键支持冲突检测、上下文作用域和用户重映射。

验收：新增一个 Action 无需分别修改四种入口；核心操作可纯键盘完成。

### Phase E3：Hybrid Workspace Shell

目标：建立区别于 Unity 的顶层操作结构。

- 实现 World / Assets / Logic / Diagnose / Ship 工作区。
- 引入 Focus Canvas、Context Lens、Activity Rail 和 Task Center。
- 布局状态版本化，支持恢复、预设、迁移和安全重置。
- 保留 Classic 布局作为迁移入口，不强制一次性切换。

验收：用户完成一个典型任务时，跨面板移动次数和无关信息显著减少。

### Phase E4：内容生产效率

目标：让独立交互模型真正产生效率优势。

- 多选批量属性编辑、Prefab/模板、资源依赖图和引用查找。
- Scene 操作工具架、快速创建、对齐、吸附、书签和局部隔离。
- 资产导入规则、批处理、变体和质量诊断。
- Play Mode 差异查看、运行时覆盖和选择保持。

验收：场景布置、资源修复和问题定位有可量化的步骤减少。

### Phase E5：诊断与发布闭环

目标：从“场景编辑器”升级为完整生产环境。

- Render Graph/Pass、GPU/CPU 帧时间、资源驻留与任务时间线。
- 构建配置、Cook、平台验证、产物浏览和错误定位。
- 问题项可直接跳转到实体、资源、代码位置或修复 Action。

验收：从问题发现到定位、修复、重新验证能在编辑器内闭环。

## 优先级与节奏

建议按 `E0 → E1 基础 → E2 → E3` 顺序推进。不要先制作大量漂亮面板：没有 Action、Theme 和 Workspace 抽象时，视觉改版会把当前耦合固化得更深。

每个阶段采用垂直切片：先选择一条真实工作流（例如“创建实体 → 调整 Transform → 保存 → 撤销”），同时覆盖交互、视觉、命令和测试，再扩展到其他功能。

## 成功指标

- 启动到可交互时间、空闲帧耗时和编辑器内存。
- 高频任务的点击数、面板切换数和完成时间。
- Action 的快捷键覆盖率与命令面板可达率。
- 跨平台 smoke case 通过率。
- UI 硬编码颜色/尺寸数量和直接 ImGui 业务调用数量。
- 崩溃恢复成功率、异步任务失败可诊断率和危险操作可撤销率。

## 暂不建议

- 不为了“高级感”立即重写整套 UI 框架。
- 不同时维护 Unity Classic、Unreal 风格和新 Hybrid 三套永久交互。
- 不先做复杂动画；响应速度、层级、留白和一致性更重要。
- 不让面板直接操作 Runtime 单例；继续通过 Editor Context、Action 和 Service 边界访问。
