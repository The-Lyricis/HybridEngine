# Hybrid Engine 开发计划

## 技术选择

| 组件     | 选择                   | 说明              |
| ------ | -------------------- | --------------- |
| 构建系统   | CMake 3.20+          | 跨平台、可扩展         |
| 图形 API | OpenGL 4.5+          | 学习成本低，工具链成熟     |
| 窗口系统   | GLFW                 | 轻量、稳定           |
| 数学库    | GLM                  | GLSL 兼容         |
| UI 框架  | ImGui                | 编辑器最佳实践         |
| 日志系统   | spdlog               | 高性能、易扩展         |
| 序列化    | JSON (nlohmann_json) | 易用、生态成熟         |
| 脚本系统   | <mark>待定</mark>      | <mark>待定</mark> |

## 引擎结构设计

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
│  │  └─ 平台抽象层
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

## 总计划

- Phase 1.1（基础框架，2-3周）：日志/事件/窗口/ImGui/引擎主循环

- Phase 1.2（渲染实现，3 周）：Shader/Buffer/Texture/Camera/Renderer2D
  
  Phase 1.3（输入系统，1 周）：键鼠输入/Action Mapping
  
  Phase 1.4 （物理系统，2 周）：包围盒/碰撞
  
  Phase 1.5 （音频系统，1 周）：包围盒/碰撞

由于时间问题以下的交给课后实现

- Phase 2.1（场景系统，5 周）：ECS/场景序列化/层级编辑
- Phase 2.2（脚本系统，4 周）：Mono/C# 热重载/字段反射
- Phase 2.3（编辑器完善，4 周）：Gizmo/Profiler/资产浏览

## 分计划

## 1.课程内需要完成第一部分

### Phase 1.1：基础框架

- [x] CMake 构建系统配置

- [x] 日志系统（spdlog）

- [x] 事件系统（Event + Dispatcher + 窗口事件）

- [x] Runtime 窗口系统（GLFW + GLAD + OpenGL Context）

- [ ] ImGui Layer（Runtime 统一封装）

- [ ] 引擎主循环（运行、更新、渲染、退出）

- 里程碑：窗口显示 + 日志输出 + 基础 UI

### Phase 1.2：渲染基础

- [ ] Shader 管理
- [x] Buffer/VertexArray/Framebuffer
- [ ] Texture 加载
- [ ] 2D 渲染器
- [ ] Camera 系统

### Phase 1.3：输入系统

- [x] 键盘/鼠标输入
- [x] 输入事件
- [ ] Action/Axis Mapping

### Phase 1.4：物理系统

### Phase 1.5：音频系统

## 2. 之后的计划

### Phase 4：场景系统

- [ ] ECS 基础
- [ ] Transform/Renderer Component
- [ ] 场景序列化（JSON）
- [ ] Scene Hierarchy

### Phase 5：脚本系统

- [ ] Mono 集成
- [ ] C# 程序集加载/Lua 热更？/原生C++ 脚本集合
- [ ] 热重载
- [ ] 脚本生命周期

### Phase 6：编辑器完善

- [ ] Gizmo 系统
- [ ] Inspector 面板
- [ ] 资源浏览器
- [ ] Profiler 面板
