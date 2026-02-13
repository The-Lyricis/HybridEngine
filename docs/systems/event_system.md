# Event System 模块说明

## 规划（已完成）

### 目标
- 提供统一的事件对象模型（类型、类别、Handled）。
- 支持 Layer/Overlay 逆序分发与可拦截传播。
- 与 GLFW 输入/窗口事件打通，形成 Engine 级入口。
- 输入采样与逻辑分发两阶段，保证输入状态完整。

### 阶段拆分
- L1：事件基类 + Dispatcher（已完成）
- L2：窗口事件 + 输入事件类型（已完成）
- L3：SurfaceIO 桥接 GLFW → Event → Engine::onEvent（已完成）
- L4：InputLayer 采样 + LayerStack 分发（已完成）
- L5：Layer 生命周期 OnAttach/OnDetach 自动调用（未完成，待补）

### 关键接口
```
Event / EventDispatcher
Layer / LayerStack
Window* / App* / Input* 事件类型
SurfaceIO::registerOnEventFunc(std::function<void(Event&)>)
InputLayer::onEvent / onUpdate / getState
```

### 目录结构
```
engine/source/runtime/core/event/
  event.h
  application_event.h
  input_event.h
  layer.h
  layer.cpp

engine/source/runtime/function/input/
  input_state.h
  input_layer.h

engine/source/runtime/function/render/
  surface_io.h
  surface_io.cpp   // GLFW 回调 -> Event -> onEvent
```

## 实现现状

- **事件模型**：`EventType/EventCategory` + Hazel 风格宏；`EventDispatcher`。
- **Layer 分发**：`LayerStack` 支持普通层/Overlay，逆序分发，Handled 截断。
- **事件类型**：窗口/应用事件（Close/Resize/Focus/Update/Render/Tick）、输入事件（键盘/鼠标/滚轮/字符）。
- **入口桥接**：`SurfaceIO` 在 GLFW 回调中构造事件，通过 `registerOnEventFunc` 调用 `HybridEngine::onEvent`。
- **输入两阶段**：
  - 采样：`InputLayer::onEvent` 更新 `InputState`（不受 Handled 影响）。
  - 使用：渲染/逻辑在 `renderFrame(...)` 中读取 `InputState`，并可用 `viewportActive` 做 UI 占用判断。
- **主循环**（engine.cpp）：`LayerStack.onUpdate -> pollEvents -> renderFrame`，事件在同帧可用。

## 已知缺口
- `LayerStack` 未自动调用 `Layer::onAttach/onDetach`。
- 系统级 resize 后未调用渲染器的 FBO resize。
- 仍依赖 GLFW，同步派发；无事件队列。

## 后续建议
- 在 `pushLayer/popLayer` 时调用 `onAttach/onDetach`。
- 在 `WindowResizeEvent` 中触发 `RenderSystem` 的 framebuffer/viewport 更新。
- 若需要更平滑输入，可在 `InputState` 中合并鼠标移动/滚轮（可选队列化）。
- 补充宏/助手以减少事件类型样板代码（可选）。

## 使用示例
```cpp
// 注册入口（SurfaceIO）
surface_io->registerOnEventFunc([this](Event& e){ onEvent(e); });

// Engine::onEvent
inputLayer->onEvent(e);                  // 采样
EventDispatcher d(e);
d.dispatch<WindowCloseEvent>(...);
for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it) {
    (*it)->onEvent(e);
    if (e.Handled) break;
}

// 主循环（简化）
inputLayer->onUpdate(dt);
m_Window->pollEvents();
renderSystem.renderFrame(viewportSize, window, dt,
    viewportActive, inputLayer->getState());
```

