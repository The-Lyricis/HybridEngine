#include "material_system.h"

#include "runtime/modules/render/public/shader.h"

namespace Hybrid
{
    void MaterialSystem::MaterialGPU::bind(Shader& shader) const
    {
        shader.setVec4("u_AlbedoColor", data.albedo_color);
        shader.setFloat("u_Metallic", data.metallic);
        shader.setFloat("u_Roughness", data.roughness);
        shader.setFloat("u_AO", data.ao);
        shader.setFloat("u_Emissive", data.emissive);
        shader.setInt("u_HasNormalMap", (data.normal_map.value != 0) ? 1 : 0);

        if (albedo)
            albedo->bind(0);
        if (normal)
            normal->bind(1);
        if (mr)
            mr->bind(2);
        if (ao)
            ao->bind(3);
        if (emissive)
            emissive->bind(4);
    }

    void MaterialSystem::initialize(std::shared_ptr<AssetManager> asset_manager)
    {
        m_AssetManager = std::move(asset_manager);
        m_Initialized = true;
    }

    void MaterialSystem::shutdown()
    {
        invalidateAll();
        m_AssetManager.reset();
        m_Initialized = false;
    }

    void MaterialSystem::setAssetManager(std::shared_ptr<AssetManager> asset_manager)
    {
        m_AssetManager = std::move(asset_manager);
        m_Initialized = true;
        invalidateAll();
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

            auto texture = m_AssetManager->loadSync<Texture>(texture_id);
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
} // namespace Hybrid
