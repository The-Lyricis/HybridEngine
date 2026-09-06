# Hybrid Project Format v2

`.hyproj` is a UTF-8 key/value file. Version 2 is intentionally a breaking project contract; the engine does not rewrite or delete older projects.

```ini
format_version=2
name=Example
startup_scene=asset:Scenes/Main.scene
assets=Assets
cache=Cache
build=Build
settings=ProjectSettings
```

`format_version=2` is required. `startup_scene` is a VFS logical path. It may be empty while using the Editor, but Player requires either this value or `--scene`.

Player usage:

```text
HybridPlayer --project <file.hyproj> [--scene <logical-path>] [--headless] [--max-frames <N>]
```

`--scene` overrides `startup_scene`. Missing projects, unregistered paths, non-Scene assets, and deserialize failures return non-zero exit codes and print the failing path.

## Editor-owned settings

`Edit > Project Settings...` validates `startup_scene` against the asset registry and updates only that key. The update is written through a temporary file and replacement operation, preserving comments, unknown fields, and field order; a failed replacement leaves the original project file intact.

Game View preferences are not project runtime data. They are stored separately in `ProjectSettings/EditorState.json` with `format_version=1`. The file records the Free, 16:9, 1280x720, 1920x1080, or Custom resolution mode and custom dimensions.
