#pragma once
#include <memory>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "asset_type.h"

namespace Hybrid
{
    struct MeshVertex
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
        glm::vec4 tangent{}; //(x,y,z,w)
    };

    struct Submesh
    {
        uint32_t index_offset = 0;
        uint32_t index_count = 0;
        AssetID  material{};
        glm::vec3 aabb_min{0.0f};
        glm::vec3 aabb_max{0.0f};
    };

    // CPU-side mesh asset; GPU 缓存由渲染系统另行维护
    class Mesh
    {
    public:
        Mesh() = default;

        static std::shared_ptr<Mesh> CreateCube(); // 内置默认立方体

        const std::vector<MeshVertex>& getVertices() const { return m_vertices; }
        const std::vector<uint32_t>&   getIndices() const { return m_indices; }
        const std::vector<Submesh>&    getSubmeshes() const { return m_submeshes; }

        std::vector<MeshVertex>&       vertices() { return m_vertices; }
        std::vector<uint32_t>&         indices() { return m_indices; }
        std::vector<Submesh>&          submeshes() { return m_submeshes; }

    private:
        std::vector<MeshVertex> m_vertices;
        std::vector<uint32_t>   m_indices;
        std::vector<Submesh>    m_submeshes;
    };

} // namespace Hybrid
