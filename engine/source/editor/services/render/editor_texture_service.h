#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "editor/services/render/editor_image_handle.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/rhi/rhi_handles.h"

namespace Hybrid
{
    class IImGuiRenderBackend;
    class IRenderDevice;

    // Owns editor UI textures and backend-specific ImGui registrations. Panels
    // retain only generational EditorImageHandles, never native GPU identifiers.
    class EditorTextureService
    {
    public:
        explicit EditorTextureService(std::shared_ptr<IImGuiRenderBackend> backend);
        ~EditorTextureService();

        EditorImageHandle loadRgba8(const std::filesystem::path& path);
        EditorImageHandle registerTextureView(IRenderDevice& device, TextureViewHandle view);
        ImTextureID imageId(EditorImageHandle handle) const;
        uint32_t maxTextureDimension2D() const;
        void shutdown();

    private:
        struct Slot
        {
            TexturePtr texture;
            uint64_t backend_id = 0;
            uint32_t generation = 1;
            bool alive = false;
        };

        std::shared_ptr<IImGuiRenderBackend> m_backend;
        std::vector<Slot> m_slots;
        std::unordered_map<std::string, EditorImageHandle> m_path_cache;
        std::unordered_map<uint64_t, EditorImageHandle> m_view_cache;
    };
} // namespace Hybrid
