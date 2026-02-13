#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Hybrid
{
    // 可选：后续接入真正 UUID 系统（现在先用 uint64_t 占位）
    struct IDComponent
    {
        uint64_t ID = 0;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation{ 0.0f, 0.0f, 0.0f }; // Euler，后续可换 quaternion
        glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };
    };
    struct CameraComponent
    {
        bool Primary = false;

        // 简化：先做透视相机参数
        float FovY = 45.0f;      // degrees
        float Near = 0.1f;
        float Far = 1000.0f;
    };

    // --- Step 2: MeshRenderer ---
    // 先不引入复杂资源系统：只保留“能画什么”的最小标识
    struct MeshRendererComponent
    {
        // 最小可运行方案：用一个 primitive id 或 debug 颜色占位
        // 后续你可以替换为 Ref<Mesh> / Ref<Material> 等。
        int Primitive = 0;           // 0=cube/triangle(由你RenderSystem解释)
        glm::vec4 Tint{ 1.0f };        // debug color
    };
}
