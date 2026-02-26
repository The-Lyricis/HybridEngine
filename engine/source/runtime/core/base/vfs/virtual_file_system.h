#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hybrid
{
    struct VfsMount
    {
        std::filesystem::path root;
        int priority;
    };

    class IVirtualFileSystem
    {
    public:
        virtual ~IVirtualFileSystem() = default;

        // 挂载别名到根目录（可多次挂载，同一别名按优先级选择）
        virtual void mount(const std::string& alias,
            const std::filesystem::path& root,
            int priority = 0) = 0;

        // 判断逻辑路径是否存在（<别名>:<相对路径>，即 alias:relative）
        virtual bool exists(const std::string& path) const = 0;

        // 将逻辑路径解析为实际文件系统路径；失败返回 nullopt
        virtual std::optional<std::filesystem::path> resolve(const std::string& path) const = 0;
        virtual std::optional<std::filesystem::path> resolveForWrite(const std::string& path) const =0;

        // 读取完整文件内容；失败返回空 vector
        virtual std::vector<char> readAll(const std::string& path) const = 0;
    };
    // 基于本地文件系统的虚拟文件系统实现
    class NativeFileSystem : public IVirtualFileSystem {
    public:
        void mount(const std::string& alias,
            const std::filesystem::path& root,
            int priority = 0) override;

        bool exists(const std::string& path) const override;

        std::optional<std::filesystem::path> resolve(const std::string& path) const override;

        std::optional<std::filesystem::path> resolveForWrite(const std::string& path) const override;

        std::vector<char> readAll(const std::string& path) const override;

    private:
        // alias -> 多个挂载点（按 priority 排序）
        std::unordered_map<std::string, std::vector<VfsMount>> m_mounts;
    };

}// namespace Hybrid
