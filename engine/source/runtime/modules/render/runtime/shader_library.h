#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Hybrid
{
    class Shader;

    class ShaderLibrary
    {
    public:
        struct ShaderEntry
        {
            std::string name;
            std::filesystem::path vertex_path;
            std::filesystem::path fragment_path;
            std::shared_ptr<Shader> shader;
            std::filesystem::file_time_type vertex_write_time{};
            std::filesystem::file_time_type fragment_write_time{};
            bool loaded = false;
        };

    public:
        void setRoot(const std::filesystem::path& shader_root);
        bool load(const std::string& name,
                  const std::filesystem::path& vertex_path,
                  const std::filesystem::path& fragment_path);
        bool contains(const std::string& name) const;
        std::shared_ptr<Shader> get(const std::string& name) const;
        bool reload(const std::string& name);
        void reloadChanged();
        void clear();

    private:
        static bool readTextFile(const std::filesystem::path& path, std::string& out_text);
        std::filesystem::path resolvePath(const std::filesystem::path& path) const;

    private:
        std::filesystem::path m_root;
        std::unordered_map<std::string, ShaderEntry> m_entries;
    };
} // namespace Hybrid
