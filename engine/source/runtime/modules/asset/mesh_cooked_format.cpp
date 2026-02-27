#include "mesh_cooked_format.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace Hybrid
{
    namespace
    {
        constexpr char kMagic[4] = {'H', 'M', 'S', 'H'};
        constexpr uint16_t kVersion = 1;

        struct HmeshHeaderV1
        {
            char magic[4];
            uint16_t version;
            uint16_t flags;
            uint32_t vertex_count;
            uint32_t index_count;
            uint32_t submesh_count;
            uint32_t vertex_stride;
            uint32_t submesh_stride;
            uint32_t reserved;
        };

        struct HmeshVertexV1
        {
            float px, py, pz;
            float nx, ny, nz;
            float u, v;
            float tx, ty, tz, tw;
        };

        struct HmeshSubmeshV1
        {
            uint32_t index_offset;
            uint32_t index_count;
            uint64_t material_id;
            float aabb_min[3];
            float aabb_max[3];
            uint32_t reserved0;
            uint32_t reserved1;
        };

        static_assert(sizeof(HmeshHeaderV1) == 32, "Unexpected HMSH header size");
        static_assert(sizeof(HmeshVertexV1) == 48, "Unexpected HMSH vertex size");
        static_assert(sizeof(HmeshSubmeshV1) == 48, "Unexpected HMSH submesh size");

        void setError(std::string* out_error, const char* msg)
        {
            if (out_error)
                *out_error = msg;
        }
    } // namespace

    bool HmeshEncode(const Mesh& mesh, std::vector<char>& out_bytes)
    {
        const auto& vertices = mesh.getVertices();
        const auto& indices = mesh.getIndices();
        const auto& submeshes = mesh.getSubmeshes();

        if (vertices.size() > std::numeric_limits<uint32_t>::max() ||
            indices.size() > std::numeric_limits<uint32_t>::max() ||
            submeshes.size() > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }

        const uint64_t vertex_bytes = static_cast<uint64_t>(vertices.size()) * sizeof(HmeshVertexV1);
        const uint64_t index_bytes = static_cast<uint64_t>(indices.size()) * sizeof(uint32_t);
        const uint64_t submesh_bytes = static_cast<uint64_t>(submeshes.size()) * sizeof(HmeshSubmeshV1);
        const uint64_t total_bytes = sizeof(HmeshHeaderV1) + vertex_bytes + index_bytes + submesh_bytes;
        if (total_bytes > static_cast<uint64_t>(SIZE_MAX))
            return false;

        HmeshHeaderV1 header{};
        std::memcpy(header.magic, kMagic, sizeof(kMagic));
        header.version = kVersion;
        header.flags = 0;
        header.vertex_count = static_cast<uint32_t>(vertices.size());
        header.index_count = static_cast<uint32_t>(indices.size());
        header.submesh_count = static_cast<uint32_t>(submeshes.size());
        header.vertex_stride = static_cast<uint32_t>(sizeof(HmeshVertexV1));
        header.submesh_stride = static_cast<uint32_t>(sizeof(HmeshSubmeshV1));
        header.reserved = 0;

        out_bytes.resize(static_cast<size_t>(total_bytes));
        char* write = out_bytes.data();
        std::memcpy(write, &header, sizeof(header));
        write += sizeof(header);

        for (const MeshVertex& src : vertices)
        {
            HmeshVertexV1 dst{};
            dst.px = src.position.x;
            dst.py = src.position.y;
            dst.pz = src.position.z;
            dst.nx = src.normal.x;
            dst.ny = src.normal.y;
            dst.nz = src.normal.z;
            dst.u = src.uv.x;
            dst.v = src.uv.y;
            dst.tx = src.tangent.x;
            dst.ty = src.tangent.y;
            dst.tz = src.tangent.z;
            dst.tw = src.tangent.w;
            std::memcpy(write, &dst, sizeof(dst));
            write += sizeof(dst);
        }

        if (!indices.empty())
        {
            std::memcpy(write, indices.data(), static_cast<size_t>(index_bytes));
            write += static_cast<size_t>(index_bytes);
        }

        for (const Submesh& src : submeshes)
        {
            HmeshSubmeshV1 dst{};
            dst.index_offset = src.index_offset;
            dst.index_count = src.index_count;
            dst.material_id = src.material.value;
            dst.aabb_min[0] = src.aabb_min.x;
            dst.aabb_min[1] = src.aabb_min.y;
            dst.aabb_min[2] = src.aabb_min.z;
            dst.aabb_max[0] = src.aabb_max.x;
            dst.aabb_max[1] = src.aabb_max.y;
            dst.aabb_max[2] = src.aabb_max.z;
            dst.reserved0 = 0;
            dst.reserved1 = 0;
            std::memcpy(write, &dst, sizeof(dst));
            write += sizeof(dst);
        }

        return true;
    }

    bool HmeshDecode(const std::vector<char>& bytes, Mesh& out_mesh, std::string* out_error)
    {
        if (bytes.size() < sizeof(HmeshHeaderV1))
        {
            setError(out_error, "HMSH decode failed: file too small");
            return false;
        }

        HmeshHeaderV1 header{};
        std::memcpy(&header, bytes.data(), sizeof(header));

        if (std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0)
        {
            setError(out_error, "HMSH decode failed: bad magic");
            return false;
        }

        if (header.version != kVersion)
        {
            setError(out_error, "HMSH decode failed: unsupported version");
            return false;
        }

        if (header.vertex_stride != sizeof(HmeshVertexV1) || header.submesh_stride != sizeof(HmeshSubmeshV1))
        {
            setError(out_error, "HMSH decode failed: unsupported stride");
            return false;
        }

        const uint64_t vertex_bytes = static_cast<uint64_t>(header.vertex_count) * sizeof(HmeshVertexV1);
        const uint64_t index_bytes = static_cast<uint64_t>(header.index_count) * sizeof(uint32_t);
        const uint64_t submesh_bytes = static_cast<uint64_t>(header.submesh_count) * sizeof(HmeshSubmeshV1);
        const uint64_t expected = sizeof(HmeshHeaderV1) + vertex_bytes + index_bytes + submesh_bytes;
        if (expected != bytes.size())
        {
            setError(out_error, "HMSH decode failed: payload size mismatch");
            return false;
        }

        auto& dst_vertices = out_mesh.vertices();
        auto& dst_indices = out_mesh.indices();
        auto& dst_submeshes = out_mesh.submeshes();
        dst_vertices.clear();
        dst_indices.clear();
        dst_submeshes.clear();
        dst_vertices.resize(header.vertex_count);
        dst_indices.resize(header.index_count);
        dst_submeshes.resize(header.submesh_count);

        const char* read = bytes.data() + sizeof(HmeshHeaderV1);

        for (uint32_t i = 0; i < header.vertex_count; ++i)
        {
            HmeshVertexV1 src{};
            std::memcpy(&src, read, sizeof(src));
            read += sizeof(src);

            MeshVertex dst{};
            dst.position = {src.px, src.py, src.pz};
            dst.normal = {src.nx, src.ny, src.nz};
            dst.uv = {src.u, src.v};
            dst.tangent = {src.tx, src.ty, src.tz, src.tw};
            dst_vertices[i] = dst;
        }

        if (header.index_count > 0)
        {
            std::memcpy(dst_indices.data(), read, static_cast<size_t>(index_bytes));
            read += static_cast<size_t>(index_bytes);
        }

        for (uint32_t i = 0; i < header.submesh_count; ++i)
        {
            HmeshSubmeshV1 src{};
            std::memcpy(&src, read, sizeof(src));
            read += sizeof(src);

            Submesh dst{};
            dst.index_offset = src.index_offset;
            dst.index_count = src.index_count;
            dst.material = AssetID::FromRaw(src.material_id);
            dst.aabb_min = {src.aabb_min[0], src.aabb_min[1], src.aabb_min[2]};
            dst.aabb_max = {src.aabb_max[0], src.aabb_max[1], src.aabb_max[2]};
            dst_submeshes[i] = dst;
        }

        return true;
    }

    bool HmeshLooksLikeFile(const std::vector<char>& bytes)
    {
        return bytes.size() >= 4 && std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) == 0;
    }
} // namespace Hybrid
