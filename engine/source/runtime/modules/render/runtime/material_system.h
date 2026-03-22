#pragma once

#include <memory>
#include <unordered_map>

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
        struct MaterialGPU
        {
            MaterialData data;
            TexturePtr albedo;
            TexturePtr normal;
            TexturePtr mr;
            TexturePtr ao;
            TexturePtr emissive;

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
