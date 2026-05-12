#include "material_system.h"

#include <string>

#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/runtime/render_binding_layout.h"
#include "runtime/modules/render/runtime/render_bindings.h"

namespace Hybrid
{
    namespace
    {
        MaterialSystem::MaterialTemplateDesc BuildMaterialTemplateDesc(const MaterialData& data)
        {
            MaterialSystem::MaterialTemplateDesc desc{};
            desc.workflow = data.workflow;
            desc.alpha_mode = data.alpha_mode;
            desc.double_sided = data.double_sided;
            desc.depth_write = data.alpha_mode != MaterialAlphaMode::Blend;
            desc.casts_shadow = data.alpha_mode != MaterialAlphaMode::Blend;
            return desc;
        }

        MaterialSystem::MaterialTextureBindingSet BuildMaterialTextureBindingSet(const MaterialData& data)
        {
            MaterialSystem::MaterialTextureBindingSet textures{};
            textures.base_color = data.base_color_texture.texture;
            textures.normal = data.normal_texture.texture;
            textures.metallic_roughness = data.metallic_roughness_texture.texture;
            textures.occlusion = data.occlusion_texture.texture;
            textures.emissive = data.emissive_texture.texture;
            return textures;
        }

        MaterialSystem::MaterialParameterBlock BuildMaterialParameterBlock(const MaterialData& data)
        {
            MaterialSystem::MaterialParameterBlock block{};
            block.base_color_factor = data.base_color_factor;
            block.metallic_factor = data.metallic_factor;
            block.roughness_factor = data.roughness_factor;
            block.occlusion_strength = data.occlusion_strength;
            block.emissive_factor = data.emissive_factor;
            block.has_normal_map = (data.normal_texture.texture.value != 0) ? 1 : 0;
            block.alpha_mode = static_cast<int>(data.alpha_mode);
            block.alpha_cutoff = data.alpha_cutoff;
            return block;
        }

        MaterialSystem::MaterialInstanceDesc BuildMaterialInstanceDesc(const MaterialData& data)
        {
            MaterialSystem::MaterialInstanceDesc desc{};
            desc.material_template = BuildMaterialTemplateDesc(data);
            desc.parameters = BuildMaterialParameterBlock(data);
            desc.textures = BuildMaterialTextureBindingSet(data);
            return desc;
        }

        void BindTextureIfPresent(const TexturePtr& texture,
                                  const RenderBindingLayoutDesc& layout,
                                  std::string_view binding_name)
        {
            if (!texture)
                return;

            const RenderBindingDesc* binding = FindRenderBinding(layout, binding_name);
            if (!binding)
                return;

            texture->bind(binding->slot);
        }
    } // namespace

    void MaterialSystem::MaterialGPU::bind(Shader& shader) const
    {
        const RenderBindingLayoutDesc& layout = GetSceneMaterialBindingLayout();
        const auto set_float = [&shader, &layout](std::string_view name, float value)
        {
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, name))
                shader.setFloat(std::string(binding->name), value);
        };
        const auto set_int = [&shader, &layout](std::string_view name, int value)
        {
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, name))
                shader.setInt(std::string(binding->name), value);
        };
        const auto set_vec4 = [&shader, &layout](std::string_view name, const glm::vec4& value)
        {
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, name))
                shader.setVec4(std::string(binding->name), value);
        };
        const auto set_vec3 = [&shader, &layout](std::string_view name, const glm::vec3& value)
        {
            if (const RenderBindingDesc* binding = FindRenderBinding(layout, name))
                shader.setVec3(std::string(binding->name), value);
        };

        const MaterialParameterBlock& params = instance.parameters;
        set_vec4(RenderBindings::kSceneBaseColorFactorUniform, params.base_color_factor);
        set_float(RenderBindings::kSceneMetallicFactorUniform, params.metallic_factor);
        set_float(RenderBindings::kSceneRoughnessFactorUniform, params.roughness_factor);
        set_float(RenderBindings::kSceneOcclusionStrengthUniform, params.occlusion_strength);
        set_vec3(RenderBindings::kSceneEmissiveFactorUniform, params.emissive_factor);
        set_int(RenderBindings::kSceneHasNormalMapUniform, params.has_normal_map);
        set_int(RenderBindings::kSceneAlphaModeUniform, params.alpha_mode);
        set_float(RenderBindings::kSceneAlphaCutoffUniform, params.alpha_cutoff);

        BindTextureIfPresent(albedo, layout, RenderBindings::kSceneBaseColorTextureUniform);
        BindTextureIfPresent(normal, layout, RenderBindings::kSceneNormalUniform);
        BindTextureIfPresent(mr, layout, RenderBindings::kSceneMetallicRoughnessTextureUniform);
        BindTextureIfPresent(ao, layout, RenderBindings::kSceneOcclusionTextureUniform);
        BindTextureIfPresent(emissive, layout, RenderBindings::kSceneEmissiveTextureUniform);
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
        gpu->instance = BuildMaterialInstanceDesc(material->getData());
        gpu->albedo = texOrDefault(gpu->instance.textures.base_color, m_DefaultAlbedoTex);
        gpu->normal = texOrDefault(gpu->instance.textures.normal, m_DefaultNormalTex);
        gpu->mr = texOrDefault(gpu->instance.textures.metallic_roughness, m_DefaultMRTex);
        gpu->ao = texOrDefault(gpu->instance.textures.occlusion, m_DefaultAOTex);
        gpu->emissive = texOrDefault(gpu->instance.textures.emissive, m_DefaultEmissiveTex);

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
