#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/render/runtime/material_system.h"

namespace Hybrid
{
    // Matches the std140 DrawBlock and MaterialBlock used by scene-family passes.
    struct alignas(16) SceneDrawGPU
    {
        glm::mat4 model{1.0f};
        glm::vec4 tint{1.0f};
        glm::uvec4 ids{0u};
    };
    static_assert(sizeof(SceneDrawGPU) == 96);

    struct alignas(16) SceneMaterialGPU
    {
        glm::vec4 base_color{1.0f};
        glm::vec4 surface{0.0f, 1.0f, 1.0f, 0.5f};
        glm::vec4 emissive{0.0f};
        glm::ivec4 flags{0};
    };
    static_assert(sizeof(SceneMaterialGPU) == 64);

    inline SceneMaterialGPU BuildSceneMaterialGPU(const MaterialSystem::MaterialGPU* material)
    {
        SceneMaterialGPU data{};
        if (!material)
            return data;
        const auto& params = material->instance.parameters;
        data.base_color = params.base_color_factor;
        data.surface = {params.metallic_factor, params.roughness_factor,
                        params.occlusion_strength, params.alpha_cutoff};
        data.emissive = {params.emissive_factor, 0.0f};
        data.flags.x = params.alpha_mode;
        data.flags.y = material->instance.material_template.double_sided ? 1 : 0;
        data.flags.z = params.has_normal_map;
        return data;
    }
} // namespace Hybrid
