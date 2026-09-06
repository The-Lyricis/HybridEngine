#include "editor_scene_io_service.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "editor/services/asset/editor_resource_system.h"
#include "editor/services/platform/editor_platform_services.h"
#include "editor/services/state/editor_camera_state_serde.h"
#include "runtime/core/base/math_util.h"
#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/runtime_resource_system.h"
#include "runtime/modules/asset/scene_loader.h"
#include "runtime/modules/project/project_context.h"
#include "runtime/modules/render/runtime/render_system.h"
#include "runtime/modules/scene/scene_manager.h"
#include "runtime/modules/scene/scene_serializer.h"
#include "runtime/modules/window/window_system.h"

namespace Hybrid
{
    namespace
    {
        using json = nlohmann::json;
        constexpr const char* kEditorSceneIOLogTag = "[EditorSceneIOService]";

        std::string pathOrPlaceholder(const std::filesystem::path& path)
        {
            return path.empty() ? std::string("<empty>") : path.generic_string();
        }

        std::filesystem::path GetEditorUserSettingsPath()
        {
            const auto& project = ProjectService::Get();
            return project.root / "UserSettings" / "EditorState.json";
        }

        std::filesystem::path GetProjectSettingsPath()
        {
            const auto& project = ProjectService::Get();
            return project.settings / "ProjectSettings.json";
        }

        std::filesystem::path GetSceneStateDirectoryPath()
        {
            const auto& project = ProjectService::Get();
            return project.root / "UserSettings" / "SceneState";
        }

        std::string trimCopy(std::string value)
        {
            auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
            return value;
        }

        void initializeDefaultSceneContent(Scene& scene)
        {
            Entity camera = scene.createCameraEntity("Main Camera", true);
            auto& camera_transform = camera.GetComponent<TransformComponent>();
            auto& camera_component = camera.GetComponent<CameraComponent>();
            camera_transform.Position = glm::vec3(0.0f, 1.5f, 6.0f);
            camera_transform.Rotation = MathUtil::quatFromEulerDegrees(glm::vec3(-12.0f, 180.0f, 0.0f));
            camera_component.ClearMode = CameraClearMode::Skybox;
            camera_component.ClearColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
            camera_component.FovY = 45.0f;
            camera_component.Near = 0.1f;
            camera_component.Far = 1000.0f;

            Entity sun = scene.createEntity("Directional Light");
            auto& sun_transform = sun.GetComponent<TransformComponent>();
            auto& sun_light = sun.AddComponent<DirectionalLightComponent>();
            sun_transform.Rotation = MathUtil::quatFromEulerDegrees(glm::vec3(-50.0f, -30.0f, 0.0f));
            sun_light.Color = glm::vec3(1.0f, 0.98f, 0.95f);
            sun_light.Intensity = 1.0f;
        }

        bool normalizeSceneSaveVPath(const std::string& input, std::string& out_vpath)
        {
            std::string value = trimCopy(input);
            if (value.empty())
                return false;

            for (char& ch : value)
            {
                if (ch == '\\')
                    ch = '/';
            }

            constexpr const char* kAssetPrefix = "asset:";
            if (value.rfind(kAssetPrefix, 0) == 0)
                value.erase(0, std::char_traits<char>::length(kAssetPrefix));

            while (!value.empty() && value.front() == '/')
                value.erase(value.begin());
            if (value.empty())
                return false;

            std::filesystem::path rel_path(value);
            if (rel_path.is_absolute())
                return false;

            if (!rel_path.has_extension())
                rel_path += ".scene";
            else if (rel_path.extension() != ".scene")
                return false;

            rel_path = rel_path.lexically_normal();
            for (const auto& part : rel_path)
            {
                if (part == "..")
                    return false;
            }

            const std::string rel_string = rel_path.generic_string();
            if (rel_string.empty())
                return false;

            out_vpath = std::string(kAssetPrefix) + rel_string;
            return true;
        }

        uint64_t HashSceneKey(std::string_view text)
        {
            uint64_t value = 14695981039346656037ull;
            for (unsigned char ch : text)
            {
                value ^= static_cast<uint64_t>(ch);
                value *= 1099511628211ull;
            }
            return value;
        }

        std::string SanitizeSceneFileStem(std::string value)
        {
            for (char& ch : value)
            {
                const bool ok =
                    (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '-';
                if (!ok)
                    ch = '_';
            }

            if (value.empty())
                value = "Scene";
            return value;
        }
    } // namespace

    EditorSceneIOService::EditorSceneIOService(EngineServices services)
        : m_services(std::move(services))
    {
    }

    void EditorSceneIOService::initialize()
    {
        m_status_message.clear();
        m_active_document.reset();

        std::error_code ec;
        std::filesystem::create_directories(GetSceneStateDirectoryPath(), ec);

        if (m_services.resources && m_services.resources->getRegistry())
            m_assets_root = m_services.resources->getRegistry()->getRoot();
        else
            m_assets_root.clear();

        HBD_CORE_INFO("{} initialize_completed assets_root={}",
                      kEditorSceneIOLogTag,
                      pathOrPlaceholder(m_assets_root));
    }

    void EditorSceneIOService::shutdown()
    {
        m_active_document.reset();
        m_assets_root.clear();
        m_status_message.clear();
        HBD_CORE_INFO("{} shutdown_completed", kEditorSceneIOLogTag);
    }

    void EditorSceneIOService::clearStatusMessage()
    {
        m_status_message.clear();
    }

    bool EditorSceneIOService::activateDocument(std::shared_ptr<SceneDocument> document)
    {
        if (!document || !document->scene)
            return false;

        bool replaced = false;
        if (m_services.set_editor_scene)
        {
            replaced = m_services.set_editor_scene(document->scene);
        }
        else if (m_services.scene && m_services.render)
        {
            m_services.scene->setActiveScene(document->scene);
            m_services.render->setScene(document->scene);
            replaced = (m_services.scene->getActiveScene() == document->scene);
        }

        if (!replaced)
            return false;

        m_active_document = std::move(document);
        return true;
    }

    std::shared_ptr<SceneDocument> EditorSceneIOService::loadDocumentFromVPath(const std::string& scene_vpath)
    {
        if (!m_services.resources)
            return nullptr;

        auto vfs = m_services.resources->getVFS();
        if (!vfs)
            return nullptr;

        auto native = vfs->resolve(scene_vpath);
        if (!native)
            return nullptr;

        const auto registry = m_services.resources ? m_services.resources->getRegistry().get() : nullptr;
        auto scene = SceneLoader::loadFromLogicalPath(scene_vpath, *vfs, registry);
        if (!scene)
            return nullptr;

        scene->setName(native->stem().string());

        auto document = std::make_shared<SceneDocument>();
        document->scene = std::move(scene);
        document->dirty = false;
        if (registry)
        {
            if (const auto meta = registry->findByPath(scene_vpath))
                document->scene_asset_id = meta->id;
        }
        document->vpath = scene_vpath;
        document->native_path = *native;
        document->display_name = native->filename().string();
        return document;
    }

    bool EditorSceneIOService::open(const std::string& scene_vpath, OpenSceneOptions options)
    {
        auto document = loadDocumentFromVPath(scene_vpath);
        if (!document)
        {
            m_status_message = "场景打开失败";
            HBD_CORE_WARN("{} open_failed scene_vpath={} reason=load_document_failed",
                          kEditorSceneIOLogTag,
                          scene_vpath);
            return false;
        }

        if (!activateDocument(document))
        {
            m_status_message = "场景激活失败";
            HBD_CORE_WARN("{} open_failed scene_vpath={} reason=activate_document_failed",
                          kEditorSceneIOLogTag,
                          scene_vpath);
            return false;
        }

        if (options.remember_last_opened)
            saveLastOpenedScene(scene_vpath);

        m_status_message.clear();
        HBD_CORE_INFO("{} open_completed scene_vpath={} display_name={}",
                      kEditorSceneIOLogTag,
                      scene_vpath,
                      document->display_name);
        return true;
    }

    bool EditorSceneIOService::requestOpen()
    {
        std::string selected_vpath;
        if (!chooseOpenVPath(selected_vpath))
            return false;

        return open(selected_vpath);
    }

    bool EditorSceneIOService::requestSave()
    {
        if (!m_active_document || !m_active_document->scene)
        {
            m_status_message = "当前没有可保存的场景";
            return false;
        }

        if (!m_active_document->isSaved())
            return requestSaveAs();

        return saveToVPath(m_active_document->vpath);
    }

    bool EditorSceneIOService::chooseSaveAsVPath(std::string& out_vpath)
    {
        out_vpath.clear();
        if (!m_active_document || !m_active_document->scene)
            return false;

        std::filesystem::path initial_dir = m_assets_root.empty() ? std::filesystem::path{} : (m_assets_root / "Scenes");
        if (!m_active_document->native_path.empty())
            initial_dir = m_active_document->native_path.parent_path();
        else if (!std::filesystem::exists(initial_dir))
            initial_dir = m_assets_root;

        std::string default_name = m_active_document->display_name.empty()
            ? std::string("Untitled.scene")
            : m_active_document->display_name;
        std::filesystem::path default_file = std::filesystem::path(default_name);
        if (default_file.extension() != ".scene")
            default_file.replace_extension(".scene");

        if (!m_services.platform)
        {
            m_status_message = "Platform file dialog service is unavailable";
            return false;
        }

        SaveFileDialogDesc dialog_desc{};
        dialog_desc.title = "Save Scene";
        dialog_desc.initial_dir = initial_dir;
        dialog_desc.default_name = default_file.string();
        dialog_desc.default_extension = "scene";
        dialog_desc.filters = {
            FileDialogFilter{"Scene Files", "*.scene"},
            FileDialogFilter{"All Files", "*.*"},
        };

        auto selected_path = m_services.platform->showSaveFileDialog(
            m_services.window ? m_services.window->getNativeWindow() : nullptr,
            dialog_desc);
        if (!selected_path)
        {
            m_status_message.clear();
            return false;
        }

        if (!toAssetVPath(*selected_path, out_vpath))
        {
            m_status_message = "保存路径必须位于 Assets 目录下";
            return false;
        }

        return true;
    }

    bool EditorSceneIOService::chooseOpenVPath(std::string& out_vpath)
    {
        out_vpath.clear();

        std::filesystem::path initial_dir = m_assets_root.empty() ? std::filesystem::path{} : (m_assets_root / "Scenes");
        if (m_active_document && !m_active_document->native_path.empty())
            initial_dir = m_active_document->native_path.parent_path();
        else if (!std::filesystem::exists(initial_dir))
            initial_dir = m_assets_root;

        if (!m_services.platform)
        {
            m_status_message = "Platform file dialog service is unavailable";
            return false;
        }

        OpenFileDialogDesc dialog_desc{};
        dialog_desc.title = "Open Scene";
        dialog_desc.initial_dir = initial_dir;
        dialog_desc.filters = {
            FileDialogFilter{"Scene Files", "*.scene"},
            FileDialogFilter{"All Files", "*.*"},
        };
        dialog_desc.allow_multi_select = false;

        const auto selected_paths = m_services.platform->showOpenFileDialog(
            m_services.window ? m_services.window->getNativeWindow() : nullptr,
            dialog_desc);
        if (selected_paths.empty())
        {
            m_status_message.clear();
            return false;
        }

        const std::filesystem::path& selected_path = selected_paths.front();
        if (selected_path.extension() != ".scene")
        {
            m_status_message = "Selected file must use the .scene extension";
            return false;
        }

        if (!toAssetVPath(selected_path, out_vpath))
        {
            m_status_message = "Selected scene must be located under Assets";
            return false;
        }

        return true;
    }

    bool EditorSceneIOService::requestSaveAs()
    {
        if (!m_active_document || !m_active_document->scene)
        {
            m_status_message = "当前没有可保存的场景";
            return false;
        }

        std::string selected_vpath;
        if (!chooseSaveAsVPath(selected_vpath))
            return false;

        return saveToVPath(selected_vpath);
    }

    bool EditorSceneIOService::saveToVPath(const std::string& scene_vpath)
    {
        if (!m_active_document || !m_active_document->scene)
        {
            m_status_message = "当前没有可保存的场景";
            return false;
        }

        if (!m_services.resources)
        {
            m_status_message = "资源系统不可用，无法保存场景";
            return false;
        }

        std::string normalized_vpath;
        if (!normalizeSceneSaveVPath(scene_vpath, normalized_vpath))
        {
            m_status_message = "Save As 路径无效，需使用 asset 相对路径";
            return false;
        }

        auto vfs = m_services.resources->getVFS();
        if (!vfs)
        {
            m_status_message = "VFS 不可用，无法保存场景";
            return false;
        }

        auto native = vfs->resolveForWrite(normalized_vpath);
        if (!native)
        {
            m_status_message = "无法解析保存路径";
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(native->parent_path(), ec);
        if (ec)
        {
            m_status_message = "无法创建场景目录";
            HBD_CORE_WARN("{} save_failed scene_vpath={} native_path={} reason=create_scene_directory_failed error={}",
                          kEditorSceneIOLogTag,
                          normalized_vpath,
                          pathOrPlaceholder(*native),
                          ec.message());
            return false;
        }

        const bool existed = std::filesystem::exists(*native);
        const auto registry = m_services.resources ? m_services.resources->getRegistry().get() : nullptr;
        if (!SceneSerializer::SerializeToFile(*m_active_document->scene, *native, registry))
        {
            m_status_message = "场景保存失败";
            HBD_CORE_WARN("{} save_failed scene_vpath={} native_path={} reason=serialize_failed",
                          kEditorSceneIOLogTag,
                          normalized_vpath,
                          pathOrPlaceholder(*native));
            return false;
        }

        m_active_document->dirty = false;
        if (m_services.resources && m_services.resources->getRegistry())
        {
            if (const auto meta = m_services.resources->getRegistry()->findByPath(normalized_vpath))
                m_active_document->scene_asset_id = meta->id;
        }
        m_active_document->vpath = normalized_vpath;
        m_active_document->native_path = *native;
        m_active_document->display_name = native->filename().string();
        m_active_document->scene->setName(native->stem().string());
        m_status_message = existed ? "场景已保存" : "场景已另存为";
        saveLastOpenedScene(normalized_vpath);

        if (m_services.editor_resources)
        {
            m_services.editor_resources->enqueueSourceChanged(
                normalized_vpath,
                existed ? AssetSourceChangeType::Modified : AssetSourceChangeType::Added);
        }

        HBD_CORE_INFO("{} save_completed scene_vpath={} native_path={} existed_before={} asset_id={}",
                      kEditorSceneIOLogTag,
                      normalized_vpath,
                      pathOrPlaceholder(*native),
                      existed ? "true" : "false",
                      m_active_document->scene_asset_id.value);
        return true;
    }

    bool EditorSceneIOService::tryOpenProjectDefaultScene()
    {
        const std::filesystem::path settings_path = GetProjectSettingsPath();
        std::ifstream ifs(settings_path);
        if (!ifs.is_open())
            return false;

        json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception& e)
        {
            HBD_CORE_WARN("{} restore_default_scene_skipped settings_path={} reason=parse_project_settings_failed error={}",
                          kEditorSceneIOLogTag,
                          pathOrPlaceholder(settings_path),
                          e.what());
            return false;
        }

        const std::string default_scene = root.value("default_scene", std::string{});
        return !default_scene.empty() && open(default_scene);
    }

    bool EditorSceneIOService::tryOpenScannedScene()
    {
        if (m_assets_root.empty())
            return false;

        const std::filesystem::path scenes_dir = m_assets_root / "Scenes";
        if (!std::filesystem::exists(scenes_dir) || !std::filesystem::is_directory(scenes_dir))
            return false;

        struct Candidate
        {
            std::filesystem::path physical;
            std::filesystem::file_time_type modified{};
            std::string vpath;
        };

        std::vector<Candidate> candidates;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(scenes_dir, ec), end; it != end && !ec; it.increment(ec))
        {
            if (!it->is_regular_file())
                continue;

            const auto& physical = it->path();
            if (physical.extension() != ".scene")
                continue;

            std::string vpath;
            if (!toAssetVPath(physical, vpath))
                continue;

            candidates.push_back(Candidate{ physical, it->last_write_time(ec), vpath });
            ec.clear();
        }

        if (candidates.empty())
            return false;

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            if (a.modified != b.modified)
                return a.modified > b.modified;
            return a.vpath < b.vpath;
        });

        for (const Candidate& candidate : candidates)
        {
            if (open(candidate.vpath))
                return true;
        }

        return false;
    }

    bool EditorSceneIOService::restoreStartupScene()
    {
        const std::string last_scene = loadLastOpenedScene();
        if (!last_scene.empty() && open(last_scene, OpenSceneOptions{ false }))
            return true;

        if (tryOpenProjectDefaultScene())
            return true;

        if (tryOpenScannedScene())
            return true;

        return createUntitled("未找到可用场景，已创建空场景");
    }

    bool EditorSceneIOService::createUntitled(const char* reason)
    {
        auto scene = std::make_shared<Scene>();
        scene->setName("Untitled");
        if (m_services.resources)
            scene->environment().skybox_cubemap = m_services.resources->getBuiltinCubemapID(BuiltinCubemap::DefaultSky);
        initializeDefaultSceneContent(*scene);

        auto document = std::make_shared<SceneDocument>();
        document->scene = std::move(scene);
        document->dirty = true;
        document->display_name = "Untitled";

        if (!activateDocument(document))
        {
            m_status_message = "创建空场景失败";
            return false;
        }

        m_status_message = reason ? reason : "";
        return true;
    }

    std::string EditorSceneIOService::getSceneViewStateKey(const SceneDocument& document) const
    {
        if (document.scene_asset_id.value != 0)
        {
            char key_hex[17] = {};
            std::snprintf(key_hex, sizeof(key_hex), "%016llX",
                          static_cast<unsigned long long>(document.scene_asset_id.value));
            return key_hex;
        }

        if (!document.vpath.empty())
        {
            char key_hex[20] = {};
            std::snprintf(key_hex, sizeof(key_hex), "VP_%016llX",
                          static_cast<unsigned long long>(HashSceneKey(document.vpath)));
            return key_hex;
        }
        return {};
    }

    std::filesystem::path EditorSceneIOService::getSceneViewStatePath(const SceneDocument& document) const
    {
        const std::string key = getSceneViewStateKey(document);
        if (key.empty())
            return {};

        std::string label = document.display_name.empty() ? std::string("Scene") : document.display_name;
        label = SanitizeSceneFileStem(std::filesystem::path(label).stem().string());
        return GetSceneStateDirectoryPath() / (key + "_" + label + ".json");
    }

    bool EditorSceneIOService::saveSceneViewState(const SceneDocument& document, const EditorCameraState& camera_state) const
    {
        const std::filesystem::path state_path = getSceneViewStatePath(document);
        if (state_path.empty())
            return false;

        std::error_code ec;
        std::filesystem::create_directories(state_path.parent_path(), ec);
        if (ec)
            return false;

        json root = EditorCameraStateSerde::toJson(camera_state);
        root["scene"] = document.vpath;
        root["scene_asset_id"] = document.scene_asset_id.value;

        const std::filesystem::path temp_path = state_path.string() + ".tmp";
        std::ofstream ofs(temp_path, std::ios::trunc);
        if (!ofs.is_open())
            return false;

        ofs << root.dump(2);
        ofs.flush();
        ofs.close();

        ec.clear();
        std::filesystem::rename(temp_path, state_path, ec);
        if (ec)
        {
            ec.clear();
            std::filesystem::remove(state_path, ec);
            ec.clear();
            std::filesystem::rename(temp_path, state_path, ec);
            if (ec)
                return false;
        }

        return true;
    }

    bool EditorSceneIOService::loadSceneViewState(const SceneDocument& document, EditorCameraState& out_camera_state) const
    {
        const std::filesystem::path state_path = getSceneViewStatePath(document);
        std::ifstream ifs(state_path);
        if (!ifs.is_open())
            return false;

        json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception&)
        {
            return false;
        }
        return EditorCameraStateSerde::fromJson(root, out_camera_state);
    }

    void EditorSceneIOService::saveLastOpenedScene(const std::string& scene_vpath) const
    {
        const std::filesystem::path session_path = GetEditorUserSettingsPath();
        std::error_code ec;
        std::filesystem::create_directories(session_path.parent_path(), ec);
        if (ec)
        {
            HBD_CORE_WARN("{} session_state_save_failed path={} reason=create_session_directory_failed error={}",
                          kEditorSceneIOLogTag,
                          pathOrPlaceholder(session_path),
                          ec.message());
            return;
        }

        json root;
        root["last_open_scene"] = scene_vpath;

        std::ofstream ofs(session_path, std::ios::trunc);
        if (!ofs.is_open())
        {
            HBD_CORE_WARN("{} session_state_save_failed path={} reason=open_file_failed",
                          kEditorSceneIOLogTag,
                          pathOrPlaceholder(session_path));
            return;
        }

        ofs << root.dump(2);
    }

    std::string EditorSceneIOService::loadLastOpenedScene() const
    {
        const std::filesystem::path session_path = GetEditorUserSettingsPath();
        std::ifstream ifs(session_path);
        if (!ifs.is_open())
            return {};

        json root;
        try
        {
            ifs >> root;
        }
        catch (const std::exception& e)
        {
            HBD_CORE_WARN("{} session_state_load_failed path={} reason=parse_failed error={}",
                          kEditorSceneIOLogTag,
                          pathOrPlaceholder(session_path),
                          e.what());
            return {};
        }

        return root.value("last_open_scene", std::string{});
    }

    bool EditorSceneIOService::toAssetVPath(const std::filesystem::path& physical_path, std::string& out_vpath) const
    {
        out_vpath.clear();
        if (m_assets_root.empty() || physical_path.empty())
            return false;

        std::error_code ec;
        auto rel = std::filesystem::relative(physical_path, m_assets_root, ec);
        if (ec || rel.empty())
            return false;

        std::string rel_str = rel.generic_string();
        while (!rel_str.empty() && (rel_str.front() == '/' || rel_str.front() == '\\'))
            rel_str.erase(rel_str.begin());
        if (rel_str.empty())
            return false;

        out_vpath = std::string("asset:") + rel_str;
        return true;
    }
} // namespace Hybrid
