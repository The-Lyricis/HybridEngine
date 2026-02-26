#pragma once
#include <memory>
#include <glm/vec4.hpp>

#include "asset_type.h"

namespace Hybrid
{
    struct MaterialData
    {
        glm::vec4 albedo_color{1.0f};
        float metallic  = 0.0f;
        float roughness = 1.0f;
        float ao        = 1.0f;
        float emissive  = 0.0f;

        AssetID albedo_map{};
        AssetID normal_map{};
        AssetID metallic_roughness_map{};
        AssetID ao_map{};
        AssetID emissive_map{};
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
