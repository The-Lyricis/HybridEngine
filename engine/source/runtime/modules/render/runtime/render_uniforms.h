#pragma once

#include <array>
#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Hybrid::RenderUniforms
{
    inline constexpr int kMaxPointLights = 16;

    inline constexpr uint32_t kFrameUBOBinding = 0;
    inline constexpr uint32_t kLightUBOBinding = 1;

    inline constexpr const char* kFrameBlockName = "FrameBlock";
    inline constexpr const char* kLightBlockName = "LightBlock";

    struct alignas(16) FrameUBOData
    {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
        glm::mat4 viewProj{1.0f};
        glm::vec4 cameraPos{0.0f};
        glm::vec4 viewport{0.0f};
    };

    struct alignas(16) DirLightUBOData
    {
        glm::vec4 colorIntensity{0.0f};
        glm::vec4 direction{0.0f};
    };

    struct alignas(16) PointLightUBOData
    {
        glm::vec4 colorIntensity{0.0f};
        glm::vec4 positionRange{0.0f};
    };

    struct alignas(16) LightUBOData
    {
        DirLightUBOData dirLight{};
        std::array<PointLightUBOData, kMaxPointLights> pointLights{};
        glm::ivec4 counts{0};
    };

    static_assert(sizeof(FrameUBOData) == 224, "FrameBlock std140 layout mismatch");
    static_assert(sizeof(DirLightUBOData) == 32, "DirLight std140 layout mismatch");
    static_assert(sizeof(PointLightUBOData) == 32, "PointLight std140 layout mismatch");
    static_assert(sizeof(LightUBOData) == 560, "LightBlock std140 layout mismatch");
} // namespace Hybrid::RenderUniforms
