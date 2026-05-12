#pragma once

#include <memory>
#include <unordered_map>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/texture_image.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/public/texture_uploader.h"

namespace Hybrid
{
    class Shader;

    class MaterialSystem
    {
    public:
        struct MaterialParameterBlock
        {
            glm::vec4 base_color_factor{1.0f};
            float metallic_factor = 0.0f;
            float roughness_factor = 1.0f;
            float occlusion_strength = 1.0f;
            glm::vec3 emissive_factor{0.0f};
            int has_normal_map = 0;
            int alpha_mode = 0;
            float alpha_cutoff = 0.5f;
        };

        struct MaterialTemplateDesc
        {
            MaterialWorkflow workflow = MaterialWorkflow::MetallicRoughness;
            MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
            bool double_sided = false;
            bool depth_write = true;
            bool casts_shadow = true;
        };

        struct MaterialTextureBindingSet
        {
            AssetID base_color{};
            AssetID normal{};
            AssetID metallic_roughness{};
            AssetID occlusion{};
            AssetID emissive{};
        };

        struct MaterialInstanceDesc
        {
            MaterialTemplateDesc material_template;
            MaterialParameterBlock parameters;
            MaterialTextureBindingSet textures;
        };

        struct MaterialGPU
        {
            MaterialInstanceDesc instance;
            TexturePtr albedo;
            TexturePtr normal;
            TexturePtr mr;
            TexturePtr ao;
            TexturePtr emissive;

            MaterialAlphaMode alphaMode() const { return instance.material_template.alpha_mode; }
            bool castsShadow() const { return instance.material_template.casts_shadow; }
            void bind(Shader& shader) const;
        };

    public:
        void initialize(std::shared_ptr<AssetManager> asset_manager);
        void shutdown();
        void setAssetManager(std::shared_ptr<AssetManager> asset_manager);

        MaterialGPU* getOrCreate(AssetID material_id, const std::shared_ptr<Material>& material);

        void invalidateMaterial(AssetID material_id);
        void invalidateTexture(AssetID texture_id);
        void invalidateAll();

    private:
        TexturePtr getOrCreateTexture(AssetID texture_id);
        void ensureDefaultTextures();
        TexturePtr createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    private:
        std::shared_ptr<AssetManager> m_AssetManager;
        std::unordered_map<AssetID, std::shared_ptr<MaterialGPU>, AssetID::Hasher> m_MaterialCache;
        std::unordered_map<AssetID, TexturePtr, AssetID::Hasher> m_TextureCache;
        std::unique_ptr<TextureUploader> m_TextureUploader;

        TexturePtr m_DefaultAlbedoTex;
        TexturePtr m_DefaultNormalTex;
        TexturePtr m_DefaultMRTex;
        TexturePtr m_DefaultAOTex;
        TexturePtr m_DefaultEmissiveTex;
        bool m_Initialized = false;
    };
} // namespace Hybrid
