#pragma once

#include <memory>
#include <unordered_map>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/texture_image.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class ICommandList;
    class IRenderDevice;

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
            TextureViewHandle albedo;
            TextureViewHandle normal;
            TextureViewHandle mr;
            TextureViewHandle ao;
            TextureViewHandle emissive;
            SamplerHandle sampler;

            MaterialAlphaMode alphaMode() const { return instance.material_template.alpha_mode; }
            bool castsShadow() const { return instance.material_template.casts_shadow; }
            bool bindBaseColor(ICommandList& commands) const;
            bool bindTextures(ICommandList& commands) const;
        };

    public:
        void setResources(std::shared_ptr<AssetManager> asset_manager, IRenderDevice& device);
        void shutdown();

        MaterialGPU* getOrCreate(AssetID material_id, const std::shared_ptr<Material>& material);

        void invalidateMaterial(AssetID material_id);
        void invalidateTexture(AssetID texture_id);
        void invalidateAll();

    private:
        struct TextureResource
        {
            TextureHandle texture;
            TextureViewHandle view;
        };
        TextureViewHandle getOrCreateTexture(AssetID texture_id);
        TextureResource createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void destroyTexture(TextureResource& resource);
        bool ensureDefaultTextures();

    private:
        std::shared_ptr<AssetManager> m_AssetManager;
        IRenderDevice* m_Device = nullptr;
        std::unordered_map<AssetID, std::shared_ptr<MaterialGPU>, AssetID::Hasher> m_MaterialCache;
        std::unordered_map<AssetID, TextureResource, AssetID::Hasher> m_TextureCache;
        TextureResource m_DefaultAlbedoTex;
        TextureResource m_DefaultNormalTex;
        TextureResource m_DefaultMRTex;
        TextureResource m_DefaultAOTex;
        TextureResource m_DefaultEmissiveTex;
        SamplerHandle m_Sampler;
    };
} // namespace Hybrid
