#include "material_system.h"

#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/runtime/render_bindings.h"

namespace Hybrid
{
    void MaterialSystem::MaterialGPU::bind(Shader& shader) const
    {
        shader.setVec4(RenderBindings::kSceneAlbedoColorUniform, data.albedo_color);
        shader.setFloat(RenderBindings::kSceneMetallicUniform, data.metallic);
        shader.setFloat(RenderBindings::kSceneRoughnessUniform, data.roughness);
        shader.setFloat(RenderBindings::kSceneAOScalarUniform, data.ao);
        shader.setFloat(RenderBindings::kSceneEmissiveScalarUniform, data.emissive);
        shader.setInt(RenderBindings::kSceneHasNormalMapUniform, (data.normal_map.value != 0) ? 1 : 0);

        if (albedo)
            albedo->bind(RenderBindings::kSceneAlbedoSlot);
        if (normal)
            normal->bind(RenderBindings::kSceneNormalSlot);
        if (mr)
            mr->bind(RenderBindings::kSceneMRSlot);
        if (ao)
            ao->bind(RenderBindings::kSceneAOSlot);
        if (emissive)
            emissive->bind(RenderBindings::kSceneEmissiveSlot);
    }

    void MaterialSystem::initialize(std::shared_ptr<AssetManager> asset_manager)
    {
        m_AssetManager = std::move(asset_manager);
        m_TextureUploader = TextureUploader::Create();
        m_Initialized = true;
    }

    void MaterialSystem::shutdown()
    {
        invalidateAll();
        m_TextureCache.clear();
        m_TextureUploader.reset();
        m_AssetManager.reset();
        m_Initialized = false;
    }

    void MaterialSystem::setAssetManager(std::shared_ptr<AssetManager> asset_manager)
    {
        m_AssetManager = std::move(asset_manager);
        m_TextureUploader = TextureUploader::Create();
        m_Initialized = true;
        invalidateAll();
        m_TextureCache.clear();
    }

    MaterialSystem::MaterialGPU* MaterialSystem::getOrCreate(AssetID material_id, const std::shared_ptr<Material>& material)
    {
        if (!material)
            return nullptr;

        if (auto it = m_MaterialCache.find(material_id); it != m_MaterialCache.end())
            return it->second.get();

        ensureDefaultTextures();

        auto texOrDefault = [&](AssetID texture_id, const TexturePtr& fallback) -> TexturePtr
        {
            if (!m_AssetManager || texture_id.value == 0)
                return fallback;

            auto texture = getOrCreateTexture(texture_id);
            return texture ? texture : fallback;
        };

        auto gpu = std::make_shared<MaterialGPU>();
        gpu->data = material->getData();
        gpu->albedo = texOrDefault(gpu->data.albedo_map, m_DefaultAlbedoTex);
        gpu->normal = texOrDefault(gpu->data.normal_map, m_DefaultNormalTex);
        gpu->mr = texOrDefault(gpu->data.metallic_roughness_map, m_DefaultMRTex);
        gpu->ao = texOrDefault(gpu->data.ao_map, m_DefaultAOTex);
        gpu->emissive = texOrDefault(gpu->data.emissive_map, m_DefaultEmissiveTex);

        auto* out = gpu.get();
        m_MaterialCache[material_id] = std::move(gpu);
        return out;
    }

    void MaterialSystem::invalidateMaterial(AssetID material_id)
    {
        if (material_id.value == 0)
            return;
        m_MaterialCache.erase(material_id);
    }

    void MaterialSystem::invalidateTexture(AssetID texture_id)
    {
        if (texture_id.value == 0)
            return;
        m_TextureCache.erase(texture_id);
        invalidateAll();
    }

    void MaterialSystem::invalidateAll()
    {
        m_MaterialCache.clear();
    }

    void MaterialSystem::ensureDefaultTextures()
    {
        if (!m_DefaultAlbedoTex)
            m_DefaultAlbedoTex = createSolidColorTexture(255, 255, 255, 255);
        if (!m_DefaultNormalTex)
            m_DefaultNormalTex = createSolidColorTexture(128, 128, 255, 255);
        if (!m_DefaultMRTex)
            m_DefaultMRTex = createSolidColorTexture(255, 255, 0, 255);
        if (!m_DefaultAOTex)
            m_DefaultAOTex = createSolidColorTexture(255, 255, 255, 255);
        if (!m_DefaultEmissiveTex)
            m_DefaultEmissiveTex = createSolidColorTexture(0, 0, 0, 255);
    }

    TexturePtr MaterialSystem::createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        TextureDesc desc;
        desc.type = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = desc.height = 1;
        desc.layers = 1;
        desc.mipLevels = 1;
        uint8_t data[4] = {r, g, b, a};
        return Texture::Create(desc, data, sizeof(data));
    }

    TexturePtr MaterialSystem::getOrCreateTexture(AssetID texture_id)
    {
        if (texture_id.value == 0 || !m_AssetManager || !m_TextureUploader)
            return nullptr;

        if (auto it = m_TextureCache.find(texture_id); it != m_TextureCache.end())
            return it->second;

        auto image = m_AssetManager->loadSync<TextureImageData>(texture_id);
        if (!image || !image->isValid())
            return nullptr;

        auto texture = m_TextureUploader->uploadTexture2D(*image);
        if (!texture)
            return nullptr;

        m_TextureCache[texture_id] = texture;
        return texture;
    }
} // namespace Hybrid
