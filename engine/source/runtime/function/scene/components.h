#pragma once

#include <cstdint>
#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/function/asset/asset_type.h"

namespace Hybrid
{
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
        glm::vec3 Position{0.0f, 0.0f, 0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 Scale{1.0f, 1.0f, 1.0f};

        // Intrusive hierarchy links.
        entt::entity Parent = entt::null;
        entt::entity FirstChild = entt::null;
        entt::entity NextSibling = entt::null;
        entt::entity PrevSibling = entt::null;

        // Cached matrices.
        glm::mat4 LocalMatrix{1.0f};
        glm::mat4 WorldMatrix{1.0f};
        bool DirtyLocal = true;
        bool DirtyWorld = true;
    };

    struct CameraComponent
    {
        bool Primary = false;

        float FovY = 45.0f;
        float Near = 0.1f;
        float Far = 1000.0f;
    };

    struct MeshRendererComponent
    {
        AssetID Mesh{};
        AssetID Material{};

        int Primitive = 0;
        glm::vec4 Tint{1.0f};
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
    };

    struct PointLightComponent
    {
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Range = 10.0f;
    };
} // namespace Hybrid
