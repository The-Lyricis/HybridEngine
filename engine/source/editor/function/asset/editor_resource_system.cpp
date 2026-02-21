#include "editor_resource_system.h"

#include "runtime/core/base/macro.h"
#include "editor/function/import/texture_importer.h"
#include "editor/function/import/mesh_importer.h"
#include "editor/function/import/audio_importer.h"

namespace Hybrid
{
    bool EditorResourceSystem::initialize(const std::shared_ptr<RuntimeResourceSystem>& runtime_system)
    {
        if (!runtime_system)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime resource system is null");
            return false;
        }

        auto registry = runtime_system->getRegistry();
        if (!registry)
        {
            HBD_CORE_ERROR("EditorResourceSystem init failed: runtime registry is null");
            return false;
        }

        m_runtime = runtime_system;
        m_metaStore = std::make_unique<AssetMetaStore>(registry);
        auto vfs = m_runtime->getVFS();
        m_importManager = std::make_shared<ImportManager>(
            registry,
            vfs,
            [this](const AssetMetadata& meta) { return saveAssetMeta(meta); });

        registerDefaultImporters();
        return true;
    }

    ImportResult EditorResourceSystem::importAsset(const ImportRequest& request)
    {
        if (!m_importManager)
        {
            ImportResult out{};
            out.success = false;
            out.message = "EditorResourceSystem: import manager is not initialized";
            return out;
        }
        return m_importManager->importAsset(request);
    }

    bool EditorResourceSystem::saveAssetMeta(const AssetMetadata& meta)
    {
        if (!m_runtime || !m_metaStore)
            return false;

        auto registry = m_runtime->getRegistry();
        if (!registry)
            return false;

        const auto& asset_root = registry->getRoot();
        if (asset_root.empty())
        {
            HBD_CORE_ERROR("Editor save meta failed: asset root is empty");
            return false;
        }

        registry->registerAsset(meta);

        const bool ok = m_metaStore->saveOne(meta, asset_root);
        if (!ok)
        {
            HBD_CORE_ERROR("Editor save meta failed for {}", meta.source_path);
        }
        return ok;
    }

    void EditorResourceSystem::registerDefaultImporters()
    {
        if (!m_importManager)
            return;

        m_importManager->registerImporter(std::make_shared<TextureImporter>());
        m_importManager->registerImporter(std::make_shared<MeshImporter>());
        m_importManager->registerImporter(std::make_shared<AudioImporter>());
    }
} // namespace Hybrid
