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
