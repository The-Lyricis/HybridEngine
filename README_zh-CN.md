# Hybrid Engine
[English](README.md)
一个用于学习现代引擎架构、系统实现与技术探索的个人引擎项目。

## Overview

Hybrid Engine 是一个以学习、研究和实践现代引擎架构为目标的个人项目。这个项目的重点并不只是做出一个可运行的引擎原型，而是在实际实现过程中，逐步理解各个核心模块的职责划分、协作方式以及架构边界，并在此基础上持续探索值得深入打磨的技术方向。

目前项目主要处于基础框架搭建阶段，内容涵盖渲染、资源管理、场景系统、编辑器、输入、事件、物理等核心部分。在整体框架逐渐稳定之后，我计划从中选择若干关键方向继续深入，例如渲染管线设计、物理系统实现、引擎性能优化与性能分析、编辑器交互与工作流逻辑，并逐步推动项目向跨平台与多图形 API 支持演进。

对我而言，这个项目既是一个长期的引擎学习过程，也是一个用于验证架构思路、积累工程经验和探索技术细节的持续性实践平台。

![编辑器预览](docs/images/editor_preview.png)

## 环境要求

当前主要开发环境如下：

- Windows 10 或 Windows 11
- CMake 3.20 及以上版本
- 支持 C++17 的编译器
- 推荐使用 Visual Studio 2022 / MSVC 工具链
- 支持 submodule 的 Git
- 能够运行 OpenGL 4.5 的显卡与驱动环境

补充说明：

- 仓库中已经包含部分跨平台构建痕迹，例如 macOS 构建脚本和 CI 工作流，但当前编辑器代码整体仍以 Windows 环境为主。
- 编辑器依赖 `engine/3rdparty` 下的第三方库，因此首次配置工程前需要先初始化 submodule。

## 构建与运行

首先初始化 submodule：

```bash
git submodule update --init --recursive
```

手动配置并构建：

```bash
cmake -S . -B build
cmake --build build --config Release
```

也可以直接使用 Windows 下的辅助脚本：

```bat
build_windows.bat
```

运行编辑器：

```bat
bin\HybridEditor.exe
```

运行说明：

- 构建过程会将编辑器可执行文件、Shader 和编辑器资源一并复制到 `bin/` 目录下。
- 首次启动时，引擎会自动初始化 `bin/GameProject/`作为项目根(目前是写死的路径)，并在不存在时创建默认的 `GameProject.hyproj`。
- 运行日志会输出到 `bin/hybrid_engine.log`。

## 文档

项目文档位于 `docs/` 目录下：

**开发日志**

- [Changelog](docs/CHANGELOG_zh-CN.md)：开发记录与版本里程碑

**详细文档**

- [Render System](docs/systems/render_system.md)：当前渲染系统架构与 Pass 布局
- [Resource System](docs/systems/resource_system.md)：运行时与编辑器资源管线、注册表、缓存与加载流程
- [Render ECS Asset Chain](docs/systems/render_ecs_asset_chain.md)：网格、材质、ECS 与编辑器流程在渲染路径中的集成说明
- [Event System](docs/systems/event_system.md)：事件分发与输入/事件桥接相关说明
- [Log System](docs/systems/log_system.md)：日志系统结构与使用方式

## 当前功能与开发状态

目前已经完成或基本可用的部分包括：

- 运行时基础框架，包括日志、事件、输入、窗口系统与主循环
- 基础渲染流程，包括 OpenGL 后端、Framebuffer 渲染路径，以及 Scene / Game 视口拆分
- 场景系统，包括基于 EnTT 的实体组件结构、场景序列化、层级数据与 scene document 流程
- 编辑器基础能力，包括 docking 界面、Hierarchy、Inspector、Scene View、Game View、Project Panel、Gizmo 交互与对象拾取
- 资源系统基础流程，包括 VFS 逻辑路径、资源注册表、`.meta` 文件、cooked 缓存输出，以及纹理 / 网格 / 材质 / 场景资源加载
- 导入工作流，包括 OBJ 导入、材质生成、纹理导入、拖拽放入场景，以及初步的热重载支持
- 当前渲染路径下的基础光照支持，包括平行光与点光源
- 物理系统的早期基础，包括 AABB 碰撞、碰撞体注册、刚体迭代与碰撞体 Gizmo 调试显示

目前仍在持续完善的部分包括：

- 渲染管线拆分仍在调整中，更高层的 Pass 调度结构还没有完全定型
- 阴影、描边和后处理相关路径已经开始铺设，但尚未完成
- 物理系统目前仍处于较早期的迭代阶段，整体还不稳定
- 编辑器工作流已经可以用于实验和功能验证，但在细节打磨、稳定性与工具完整度上还有较大提升空间
- 音频与脚本系统目前尚未实现

## 发展路线

接下来一段时间的我重点主要会放在以下几个方向：

- 继续整理渲染管线结构，包括 Pass 执行流程、Shader 管理和数据上传链路
- 改进物理行为、刚体流程以及编辑器侧的调试支持
- 提升编辑态与运行态切换过程中的场景编辑稳定性
- 继续完善资源热重载、缓存失效与导入流程的稳定性

后续计划逐步推进的内容包括：

- 更完整的渲染能力，例如阴影、描边与后处理
- 更完善的编辑器工作流与面板细节打磨
- 资源系统中默认资源与异常路径的进一步补全
- 音频系统相关探索
- 在核心架构更稳定之后，继续推进脚本系统与运行时扩展能力

更完整的开发计划可以参考 [docs/plans/PROJECT_PLAN_ENG.md](docs/plans/PROJECT_PLAN_ENG.md)。

## 写在后面

Hybrid Engine 目前主要作为我的长期个人学习与研究项目持续开发。

特别感谢 [@SaluteBEE](https://github.com/SaluteBEE) 在协作与交流中的支持。

联系方式：

- 主页：[Portfolio](https://the-lyricis.github.io/)
- Gmail：picchick1623@gmail.com