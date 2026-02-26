#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
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
