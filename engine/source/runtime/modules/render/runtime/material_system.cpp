#include "material_system.h"

#include <string>
#include <vector>

#include "runtime/core/base/macro.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/render_bindings.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kLogTag = "[MaterialSystem]";

        MaterialSystem::MaterialTemplateDesc buildTemplate(const MaterialData& data)
        {
            MaterialSystem::MaterialTemplateDesc desc{};
            desc.workflow = data.workflow;
            desc.alpha_mode = data.alpha_mode;
            desc.double_sided = data.double_sided;
            desc.depth_write = data.alpha_mode != MaterialAlphaMode::Blend;
            desc.casts_shadow = data.alpha_mode != MaterialAlphaMode::Blend;
            return desc;
        }

        MaterialSystem::MaterialInstanceDesc buildInstance(const MaterialData& data)
        {
            MaterialSystem::MaterialInstanceDesc desc{};
            desc.material_template = buildTemplate(data);
            desc.parameters.base_color_factor = data.base_color_factor;
            desc.parameters.metallic_factor = data.metallic_factor;
            desc.parameters.roughness_factor = data.roughness_factor;
            desc.parameters.occlusion_strength = data.occlusion_strength;
            desc.parameters.emissive_factor = data.emissive_factor;
            desc.parameters.has_normal_map = data.normal_texture.texture.value != 0 ? 1 : 0;
            desc.parameters.alpha_mode = static_cast<int>(data.alpha_mode);
            desc.parameters.alpha_cutoff = data.alpha_cutoff;
            desc.textures.base_color = data.base_color_texture.texture;
            desc.textures.normal = data.normal_texture.texture;
            desc.textures.metallic_roughness = data.metallic_roughness_texture.texture;
            desc.textures.occlusion = data.occlusion_texture.texture;
            desc.textures.emissive = data.emissive_texture.texture;
            return desc;
        }

        bool bindTexture(ICommandList& commands, TextureViewHandle view,
                         SamplerHandle sampler, uint32_t binding)
        {
            return static_cast<bool>(commands.bindTexture(
                view, sampler, {RenderBindings::kSceneMaterialSet, binding}));
        }
    } // namespace

    bool MaterialSystem::MaterialGPU::bindBaseColor(ICommandList& commands) const
    {
        return bindTexture(commands, albedo, sampler, RenderBindings::kSceneBaseColorBinding);
    }

    bool MaterialSystem::MaterialGPU::bindTextures(ICommandList& commands) const
    {
        return bindBaseColor(commands) &&
               bindTexture(commands, normal, sampler, RenderBindings::kSceneNormalBinding) &&
               bindTexture(commands, mr, sampler, RenderBindings::kSceneMRBinding) &&
               bindTexture(commands, ao, sampler, RenderBindings::kSceneAOBinding) &&
               bindTexture(commands, emissive, sampler, RenderBindings::kSceneEmissiveBinding);
    }

    void MaterialSystem::setResources(std::shared_ptr<AssetManager> asset_manager, IRenderDevice& device)
    {
        shutdown();
        m_AssetManager = std::move(asset_manager);
        m_Device = &device;
    }

    void MaterialSystem::destroyTexture(TextureResource& resource)
    {
        if (m_Device)
        {
            if (resource.view) (void)m_Device->destroyTextureView(resource.view);
            if (resource.texture) (void)m_Device->destroyTexture(resource.texture);
        }
        resource = {};
    }

    void MaterialSystem::shutdown()
    {
        m_MaterialCache.clear();
        for (auto& entry : m_TextureCache)
            destroyTexture(entry.second);
        m_TextureCache.clear();
        destroyTexture(m_DefaultAlbedoTex);
        destroyTexture(m_DefaultNormalTex);
        destroyTexture(m_DefaultMRTex);
        destroyTexture(m_DefaultAOTex);
        destroyTexture(m_DefaultEmissiveTex);
        if (m_Device && m_Sampler) (void)m_Device->destroySampler(m_Sampler);
        m_Sampler = {};
        m_AssetManager.reset();
        m_Device = nullptr;
    }

    MaterialSystem::TextureResource MaterialSystem::createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!m_Device) return {};
        const uint8_t pixel[4] = {r, g, b, a};
        RhiTextureDesc desc{};
        desc.format = RhiFormat::RGBA8Unorm;
        desc.debug_name = "Material.Default";
        const auto texture = m_Device->createTexture(desc, pixel, sizeof(pixel));
        if (!texture)
        {
            HBD_CORE_ERROR("{} default_texture_failed reason={}", kLogTag, texture.error.message);
            return {};
        }
        const auto view = m_Device->createTextureView(texture.value);
        if (!view)
        {
            (void)m_Device->destroyTexture(texture.value);
            HBD_CORE_ERROR("{} default_texture_view_failed reason={}", kLogTag, view.error.message);
            return {};
        }
        return {texture.value, view.value};
    }

    bool MaterialSystem::ensureDefaultTextures()
    {
        if (!m_Device) return false;
        if (!m_Sampler)
        {
            SamplerDesc desc{};
            desc.linear_filter = true;
            desc.clamp_to_edge = false;
            desc.debug_name = "Material.Sampler";
            const auto sampler = m_Device->createSampler(desc);
            if (!sampler)
            {
                HBD_CORE_ERROR("{} sampler_failed reason={}", kLogTag, sampler.error.message);
                return false;
            }
            m_Sampler = sampler.value;
        }
        if (!m_DefaultAlbedoTex.view) m_DefaultAlbedoTex = createSolidColorTexture(255, 255, 255, 255);
        if (!m_DefaultNormalTex.view) m_DefaultNormalTex = createSolidColorTexture(128, 128, 255, 255);
        if (!m_DefaultMRTex.view) m_DefaultMRTex = createSolidColorTexture(255, 255, 0, 255);
        if (!m_DefaultAOTex.view) m_DefaultAOTex = createSolidColorTexture(255, 255, 255, 255);
        if (!m_DefaultEmissiveTex.view) m_DefaultEmissiveTex = createSolidColorTexture(0, 0, 0, 255);
        return m_DefaultAlbedoTex.view && m_DefaultNormalTex.view && m_DefaultMRTex.view &&
               m_DefaultAOTex.view && m_DefaultEmissiveTex.view;
    }

    TextureViewHandle MaterialSystem::getOrCreateTexture(AssetID texture_id)
    {
        if (!texture_id.value || !m_AssetManager || !m_Device) return {};
        if (const auto it = m_TextureCache.find(texture_id); it != m_TextureCache.end())
            return it->second.view;

        const auto image = m_AssetManager->loadSync<TextureImageData>(texture_id);
        if (!image || !image->isValid()) return {};
        if (image->format != TextureFormat::RGB8 && image->format != TextureFormat::RGBA8) return {};
        const size_t pixel_count = static_cast<size_t>(image->width) * image->height;
        const size_t source_channels = image->format == TextureFormat::RGB8 ? 3 : 4;
        if (image->pixels.size() < pixel_count * source_channels) return {};
        std::vector<uint8_t> converted;
        const uint8_t* bytes = image->pixels.data();
        if (source_channels == 3)
        {
            converted.resize(pixel_count * 4);
            for (size_t i = 0; i < pixel_count; ++i)
            {
                converted[i * 4] = bytes[i * 3];
                converted[i * 4 + 1] = bytes[i * 3 + 1];
                converted[i * 4 + 2] = bytes[i * 3 + 2];
                converted[i * 4 + 3] = 255;
            }
            bytes = converted.data();
        }
        RhiTextureDesc desc{};
        desc.width = image->width;
        desc.height = image->height;
        desc.format = image->srgb ? RhiFormat::RGBA8Srgb : RhiFormat::RGBA8Unorm;
        desc.debug_name = "Material.Texture";
        const auto texture = m_Device->createTexture(desc, bytes, pixel_count * 4);
        if (!texture)
        {
            HBD_CORE_ERROR("{} texture_upload_failed asset={} reason={}", kLogTag,
                           texture_id.value, texture.error.message);
            return {};
        }
        const auto view = m_Device->createTextureView(texture.value);
        if (!view)
        {
            (void)m_Device->destroyTexture(texture.value);
            HBD_CORE_ERROR("{} texture_view_failed asset={} reason={}", kLogTag,
                           texture_id.value, view.error.message);
            return {};
        }
        m_TextureCache.emplace(texture_id, TextureResource{texture.value, view.value});
        return view.value;
    }

    MaterialSystem::MaterialGPU* MaterialSystem::getOrCreate(
        AssetID material_id, const std::shared_ptr<Material>& material)
    {
        if (!material || !ensureDefaultTextures()) return nullptr;
        if (const auto it = m_MaterialCache.find(material_id); it != m_MaterialCache.end())
            return it->second.get();
        auto gpu = std::make_shared<MaterialGPU>();
        gpu->instance = buildInstance(material->getData());
        const auto textureOrDefault = [this](AssetID id, TextureViewHandle fallback)
        {
            const TextureViewHandle loaded = getOrCreateTexture(id);
            return loaded ? loaded : fallback;
        };
        gpu->albedo = textureOrDefault(gpu->instance.textures.base_color, m_DefaultAlbedoTex.view);
        gpu->normal = textureOrDefault(gpu->instance.textures.normal, m_DefaultNormalTex.view);
        gpu->mr = textureOrDefault(gpu->instance.textures.metallic_roughness, m_DefaultMRTex.view);
        gpu->ao = textureOrDefault(gpu->instance.textures.occlusion, m_DefaultAOTex.view);
        gpu->emissive = textureOrDefault(gpu->instance.textures.emissive, m_DefaultEmissiveTex.view);
        gpu->sampler = m_Sampler;
        MaterialGPU* result = gpu.get();
        m_MaterialCache[material_id] = std::move(gpu);
        return result;
    }

    void MaterialSystem::invalidateMaterial(AssetID material_id)
    {
        if (material_id.value) m_MaterialCache.erase(material_id);
    }

    void MaterialSystem::invalidateTexture(AssetID texture_id)
    {
        if (!texture_id.value) return;
        m_MaterialCache.clear();
        if (const auto it = m_TextureCache.find(texture_id); it != m_TextureCache.end())
        {
            destroyTexture(it->second);
            m_TextureCache.erase(it);
        }
    }

    void MaterialSystem::invalidateAll()
    {
        m_MaterialCache.clear();
    }
} // namespace Hybrid
