# Hybrid Engine v0.0.1

## Build and Environment (keep updated)
Prerequisites:
- CMake 3.20+
- A C++17 compiler (MSVC is the primary target)

Initialize submodules:
```
git submodule update --init --recursive
```

Configure and build:
```
cmake -S . -B build
cmake --build build --config Release
```

Or use:
```
build_windows.bat
```

Run (after build):
```
<repo>/bin/TDA572Editor.exe
```
## Current Structure

Hybrid Engine
│
├─ Runtime 
│  ├─ Core/ 
│  │  ├─ Log System (spdlog)
│  │  ├─ Event System
│  │  ├─ Time Management
│  │  ├─ Config System (JSON)
│  │  └─ UUID/Serialization
│  │
│  ├─ Function/ 
│  │  ├─ Window & Context (GLFW + OpenGL)
│  │  ├─ Input System
│  │  ├─ Render System (OpenGL)
│  │  │  ├─ Shader/Buffer/Texture
│  │  │  ├─ Camera System
│  │  │  └─ Renderer2D/3D
│  │  ├─ Scene System (GameObject/Component)
│  │  ├─ Physics System 
│  │  ├─ Audio System 
│  │  └─ Script System (C# via Mono)
│  │
│  ├─ Platform/ 
│  │  ├─ IWindow
│  │  ├─ IInput
│  │  └─ IPlatform
│  │
│  └─ Resource/ 
│     ├─ Asset Loader
│     ├─ Texture/Model Manager
│     └─ Serializer (JSON)
│
└─ Editor 
   ├─ ImGui UI Framework
   ├─ Scene Editor
   ├─ Inspector Panel
   ├─ Asset Browser
   └─ Project Settings

## 

## Version Notes
### v0.0.1
- Pre-release scaffold.
- Currently only a basic framework and core third-party libraries.

## Dependencies (current)
- GLFW
- GLAD
- Dear ImGui
- GLM (submodule, not wired yet)
