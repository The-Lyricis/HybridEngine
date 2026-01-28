# Hybrid Engine 开发计划

## 技术选择
| 组件 | 选择 | 说明 |
|------|------|------|
| 构建系统 | CMake 3.20+ | 跨平台、可扩展 |
| 图形 API | OpenGL 4.5+ | 学习成本低，工具链成熟 |
| 窗口系统 | GLFW | 轻量、稳定 |
| 数学库 | GLM | GLSL 兼容 |
| UI 框架 | ImGui | 编辑器最佳实践 |
| 日志系统 | spdlog | 高性能、易扩展 |
| 序列化 | JSON (nlohmann_json) | 易用、生态成熟 |
| 脚本系统 | C# (Mono/.NET) | 参考 Hazel 方案 |

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
│  │  ├─ Physics System (可选)
│  │  └─ Script System (C#)
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
- Phase 1（基础框架，4 周）：日志/事件/窗口/ImGui/引擎主循环
- Phase 2（渲染基础，5 周）：Shader/Buffer/Texture/Camera/Renderer2D
- Phase 3（场景系统，5 周）：ECS/场景序列化/层级编辑
- Phase 4（脚本系统，4 周）：Mono/C# 热重载/字段反射
- Phase 5（输入系统，2 周）：键鼠输入/Action Mapping
- Phase 6（编辑器完善，4 周）：Gizmo/Profiler/资产浏览

## 分计划
### Phase 1：基础框架
- [x] CMake 构建系统配置
- [x] 日志系统（spdlog）
- [ ] 事件系统（Event + Dispatcher + 窗口事件）
- [ ] Runtime 窗口系统（GLFW + GLAD + OpenGL Context）
- [ ] ImGui Layer（Runtime 统一封装）
- [ ] 引擎主循环（运行、更新、渲染、退出）
- 里程碑：窗口显示 + 日志输出 + 基础 UI

### Phase 2：渲染基础
- [ ] Shader 管理
- [ ] Buffer/VertexArray/Framebuffer
- [ ] Texture 加载
- [ ] 2D 渲染器
- [ ] Camera 系统

### Phase 3：场景系统
- [ ] ECS 基础
- [ ] Transform/Renderer Component
- [ ] 场景序列化（JSON）
- [ ] Scene Hierarchy

### Phase 4：脚本系统
- [ ] Mono 集成
- [ ] C# 程序集加载
- [ ] 热重载
- [ ] 脚本生命周期

### Phase 5：输入系统
- [ ] 键盘/鼠标输入
- [ ] 输入事件
- [ ] Action/Axis Mapping

### Phase 6：编辑器完善
- [ ] Gizmo 系统
- [ ] Inspector 面板
- [ ] 资源浏览器
- [ ] Profiler 面板
