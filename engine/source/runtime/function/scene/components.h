#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include "runtime/function/asset/asset_type.h"

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
    // 扩展支持资产引用（Mesh/Material），仍保留 primitive 兼容当前渲染路径
    struct MeshRendererComponent
    {
        AssetID Mesh{};         // 资产 ID（未设置时回退到 Primitive 路径）
        AssetID Material{};     // 资产 ID（未设置时使用默认材质）

        int Primitive = 0;      // 向后兼容：0=内建立方体
        glm::vec4 Tint{ 1.0f }; // debug color / 覆盖色
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float     Intensity = 1.0f;
        glm::vec3 Direction{0.0f, -1.0f, 0.0f}; // 指向地面
    };

    struct PointLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float     Intensity = 1.0f;
        float     Range = 10.0f;
    };
}
