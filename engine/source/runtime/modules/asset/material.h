#pragma once

#include <cstdint>
#include <memory>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "asset_type.h"

namespace Hybrid
{
    enum class MaterialWorkflow : uint8_t
    {
        MetallicRoughness = 0,
    };

    enum class MaterialAlphaMode : uint8_t
    {
        Opaque = 0,
        Mask = 1,
        Blend = 2,
    };

    struct MaterialTextureSlot
    {
        AssetID texture{};
        uint32_t uv_set = 0;
    };

    struct MaterialData
    {
        MaterialWorkflow workflow = MaterialWorkflow::MetallicRoughness;
        MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
        bool double_sided = false;
        float alpha_cutoff = 0.5f;

        glm::vec4 base_color_factor{1.0f};
        MaterialTextureSlot base_color_texture{};

        float metallic_factor = 0.0f;
        float roughness_factor = 1.0f;
        MaterialTextureSlot metallic_roughness_texture{};

        MaterialTextureSlot normal_texture{};
        float normal_scale = 1.0f;

        MaterialTextureSlot occlusion_texture{};
        float occlusion_strength = 1.0f;

        glm::vec3 emissive_factor{0.0f};
        MaterialTextureSlot emissive_texture{};
    };

    class Material
    {
    public:
        Material() = default;
        explicit Material(const MaterialData& data) : m_data(data) {}

        const MaterialData& getData() const { return m_data; }
        void setData(const MaterialData& data) { m_data = data; }

    private:
        MaterialData m_data;
    };

    using MaterialPtr = std::shared_ptr<Material>;

} // namespace Hybrid
