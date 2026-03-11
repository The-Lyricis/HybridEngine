#include "shader_library.h"

#include <fstream>
#include <sstream>
#include <utility>

#include "runtime/core/base/macro.h"
#include "runtime/modules/render/public/shader.h"

namespace Hybrid
{
    namespace
    {
        bool queryWriteTime(const std::filesystem::path& path, std::filesystem::file_time_type& out_time)
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || ec)
                return false;

            out_time = std::filesystem::last_write_time(path, ec);
            return !ec;
        }
    } // namespace

    void ShaderLibrary::setRoot(const std::filesystem::path& shader_root)
    {
        m_root = shader_root;
    }

    bool ShaderLibrary::load(const std::string& name,
                             const std::filesystem::path& vertex_path,
                             const std::filesystem::path& fragment_path)
    {
        if (contains(name))
        {
            HBD_CORE_WARN("ShaderLibrary duplicate load ignored: {}", name);
            return false;
        }

        ShaderEntry entry{};
        entry.name = name;
        entry.vertex_path = resolvePath(vertex_path);
        entry.fragment_path = resolvePath(fragment_path);
        m_entries.emplace(name, std::move(entry));
        return reload(name);
    }

    bool ShaderLibrary::contains(const std::string& name) const
    {
        return m_entries.find(name) != m_entries.end();
    }

    std::shared_ptr<Shader> ShaderLibrary::get(const std::string& name) const
    {
        auto it = m_entries.find(name);
        if (it == m_entries.end())
        {
            HBD_CORE_ERROR("ShaderLibrary get failed, shader not found: {}", name);
            return nullptr;
        }

        if (!it->second.loaded || !it->second.shader)
        {
            HBD_CORE_ERROR("ShaderLibrary get failed, shader not loaded: {}", name);
            return nullptr;
        }

        return it->second.shader;
    }

    bool ShaderLibrary::reload(const std::string& name)
    {
        auto it = m_entries.find(name);
        if (it == m_entries.end())
        {
            HBD_CORE_ERROR("ShaderLibrary reload failed, shader not found: {}", name);
            return false;
        }

        ShaderEntry& entry = it->second;

        std::string vertex_source;
        std::string fragment_source;
        if (!readTextFile(entry.vertex_path, vertex_source))
        {
            HBD_CORE_ERROR("ShaderLibrary failed to read vertex shader '{}': {}", entry.name, entry.vertex_path.string());
            return false;
        }

        if (!readTextFile(entry.fragment_path, fragment_source))
        {
            HBD_CORE_ERROR("ShaderLibrary failed to read fragment shader '{}': {}", entry.name, entry.fragment_path.string());
            return false;
        }

        auto shader = Shader::Create(vertex_source, fragment_source);
        if (!shader)
        {
            HBD_CORE_ERROR("ShaderLibrary failed to create shader: {}", entry.name);
            return false;
        }

        std::filesystem::file_time_type vertex_time{};
        std::filesystem::file_time_type fragment_time{};
        if (!queryWriteTime(entry.vertex_path, vertex_time))
            HBD_CORE_WARN("ShaderLibrary cannot query timestamp for {}", entry.vertex_path.string());
        if (!queryWriteTime(entry.fragment_path, fragment_time))
            HBD_CORE_WARN("ShaderLibrary cannot query timestamp for {}", entry.fragment_path.string());

        entry.shader = std::move(shader);
        entry.vertex_write_time = vertex_time;
        entry.fragment_write_time = fragment_time;
        entry.loaded = true;
        HBD_CORE_INFO("ShaderLibrary loaded shader '{}'", entry.name);
        return true;
    }

    void ShaderLibrary::reloadChanged()
    {
        for (auto& [name, entry] : m_entries)
        {
            std::filesystem::file_time_type vertex_time{};
            std::filesystem::file_time_type fragment_time{};
            const bool has_vertex_time = queryWriteTime(entry.vertex_path, vertex_time);
            const bool has_fragment_time = queryWriteTime(entry.fragment_path, fragment_time);
            if (!has_vertex_time || !has_fragment_time)
                continue;

            const bool vertex_changed = vertex_time != entry.vertex_write_time;
            const bool fragment_changed = fragment_time != entry.fragment_write_time;
            if (!vertex_changed && !fragment_changed)
                continue;

            if (reload(name))
                HBD_CORE_INFO("ShaderLibrary reloaded changed shader '{}'", name);
        }
    }

    void ShaderLibrary::clear()
    {
        m_entries.clear();
    }

    bool ShaderLibrary::readTextFile(const std::filesystem::path& path, std::string& out_text)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return false;

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        out_text = buffer.str();
        return true;
    }

    std::filesystem::path ShaderLibrary::resolvePath(const std::filesystem::path& path) const
    {
        if (path.is_absolute())
            return path;
        return m_root.empty() ? path : (m_root / path);
    }
} // namespace Hybrid
