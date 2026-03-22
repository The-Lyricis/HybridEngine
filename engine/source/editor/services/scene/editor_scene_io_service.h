#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "editor/core/engine_services.h"
#include "editor/core/scene_document.h"
#include "editor/framework/camera/editor_camera.h"

namespace Hybrid
{
    struct OpenSceneOptions
    {
        bool remember_last_opened = true;
    };

    class EditorSceneIOService
    {
    public:
        explicit EditorSceneIOService(EngineServices services = {});

        void initialize();
        void shutdown();

        const std::shared_ptr<SceneDocument>& getActiveDocument() const { return m_active_document; }
        const std::string& getStatusMessage() const { return m_status_message; }
        void clearStatusMessage();

        bool open(const std::string& scene_vpath, OpenSceneOptions options = {});
        bool requestOpen();
        bool requestSave();
        bool requestSaveAs();
        bool restoreStartupScene();
        bool createUntitled(const char* reason = nullptr);
        bool saveSceneViewState(const SceneDocument& document, const EditorCameraState& camera_state) const;
        bool loadSceneViewState(const SceneDocument& document, EditorCameraState& out_camera_state) const;

    private:
        bool activateDocument(std::shared_ptr<SceneDocument> document);
        bool saveToVPath(const std::string& scene_vpath);
        std::shared_ptr<SceneDocument> loadDocumentFromVPath(const std::string& scene_vpath);
        bool chooseOpenVPath(std::string& out_vpath);
        bool chooseSaveAsVPath(std::string& out_vpath);
        bool tryOpenProjectDefaultScene();
        bool tryOpenScannedScene();
        void saveLastOpenedScene(const std::string& scene_vpath) const;
        std::string loadLastOpenedScene() const;
        bool toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const;
        std::filesystem::path getSceneViewStatePath(const SceneDocument& document) const;
        std::string getSceneViewStateKey(const SceneDocument& document) const;

    private:
        EngineServices m_services{};
        std::filesystem::path m_assets_root;
        std::shared_ptr<SceneDocument> m_active_document;
        std::string m_status_message;
    };
} // namespace Hybrid
