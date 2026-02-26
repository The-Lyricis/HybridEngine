// mesh.cpp
#include "mesh.h"
#include <cstdint>

namespace Hybrid
{
    std::shared_ptr<Mesh> Mesh::CreateCube()
    {
        auto mesh = std::make_shared<Mesh>();
        mesh->m_vertices.reserve(24);
        mesh->m_indices.reserve(36);

        const float s = 0.5f;

        auto addFace = [&](const glm::vec3& n,
                           const glm::vec3& t,
                           const glm::vec3& p0,
                           const glm::vec3& p1,
                           const glm::vec3& p2,
                           const glm::vec3& p3)
        {
            // 约定：p0,p1,p2,p3 是“从外侧看去”的 CCW 顺序
            const uint32_t base = static_cast<uint32_t>(mesh->m_vertices.size());
            mesh->m_vertices.push_back(MeshVertex{ p0, n, glm::vec2(0.0f, 0.0f), glm::vec4(t, 1.0f) });
            mesh->m_vertices.push_back(MeshVertex{ p1, n, glm::vec2(1.0f, 0.0f), glm::vec4(t, 1.0f) });
            mesh->m_vertices.push_back(MeshVertex{ p2, n, glm::vec2(1.0f, 1.0f), glm::vec4(t, 1.0f) });
            mesh->m_vertices.push_back(MeshVertex{ p3, n, glm::vec2(0.0f, 1.0f), glm::vec4(t, 1.0f) });


            // 两个三角形：0-1-2, 2-3-0
            mesh->m_indices.push_back(base + 0);
            mesh->m_indices.push_back(base + 1);
            mesh->m_indices.push_back(base + 2);

            mesh->m_indices.push_back(base + 2);
            mesh->m_indices.push_back(base + 3);
            mesh->m_indices.push_back(base + 0);
        };

        // +Z 面（前）
        addFace({0, 0, 1}, {1, 0, 0},
                {-s, -s,  s}, { s, -s,  s}, { s,  s,  s}, {-s,  s,  s});

        // -Z 面（后）
        addFace({0, 0, -1}, {-1, 0, 0},
                { s, -s, -s}, {-s, -s, -s}, {-s,  s, -s}, { s,  s, -s});

        // +X 面（右）
        addFace({1, 0, 0}, {0, 0, -1},
                { s, -s,  s}, { s, -s, -s}, { s,  s, -s}, { s,  s,  s});

        // -X 面（左）
        addFace({-1, 0, 0}, {0, 0, 1},
                {-s, -s, -s}, {-s, -s,  s}, {-s,  s,  s}, {-s,  s, -s});

        // +Y 面（上）
        addFace({0, 1, 0}, {1, 0, 0},
                {-s,  s,  s}, { s,  s,  s}, { s,  s, -s}, {-s,  s, -s});

        // -Y 面（下）
        addFace({0, -1, 0}, {1, 0, 0},
                {-s, -s, -s}, { s, -s, -s}, { s, -s,  s}, {-s, -s,  s});

        // 单一 submesh（整个 cube 同一材质）
        Submesh sm{};
        sm.index_offset = 0;
        sm.index_count  = static_cast<uint32_t>(mesh->m_indices.size());
        sm.material     = AssetID{}; // 由外部 MeshRenderer 或默认材质系统决定
        sm.aabb_min     = {-s, -s, -s};
        sm.aabb_max     = { s,  s,  s};
        mesh->m_submeshes.push_back(sm);

        return mesh;
    }
}
