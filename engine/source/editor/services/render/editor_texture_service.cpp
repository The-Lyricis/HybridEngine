#include "editor_texture_service.h"

#include <stb_image.h>

#include "editor/services/render/imgui_render_backend.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/render/rhi/render_device.h"

namespace Hybrid
{
    namespace
    {
        constexpr const char* kEditorTextureServiceLogTag = "[EditorTextureService]";
    }

    EditorTextureService::EditorTextureService(std::shared_ptr<IImGuiRenderBackend> backend)
        : m_backend(std::move(backend))
    {
    }

    EditorTextureService::~EditorTextureService()
    {
        shutdown();
    }

    EditorImageHandle EditorTextureService::loadRgba8(const std::filesystem::path& path)
    {
        const std::string key = path.lexically_normal().generic_string();
        const auto cached = m_path_cache.find(key);
        if (cached != m_path_cache.end())
            return cached->second;
        if (!m_backend)
            return {};

        int width = 0;
        int height = 0;
        int components = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &components, 4);
        if (!pixels || width <= 0 || height <= 0)
        {
            if (pixels)
                stbi_image_free(pixels);
            HBD_CORE_WARN("{} load_failed path={}", kEditorTextureServiceLogTag, key);
            m_path_cache.emplace(key, EditorImageHandle{});
            return {};
        }

        TextureDesc desc{};
        desc.type = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = static_cast<uint32_t>(width);
        desc.height = static_cast<uint32_t>(height);
        TexturePtr texture = Texture::Create(desc, pixels,
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        stbi_image_free(pixels);
        if (!texture)
        {
            HBD_CORE_WARN("{} create_failed path={}", kEditorTextureServiceLogTag, key);
            m_path_cache.emplace(key, EditorImageHandle{});
            return {};
        }

        std::string error;
        const uint64_t backend_id = m_backend->registerTexture(texture, error);
        if (backend_id == 0)
        {
            HBD_CORE_WARN("{} register_failed path={} reason={}",
                          kEditorTextureServiceLogTag, key, error);
            m_path_cache.emplace(key, EditorImageHandle{});
            return {};
        }

        uint32_t index = static_cast<uint32_t>(m_slots.size());
        for (uint32_t candidate = 0; candidate < m_slots.size(); ++candidate)
        {
            if (!m_slots[candidate].alive)
            {
                index = candidate;
                break;
            }
        }
        if (index == m_slots.size())
            m_slots.emplace_back();
        Slot& slot = m_slots[index];
        slot.texture = std::move(texture);
        slot.backend_id = backend_id;
        slot.alive = true;
        const EditorImageHandle handle{index, slot.generation};
        m_path_cache.emplace(key, handle);
        return handle;
    }

    EditorImageHandle EditorTextureService::registerTextureView(IRenderDevice& device,
                                                                TextureViewHandle view)
    {
        if (!view || !m_backend)
            return {};

        const uint64_t key = (static_cast<uint64_t>(view.generation) << 32u) |
                             static_cast<uint64_t>(view.index);
        const auto cached = m_view_cache.find(key);
        if (cached != m_view_cache.end())
            return cached->second;

        std::string error;
        const uint64_t backend_id = m_backend->registerTextureView(device, view, error);
        if (backend_id == 0)
        {
            HBD_CORE_WARN("{} view_register_failed index={} generation={} reason={}",
                          kEditorTextureServiceLogTag, view.index, view.generation, error);
            return {};
        }

        uint32_t index = static_cast<uint32_t>(m_slots.size());
        for (uint32_t candidate = 0; candidate < m_slots.size(); ++candidate)
        {
            if (!m_slots[candidate].alive)
            {
                index = candidate;
                break;
            }
        }
        if (index == m_slots.size())
            m_slots.emplace_back();
        Slot& slot = m_slots[index];
        slot.texture.reset();
        slot.backend_id = backend_id;
        slot.alive = true;
        const EditorImageHandle handle{index, slot.generation};
        m_view_cache.emplace(key, handle);
        return handle;
    }

    ImTextureID EditorTextureService::imageId(EditorImageHandle handle) const
    {
        if (!handle || handle.index >= m_slots.size())
            return {};
        const Slot& slot = m_slots[handle.index];
        if (!slot.alive || slot.generation != handle.generation)
            return {};
        return static_cast<ImTextureID>(slot.backend_id);
    }

    uint32_t EditorTextureService::maxTextureDimension2D() const
    {
        return m_backend ? m_backend->maxTextureDimension2D() : 1u;
    }

    void EditorTextureService::shutdown()
    {
        for (Slot& slot : m_slots)
        {
            if (!slot.alive)
                continue;
            if (m_backend)
                m_backend->unregisterTexture(slot.backend_id);
            slot.texture.reset();
            slot.backend_id = 0;
            slot.alive = false;
            ++slot.generation;
            if (slot.generation == 0)
                slot.generation = 1;
        }
        m_path_cache.clear();
        m_view_cache.clear();
    }
} // namespace Hybrid
