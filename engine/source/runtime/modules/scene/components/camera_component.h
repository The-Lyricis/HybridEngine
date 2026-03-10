#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

namespace Hybrid
{
    enum class CameraClearMode : uint8_t
    {
        SolidColor = 0,
        Skybox
    };

    struct CameraComponent
    {
        bool Primary = false;

        float FovY = 45.0f;
        float Near = 0.1f;
        float Far = 1000.0f;
        CameraClearMode ClearMode = CameraClearMode::SolidColor;
        glm::vec4 ClearColor{0.1f, 0.1f, 0.12f, 1.0f};
    };
} // namespace Hybrid
