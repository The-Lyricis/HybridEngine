# Log System 模块文档

## 计划（实现前）

### 目标

- 提供统一的日志输出（引擎核心与客户端分离）
- 支持控制台 + 文件输出
- 提供最小配置接口，便于后续扩展（格式、等级、文件名）
- 作为其他模块的基础依赖（窗口、渲染、脚本等）

### 阶段验收

- L1：能在控制台输出日志
- L2：能输出到文件并保留格式
- L3：区分 Core / Client 两类日志
- L4：初始化/关闭流程稳定，可重复运行

### 初步类设计

```
LogSystem
├─ Config
├─ Init(cfg)
├─ Shutdown()
├─ Core()
└─ Client()
```

### 接口设计

- `Init(const Config& cfg = {})`
- `Shutdown()`
- `Core()` / `Client()` 获取 logger
- 宏封装：`HBD_CORE_*` / `HBD_*`

### 层级结构设计

```
engine/source/runtime/core/log/
├─ log_system.h
└─ log_system.cpp
```

---

## 实现步骤（已完成）

1. 引入 spdlog 依赖（`engine/3rdparty/spdlog`，CMake 链接 `spdlog::spdlog`）
2. 定义 `LogSystem::Config`，包含：
   - `logfile`
   - `truncate_file`
   - `level` / `flush_level`
   - `console_pattern` / `file_pattern`
3. 在 `Init()` 中：
   - guard 防重复初始化
   - 创建 console sink 与 file sink
   - 设置输出格式
   - 创建 `HYBRID_CORE` 与 `HYBRID_CLIENT` 两个 logger
   - 注册 logger、设置 level/flush
4. `Shutdown()` 中：
   - drop 指定 logger
   - reset 指针
5. 提供宏封装供业务层使用

---

## 当前实现

### 文件位置

- `TDA572/engine/source/runtime/core/log/log_system.h`
- `TDA572/engine/source/runtime/core/log/log_system.cpp`

### 接口与宏

- `Hybrid::LogSystem::Init(const Config& cfg = {})`
- `Hybrid::LogSystem::Shutdown()`
- `Hybrid::LogSystem::Core() / Client()`
- `HBD_CORE_TRACE/INFO/WARN/ERROR/CRITICAL`
- `HBD_TRACE/INFO/WARN/ERROR/CRITICAL`

### 使用示例

```
Hybrid::LogSystem::Init();
HBD_CORE_INFO("Engine initialized.");
HBD_INFO("Client log.");
Hybrid::LogSystem::Shutdown();
```

---

## 总结（实现后）

### 已达成

- Core / Client 双通道日志
- 控制台 + 文件双输出
- 可配置日志格式与等级
- 简单 guard 避免重复初始化

### 已知限制

- 未支持异步日志
- 未支持运行时动态调整配置
- 未封装日志作用域或统计

### 下一步可扩展方向

- 添加异步 logger（`spdlog::async_logger`）
- 支持按模块/文件名分类日志
- 增加日志过滤与标签系统
- 输出到 ImGui 控制台窗口
