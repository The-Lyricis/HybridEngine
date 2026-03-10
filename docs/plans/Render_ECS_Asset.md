# 下一步计划

## 总体方向

以“资源资产 + ECS 组件 + 渲染管线”三层来扩展：先定义资产类型（Mesh/Material），再给实体挂组件（MeshRenderer/Light），最后改 RenderSystem 收集并下发到 GPU。

先做“小而可跑”的版本：Blinn-Phong 或简化金属度/粗糙度 PBR，支持 1 个平行光 + 若干点光，网格/材质走 AssetManager 的加载与缓存。后续再逐步丰富。

## 资产层设计

Mesh 资产：顶点格式 pos/normal/tangent/uv，可含多个 submesh（索引范围 + 绑定材质 ID），附带包围盒球做 culling。文件优先支持 glTF，临时可写一个简单 OBJ loader。

Material 资产：字段 albedoColor/metallic/roughness/ao/emissive + 贴图句柄（可为空，用默认纹理）。添加 AssetType::Mesh / AssetType::Material，写对应 Loader，并在 ResourceSystem::registerDefaultLoaders 注册。

GPU 缓存：在 RenderSystem 维护 MeshGPUCache（VAO/VBO/IBO）和 MaterialGPUCache（纹理绑定、UBO 数据）。用 AssetID 作为 key，避免重复创建。

## ECS 组件

MeshRenderer 组件扩展：

AssetID Mesh; AssetID Material; int SubmeshIndex = 0;

glm::vec4 OverrideTint（可选覆盖）。

光照组件：

DirectionalLight { vec3 color; float intensity; }

PointLight { vec3 color; float intensity; float range; }

可后续加 SpotLight { inner/outer angle }。

继续复用已有 Transform / Camera；渲染时用 Transform 生成 model 矩阵。

## 渲染管线最小实现

帧级 UBO：视图/投影矩阵、相机位置、时间等。

光照 UBO：固定上限，如 MAX_DIR_LIGHTS=1, MAX_POINT_LIGHTS=16，不足部分填 0。

材质绑定：采样器绑定（albedo/normal/metalRough/AO/emissive），无贴图用默认白/中灰。

绘制遍历：

从 Scene registry 按组件 Transform + MeshRenderer 收集，过滤缺失资源的实体。

按 Shader/Material 分组以减少切换（初期可以不分组，先跑通）。

为每个 renderable 设置 model 矩阵，提交 VAO & index count。

Shader：先做一套基础 shader：

Vertex：u_ViewProj * u_Model，输出世界空间 position/normal/uv。

Fragment：Lambert + Blinn-Phong 或简化 PBR（金属度/粗糙度）。支持一盏平行光、可选多点光。

## 与现有结构的衔接

RenderSystem 里新增：

uploadFrameData(...)、uploadLights(...)、drawRenderable(...) 等私有函数。

初始化阶段创建基础 PBR/Phong shader 和默认材质。

资源系统：在初始化时挂载 Mesh/Material loader，像现有 Texture 一样 setDefault。

Event & Editor：EditorUI 后续可加材质/光源面板，不影响首轮实现。

## 实现顺序（建议三小步）

资产与组件：定义 Mesh/Material 资产类型 + Loader stub，扩展 MeshRenderer 组件与默认材质。

光源与 UBO：新增光照组件与 RenderSystem 中的光数据收集/上传，Shader 支持 1 个平行光 + N 个点光。

Mesh 渲染替换立方体：加载一个 Mesh 资产（或内置立方体 mesh）走新的提交流程，验证帧通路和窗口缩放后的尺寸同步。

按这个路线走，可以先把 cube 渲染切换到“Mesh+Material+光照”路径，再逐步接入实际模型文件与编辑器面板。
