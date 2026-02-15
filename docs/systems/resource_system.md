# Resource System 说明

更新：2026-02-15  
适用范围：`engine/source/runtime/function/asset`，`runtime/core/base/virtual_file_system.*`，`runtime/function/render/texture*`。

## 目录与依赖
- 核心代码：`function/asset/*`（AssetManager/Registry/Loader/ResourceSystem）、`core/base/virtual_file_system.*`
- 渲染依赖：OpenGL（默认 Loader）、stb_image（纹理解码）
- 宏：`HYBRID_PROJECT_ROOT_DIR`（源码根），可选 `HYBRID_ROOT_DIR`（bin/安装根）

## 核心职责
- **VFS**：`IVirtualFileSystem` + `NativeFileSystem`，逻辑路径格式 `alias:/relative`，多挂载点优先级。
- **AssetRegistry**：AssetID/元数据管理，路径规范化（逻辑路径或 root 相对），随机 64-bit ID 去重。
- **AssetManager**：
  - 状态机：Unloaded/Loading/Loaded/Failed
  - 缓存 + in-flight 合并
  - 默认资源回退（按 type_index）
  - 纹理异步加载禁用（GPU 资源 async 直接回退默认）
- **Loader**：当前实现 `GLTexture2DLoader`（stb 解码、GL 上下文检查、强制 RGBA）。
- **ResourceSystem**：组合 VFS/Registry/Manager，挂载资源根；注册默认 Loader；创建 1x1 白纹理并设为默认 Texture。

## 初始化流程
1) `ResourceSystem::initialize()`  
   - 依次尝试 `HYBRID_ROOT_DIR/asset`、`HYBRID_PROJECT_ROOT_DIR/engine/asset`，首个存在即挂载为 `asset:/`。  
   - 创建 `AssetRegistry`（root=asset root）。  
   - 创建 `AssetManager` 并注册默认 Loader。  
   - 创建 1x1 白纹理（通过 `Texture::Create` 工厂）并设为默认 Texture。
2) Loader 注册：OpenGL 2D 纹理。

## 加载流程（Texture 示例）
```
loadSync<Texture>(id):
  - 查缓存 -> in-flight -> 状态置 Loading
  - 拷贝元数据 -> 调用 Loader
  - 写回状态/缓存；失败则回退默认纹理

loadAsync<Texture>(id):
  - 对 GPU 资源直接警告并返回默认纹理（禁用后台创建）
```

## 默认资源
- 1x1 RGBA 白纹理（运行时创建，GL 实现）。  
- AssetManager 默认表：加载失败或 async 被禁用时回退。

## 注意事项 / 待办
- **stb 实现**：确保有单独 `.cpp` 定义 `STB_IMAGE_IMPLEMENTATION` 参与 HybridRuntime 构建。
- **GL 上下文**：Loader/析构仍要求在持有 GL 上下文的线程；尚无 GPU 回收队列。
- **Cube/Array**：`GLTexture::Create` 仅完整实现 2D，其他类型待补数据上传。
- **硬依赖/热重载**：尚未实现文件监听、硬依赖加载、默认 error 资源。
- **路径要求**：元数据的 `source_path/cooked_path` 必须是逻辑路径 `alias:/relative`。

## 使用摘记
- 注册 Loader：ResourceSystem 内已默认注册 `GLTexture2DLoader`。
- 设置默认：`AssetManager::setDefault<T>(ptr)`，回退通过 type_index 匹配。
- VFS 解析：若相对部分以 `/` 或 `\` 开头会被剥离；alias 未找到返回 nullopt。
