#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "editor/function/import/import_manager.h" // ImportManager/ImportRequest/ImportResult

namespace Hybrid
{
    class RuntimeResourceSystem;
    class AssetMetaStore;

    class EditorResourceSystem
    {
    public:
        bool initialize(const std::shared_ptr<RuntimeResourceSystem>& runtime_system);

        ImportResult importAsset(const ImportRequest& request);

        // 你已有：示例接口（可保留）
        AssetID importTexture2D(const std::string& source_path,
            const std::string& cooked_path,
            const std::string& hash);

        // ===== 新增：运行中自动扫描导入 =====
        // 在编辑器更新循环里每帧调用一次
        void tickAutoImport(float dt);

        // 可选：调参接口
        void setAutoImportEnabled(bool enabled) { m_autoImportEnabled = enabled; }
        void setAutoImportInterval(float seconds) { m_autoImportInterval = seconds; }

    private:
        bool saveAssetMeta(const AssetMetadata& meta);
        void registerDefaultImporters();

        // ===== 新增：内部实现 =====
        void scanAndAutoImportTexturesOnce();

    private:
        std::shared_ptr<RuntimeResourceSystem> m_runtime;
        std::unique_ptr<AssetMetaStore> m_metaStore;
        std::shared_ptr<ImportManager> m_importManager;

        // ===== 新增：自动扫描状态 =====
        bool  m_autoImportEnabled = true;
        float m_autoImportTimer = 0.0f;
        float m_autoImportInterval = 0.75f; // 建议 0.5~1.0 秒

        // key = source vpath（asset:xxx.png），value = last hash（size+mtime）
        std::unordered_map<std::string, std::string> m_seenTextureHash;
    };
} // namespace Hybrid
