#include "mesh_importer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>
#include <tiny_obj_loader.h>

#include "runtime/core/base/macro.h"
#include "runtime/modules/asset/mesh.h"
#include "runtime/modules/asset/mesh_cooked_format.h"
#include "texture_importer.h"

namespace Hybrid
{
    namespace
    {
        struct VertexKey
        {
            int position_index = -1;
            int normal_index = -1;
            int texcoord_index = -1;

            bool operator==(const VertexKey& rhs) const
            {
                return position_index == rhs.position_index && normal_index == rhs.normal_index &&
                       texcoord_index == rhs.texcoord_index;
            }
        };

        struct VertexKeyHasher
        {
            size_t operator()(const VertexKey& key) const
            {
                size_t h = std::hash<int>()(key.position_index);
                h ^= (std::hash<int>()(key.normal_index) << 1);
                h ^= (std::hash<int>()(key.texcoord_index) << 2);
                return h;
            }
        };

        struct SubmeshBuild
        {
            std::vector<uint32_t> local_indices;
            glm::vec3 aabb_min{0.0f};
            glm::vec3 aabb_max{0.0f};
            bool has_bounds = false;
            int material_index = -1;
        };

        struct ObjBuildOutput
        {
            Mesh mesh{};
            std::vector<int> submesh_material_indices;
            std::vector<std::string> material_names;
            std::vector<tinyobj::material_t> materials;
        };

        using json = nlohmann::json;

        std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool isMeshExt(std::string_view ext)
        {
            const std::string e = toLower(std::string(ext));
            return e == ".fbx" || e == ".gltf" || e == ".glb" || e == ".obj";
        }

        bool splitLogicalPath(const std::string& path, std::string& alias_out, std::string& rel_out)
        {
            const auto pos = path.find(':');
            if (pos == std::string::npos || pos == 0 || pos + 1 >= path.size())
                return false;

            alias_out = path.substr(0, pos);
            rel_out = path.substr(pos + 1);
            if (rel_out.empty() || rel_out.front() == '/' || rel_out.front() == '\\')
                return false;
            std::replace(rel_out.begin(), rel_out.end(), '\\', '/');
            return true;
        }

        bool hasParentTraversal(const std::filesystem::path& p)
        {
            for (const auto& part : p)
            {
                if (part == "..")
                    return true;
            }
            return false;
        }

        std::string buildDefaultCookedPath(const std::string& source_path)
        {
            std::string alias, rel;
            if (!splitLogicalPath(source_path, alias, rel))
                return {};

            std::filesystem::path p(rel);
            p.replace_extension(".hmesh");
            return std::string("cache:Cooked/") + p.generic_string();
        }

        std::string makeSimpleHash(const std::filesystem::path& file)
        {
            std::error_code ec;

            auto size = std::filesystem::file_size(file, ec);
            if (ec)
                size = 0;

            ec.clear();
            auto last_write_time = std::filesystem::last_write_time(file, ec);
            const auto ticks = ec ? 0LL : static_cast<long long>(last_write_time.time_since_epoch().count());

            return std::to_string(static_cast<unsigned long long>(size)) + "_" + std::to_string(ticks);
        }

        std::string sanitizeToken(const std::string& input)
        {
            std::string out;
            out.reserve(input.size());

            for (char c : input)
            {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (std::isalnum(uc) != 0 || c == '_' || c == '-')
                {
                    out.push_back(c);
                }
                else
                {
                    out.push_back('_');
                }
            }

            while (!out.empty() && out.front() == '_')
                out.erase(out.begin());
            while (!out.empty() && out.back() == '_')
                out.pop_back();

            if (out.empty())
                out = "material";
            return out;
        }

        std::string buildMaterialSubassetSourcePath(const std::string& obj_rel_path,
                                                    const std::string& material_name,
                                                    int material_index)
        {
            std::filesystem::path obj_path(obj_rel_path);
            const std::string stem = obj_path.stem().string();
            const std::string safe_name = sanitizeToken(material_name);
            const std::string file_name =
                stem + "__mat_" + safe_name + "_" + std::to_string(material_index) + ".mat";

            // Keep material metas in the same directory as the source OBJ.
            std::filesystem::path sub_rel = obj_path.parent_path() / file_name;
            return std::string("asset:") + sub_rel.generic_string();
        }

        std::string buildMaterialSubassetKey(const std::string& material_name, int material_index)
        {
            return std::string("material:") + sanitizeToken(material_name) + ":" + std::to_string(material_index);
        }

        std::string buildReferencedAssetPath(const std::string& source_rel_path, const std::string& ref_path)
        {
            if (ref_path.empty())
                return {};

            std::string normalized = ref_path;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            std::filesystem::path ref_fs_path(normalized);
            if (ref_fs_path.is_absolute())
                return {};

            std::filesystem::path combined = std::filesystem::path(source_rel_path).parent_path() / ref_fs_path;
            combined = combined.lexically_normal();
            if (combined.empty() || hasParentTraversal(combined))
                return {};

            return std::string("asset:") + combined.generic_string();
        }

        void appendUniqueAssets(std::vector<AssetMetadata>& dst,
                                const std::vector<AssetMetadata>& src,
                                std::unordered_set<uint64_t>& seen_ids)
        {
            for (const auto& meta : src)
            {
                if (!seen_ids.insert(meta.id.value).second)
                    continue;
                dst.push_back(meta);
            }
        }

        bool readPosition(const tinyobj::attrib_t& attrib, int index, glm::vec3& out_position)
        {
            if (index < 0)
                return false;

            const size_t base = static_cast<size_t>(index) * 3u;
            if (base + 2 >= attrib.vertices.size())
                return false;

            out_position.x = static_cast<float>(attrib.vertices[base + 0]);
            out_position.y = static_cast<float>(attrib.vertices[base + 1]);
            out_position.z = static_cast<float>(attrib.vertices[base + 2]);
            return true;
        }

        bool readNormal(const tinyobj::attrib_t& attrib, int index, glm::vec3& out_normal)
        {
            if (index < 0)
                return false;

            const size_t base = static_cast<size_t>(index) * 3u;
            if (base + 2 >= attrib.normals.size())
                return false;

            out_normal.x = static_cast<float>(attrib.normals[base + 0]);
            out_normal.y = static_cast<float>(attrib.normals[base + 1]);
            out_normal.z = static_cast<float>(attrib.normals[base + 2]);
            return true;
        }

        bool readTexcoord(const tinyobj::attrib_t& attrib, int index, glm::vec2& out_uv)
        {
            if (index < 0)
                return false;

            const size_t base = static_cast<size_t>(index) * 2u;
            if (base + 1 >= attrib.texcoords.size())
                return false;

            out_uv.x = static_cast<float>(attrib.texcoords[base + 0]);
            out_uv.y = static_cast<float>(attrib.texcoords[base + 1]);
            return true;
        }

        uint64_t makeSubmeshKey(uint32_t shape_index, int material_id)
        {
            const uint32_t mat = static_cast<uint32_t>(material_id + 1);
            return (static_cast<uint64_t>(shape_index) << 32u) | static_cast<uint64_t>(mat);
        }

        void recomputeVertexNormals(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
        {
            for (auto& vertex : vertices)
                vertex.normal = glm::vec3(0.0f);

            constexpr float kMinArea2 = 1e-12f;
            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const uint32_t i0 = indices[i + 0];
                const uint32_t i1 = indices[i + 1];
                const uint32_t i2 = indices[i + 2];
                if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                    continue;

                const glm::vec3& p0 = vertices[i0].position;
                const glm::vec3& p1 = vertices[i1].position;
                const glm::vec3& p2 = vertices[i2].position;

                const glm::vec3 edge1 = p1 - p0;
                const glm::vec3 edge2 = p2 - p0;
                glm::vec3 face_normal = glm::cross(edge1, edge2);
                const float len2 = glm::dot(face_normal, face_normal);
                if (len2 <= kMinArea2)
                    continue;

                face_normal /= std::sqrt(len2);
                vertices[i0].normal += face_normal;
                vertices[i1].normal += face_normal;
                vertices[i2].normal += face_normal;
            }

            for (auto& vertex : vertices)
            {
                const float len2 = glm::dot(vertex.normal, vertex.normal);
                if (len2 > kMinArea2)
                {
                    vertex.normal /= std::sqrt(len2);
                }
                else
                {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
        }

        bool buildMeshFromObj(const std::filesystem::path& source_file, ObjBuildOutput& out_build, std::string& out_error)
        {
            tinyobj::ObjReaderConfig config{};
            config.triangulate = true;
            config.vertex_color = false;
            config.mtl_search_path = source_file.parent_path().string();

            tinyobj::ObjReader reader;
            if (!reader.ParseFromFile(source_file.string(), config))
            {
                out_error = reader.Error().empty() ? "tinyobj parse failed" : reader.Error();
                return false;
            }

            if (!reader.Warning().empty())
            {
                HBD_CORE_WARN("MeshImporter OBJ warning ({}): {}", source_file.string(), reader.Warning());
            }

            const tinyobj::attrib_t& attrib = reader.GetAttrib();
            const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
            const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

            if (attrib.vertices.empty())
            {
                out_error = "OBJ has no vertex positions";
                return false;
            }
            if (shapes.empty())
            {
                out_error = "OBJ has no shapes";
                return false;
            }

            out_build.material_names.clear();
            out_build.material_names.reserve(materials.size());
            for (const auto& material : materials)
                out_build.material_names.push_back(material.name);
            out_build.materials = materials;

            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> all_indices;
            std::vector<SubmeshBuild> submesh_builds;
            std::unordered_map<VertexKey, uint32_t, VertexKeyHasher> vertex_lut;
            std::unordered_map<uint64_t, size_t> submesh_lut;
            bool need_recompute_normals = false;

            for (uint32_t shape_index = 0; shape_index < shapes.size(); ++shape_index)
            {
                const tinyobj::shape_t& shape = shapes[shape_index];
                size_t index_offset = 0;

                for (size_t face_index = 0; face_index < shape.mesh.num_face_vertices.size(); ++face_index)
                {
                    const uint8_t face_vertex_count = shape.mesh.num_face_vertices[face_index];
                    if (face_vertex_count != 3)
                    {
                        index_offset += face_vertex_count;
                        continue;
                    }

                    int material_id = -1;
                    if (face_index < shape.mesh.material_ids.size())
                        material_id = shape.mesh.material_ids[face_index];

                    const uint64_t submesh_key = makeSubmeshKey(shape_index, material_id);
                    auto submesh_it = submesh_lut.find(submesh_key);
                    if (submesh_it == submesh_lut.end())
                    {
                        const size_t new_idx = submesh_builds.size();
                        SubmeshBuild build{};
                        build.material_index = material_id;
                        submesh_builds.push_back(std::move(build));
                        submesh_lut.emplace(submesh_key, new_idx);
                        submesh_it = submesh_lut.find(submesh_key);
                    }

                    SubmeshBuild& submesh = submesh_builds[submesh_it->second];

                    for (uint32_t corner = 0; corner < 3; ++corner)
                    {
                        const tinyobj::index_t obj_index = shape.mesh.indices[index_offset + corner];
                        VertexKey key{obj_index.vertex_index, obj_index.normal_index, obj_index.texcoord_index};

                        uint32_t vertex_index = 0;
                        auto vertex_it = vertex_lut.find(key);
                        if (vertex_it != vertex_lut.end())
                        {
                            vertex_index = vertex_it->second;
                        }
                        else
                        {
                            MeshVertex vertex{};
                            if (!readPosition(attrib, obj_index.vertex_index, vertex.position))
                            {
                                out_error = "OBJ has invalid position index";
                                return false;
                            }

                            if (!readNormal(attrib, obj_index.normal_index, vertex.normal))
                            {
                                vertex.normal = glm::vec3(0.0f);
                                need_recompute_normals = true;
                            }

                            if (!readTexcoord(attrib, obj_index.texcoord_index, vertex.uv))
                                vertex.uv = glm::vec2(0.0f);

                            vertex.tangent = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

                            vertex_index = static_cast<uint32_t>(vertices.size());
                            vertices.push_back(vertex);
                            vertex_lut.emplace(key, vertex_index);
                        }

                        all_indices.push_back(vertex_index);
                        submesh.local_indices.push_back(vertex_index);

                        const glm::vec3& position = vertices[vertex_index].position;
                        if (!submesh.has_bounds)
                        {
                            submesh.aabb_min = position;
                            submesh.aabb_max = position;
                            submesh.has_bounds = true;
                        }
                        else
                        {
                            submesh.aabb_min = glm::min(submesh.aabb_min, position);
                            submesh.aabb_max = glm::max(submesh.aabb_max, position);
                        }
                    }

                    index_offset += 3;
                }
            }

            if (vertices.empty() || all_indices.empty())
            {
                out_error = "OBJ produced empty mesh";
                return false;
            }

            if (need_recompute_normals)
                recomputeVertexNormals(vertices, all_indices);

            auto& dst_vertices = out_build.mesh.vertices();
            auto& dst_indices = out_build.mesh.indices();
            auto& dst_submeshes = out_build.mesh.submeshes();
            dst_vertices = std::move(vertices);
            dst_indices.clear();
            dst_submeshes.clear();
            out_build.submesh_material_indices.clear();

            for (const SubmeshBuild& src : submesh_builds)
            {
                if (src.local_indices.empty())
                    continue;

                Submesh submesh{};
                submesh.index_offset = static_cast<uint32_t>(dst_indices.size());
                submesh.index_count = static_cast<uint32_t>(src.local_indices.size());
                submesh.material = AssetID{};
                if (src.has_bounds)
                {
                    submesh.aabb_min = src.aabb_min;
                    submesh.aabb_max = src.aabb_max;
                }

                dst_indices.insert(dst_indices.end(), src.local_indices.begin(), src.local_indices.end());
                dst_submeshes.push_back(submesh);
                out_build.submesh_material_indices.push_back(src.material_index);
            }

            if (dst_submeshes.empty())
            {
                out_error = "OBJ produced no submeshes";
                return false;
            }

            HBD_CORE_INFO("MeshImporter OBJ: {} shapes, {} materials, {} vertices, {} indices, {} submeshes",
                          shapes.size(),
                          materials.size(),
                          dst_vertices.size(),
                          dst_indices.size(),
                          dst_submeshes.size());

            return true;
        }
    } // namespace

    bool MeshImporter::supportsExtension(std::string_view ext) const
    {
        return isMeshExt(ext);
    }

    ImportResult MeshImporter::importAsset(const ImportRequest& request,
                                           AssetRegistry& registry,
                                           IVirtualFileSystem& vfs)
    {
        ImportResult out{};

        std::string src_alias, src_rel;
        if (!splitLogicalPath(request.source_path, src_alias, src_rel))
        {
            out.message = "MeshImporter: source_path must be alias:relative";
            return out;
        }
        if (toLower(src_alias) != "asset")
        {
            out.message = "MeshImporter: source_path must use asset: alias";
            return out;
        }

        const std::string ext = toLower(std::filesystem::path(src_rel).extension().string());
        if (!isMeshExt(ext))
        {
            out.message = "MeshImporter: unsupported extension";
            return out;
        }

        if (ext == ".gltf" || ext == ".glb")
        {
            out.message = "MeshImporter: .gltf/.glb not implemented yet";
            return out;
        }
        if (ext == ".fbx")
        {
            out.message = "MeshImporter: .fbx not implemented yet";
            return out;
        }
        if (ext != ".obj")
        {
            out.message = "MeshImporter: only .obj is implemented";
            return out;
        }

        if (!vfs.exists(request.source_path))
        {
            out.message = "MeshImporter: source file not found";
            return out;
        }

        auto source_native = vfs.resolve(request.source_path);
        if (!source_native)
        {
            out.message = "MeshImporter: cannot resolve source path";
            return out;
        }

        ObjBuildOutput obj_build{};
        std::string parse_error;
        if (!buildMeshFromObj(*source_native, obj_build, parse_error))
        {
            out.message = "MeshImporter: OBJ parse failed: " + parse_error;
            return out;
        }

        AssetID mesh_asset_id{};
        if (const auto* existing_mesh = registry.findByPath(request.source_path))
            mesh_asset_id = existing_mesh->id;
        else
            mesh_asset_id = registry.generateUniqueID();

        // Inline material sub-assets generation for OBJ/MTL.
        std::unordered_map<int, AssetID> material_index_to_id;
        std::vector<AssetMetadata> generated_assets;
        std::unordered_set<uint64_t> generated_asset_ids;
        std::unordered_set<int> used_material_indices;
        used_material_indices.reserve(obj_build.submesh_material_indices.size());
        std::unordered_map<std::string, AssetID> imported_texture_ids;
        TextureImporter texture_importer;

        for (int material_index : obj_build.submesh_material_indices)
        {
            if (material_index >= 0)
                used_material_indices.insert(material_index);
        }

        std::vector<int> sorted_material_indices;
        sorted_material_indices.reserve(used_material_indices.size());
        for (int material_index : used_material_indices)
            sorted_material_indices.push_back(material_index);
        std::sort(sorted_material_indices.begin(), sorted_material_indices.end());

        for (int material_index : sorted_material_indices)
        {
            const bool valid_index =
                material_index >= 0 && material_index < static_cast<int>(obj_build.material_names.size()) &&
                material_index < static_cast<int>(obj_build.materials.size());
            if (!valid_index)
            {
                HBD_CORE_WARN("MeshImporter: skip invalid OBJ material index {} for {}",
                              material_index,
                              request.source_path);
                continue;
            }

            const std::string& material_name_raw = obj_build.material_names[material_index];
            const tinyobj::material_t& src_material = obj_build.materials[material_index];
            const std::string material_name = material_name_raw.empty()
                                                  ? ("material_" + std::to_string(material_index))
                                                  : material_name_raw;

            const std::string material_source_path =
                buildMaterialSubassetSourcePath(src_rel, material_name, material_index);
            const std::string subasset_key = buildMaterialSubassetKey(material_name, material_index);

            AssetMetadata material_meta{};
            std::string previous_material_source_path;
            if (const auto* existing = registry.findBySubasset(mesh_asset_id, subasset_key))
                material_meta = *existing;
            else if (const auto* existing = registry.findByPath(material_source_path))
                material_meta = *existing;
            else
                material_meta.id = registry.generateUniqueID();
            previous_material_source_path = material_meta.source_path;

            auto importTextureRef = [&](const std::string& ref_path) -> AssetID {
                const std::string texture_source_path = buildReferencedAssetPath(src_rel, ref_path);
                if (texture_source_path.empty())
                    return AssetID{};

                if (auto imported = imported_texture_ids.find(texture_source_path); imported != imported_texture_ids.end())
                    return imported->second;

                const auto* existing_texture = registry.findByPath(texture_source_path);
                if (existing_texture && existing_texture->type == AssetType::Texture2D)
                {
                    imported_texture_ids[texture_source_path] = existing_texture->id;
                    return existing_texture->id;
                }

                if (!vfs.exists(texture_source_path))
                {
                    HBD_CORE_WARN("MeshImporter: referenced texture missing {}", texture_source_path);
                    return AssetID{};
                }

                auto texture_native = vfs.resolve(texture_source_path);
                if (!texture_native)
                {
                    HBD_CORE_WARN("MeshImporter: cannot resolve referenced texture {}", texture_source_path);
                    return AssetID{};
                }

                ImportRequest texture_request{};
                texture_request.source_path = texture_source_path;
                texture_request.hash = makeSimpleHash(*texture_native);
                texture_request.preferred_type = AssetType::Texture2D;

                ImportResult texture_result = texture_importer.importAsset(texture_request, registry, vfs);
                if (!texture_result.success)
                {
                    HBD_CORE_WARN("MeshImporter: texture import failed for {} ({})",
                                  texture_source_path,
                                  texture_result.message);
                    return AssetID{};
                }

                imported_texture_ids[texture_source_path] = texture_result.primary_id;
                appendUniqueAssets(generated_assets, texture_result.assets, generated_asset_ids);
                return texture_result.primary_id;
            };

            const std::string albedo_map_path = buildReferencedAssetPath(src_rel, src_material.diffuse_texname);
            const std::string normal_ref = !src_material.normal_texname.empty() ? src_material.normal_texname
                                                                                : src_material.bump_texname;
            const std::string normal_map_path = buildReferencedAssetPath(src_rel, normal_ref);
            const std::string ao_map_path = buildReferencedAssetPath(src_rel, src_material.ambient_texname);
            const std::string emissive_map_path = buildReferencedAssetPath(src_rel, src_material.emissive_texname);

            const AssetID albedo_map_id = importTextureRef(src_material.diffuse_texname);
            const AssetID normal_map_id = importTextureRef(normal_ref);
            const AssetID ao_map_id = importTextureRef(src_material.ambient_texname);
            const AssetID emissive_map_id = importTextureRef(src_material.emissive_texname);

            json material_doc;
            material_doc["version"] = 1;
            material_doc["type"] = "Material";
            material_doc["name"] = material_name;
            material_doc["albedo_color"] = {
                static_cast<float>(src_material.diffuse[0]),
                static_cast<float>(src_material.diffuse[1]),
                static_cast<float>(src_material.diffuse[2]),
                static_cast<float>(src_material.dissolve)};
            material_doc["metallic"] = static_cast<float>(src_material.metallic);
            material_doc["roughness"] = (src_material.roughness > 0.0f) ? static_cast<float>(src_material.roughness)
                                                                        : 1.0f;
            material_doc["ao"] = 1.0f;
            material_doc["emissive"] = static_cast<float>(std::max({src_material.emission[0],
                                                                    src_material.emission[1],
                                                                    src_material.emission[2]}));
            if (albedo_map_id.value != 0)
                material_doc["albedo_map_id"] = std::to_string(albedo_map_id.value);
            if (normal_map_id.value != 0)
                material_doc["normal_map_id"] = std::to_string(normal_map_id.value);
            if (ao_map_id.value != 0)
                material_doc["ao_map_id"] = std::to_string(ao_map_id.value);
            if (emissive_map_id.value != 0)
                material_doc["emissive_map_id"] = std::to_string(emissive_map_id.value);
            material_doc["albedo_map_path"] = albedo_map_path;
            material_doc["normal_map_path"] = normal_map_path;
            material_doc["metallic_roughness_map_path"] = "";
            material_doc["ao_map_path"] = ao_map_path;
            material_doc["emissive_map_path"] = emissive_map_path;

            auto material_native = vfs.resolveForWrite(material_source_path);
            if (!material_native)
            {
                out.message = "MeshImporter: cannot resolve material source path";
                return out;
            }

            std::error_code material_ec;
            std::filesystem::create_directories(material_native->parent_path(), material_ec);
            if (material_ec)
            {
                out.message = "MeshImporter: create material directory failed";
                return out;
            }

            if (!previous_material_source_path.empty() && previous_material_source_path != material_source_path)
            {
                if (auto previous_native = vfs.resolve(previous_material_source_path))
                {
                    std::error_code remove_ec;
                    std::filesystem::remove(*previous_native, remove_ec);
                }
            }

            {
                std::ofstream material_ofs(*material_native, std::ios::binary | std::ios::trunc);
                if (!material_ofs)
                {
                    out.message = "MeshImporter: open material file failed";
                    return out;
                }
                const std::string material_json = material_doc.dump(2);
                material_ofs.write(material_json.data(), static_cast<std::streamsize>(material_json.size()));
                if (!material_ofs.good())
                {
                    out.message = "MeshImporter: write material file failed";
                    return out;
                }
            }

            material_meta.type = AssetType::Material;
            material_meta.source_path = material_source_path;
            material_meta.cooked_path.clear();
            material_meta.hash = request.hash + "|mtl:" + std::to_string(material_index) + ":" + material_name;
            material_meta.parent_id = mesh_asset_id;
            material_meta.subasset_key = subasset_key;
            material_meta.is_valid = true;
            material_meta.hard_deps.clear();
            material_meta.soft_deps.clear();
            if (albedo_map_id.value != 0)
                material_meta.hard_deps.push_back(albedo_map_id);
            if (normal_map_id.value != 0)
                material_meta.hard_deps.push_back(normal_map_id);
            if (ao_map_id.value != 0)
                material_meta.hard_deps.push_back(ao_map_id);
            if (emissive_map_id.value != 0)
                material_meta.hard_deps.push_back(emissive_map_id);

            material_index_to_id[material_index] = material_meta.id;
            if (generated_asset_ids.insert(material_meta.id.value).second)
                generated_assets.push_back(std::move(material_meta));
        }

        auto& submeshes = obj_build.mesh.submeshes();
        for (size_t i = 0; i < submeshes.size() && i < obj_build.submesh_material_indices.size(); ++i)
        {
            const int material_index = obj_build.submesh_material_indices[i];
            if (material_index < 0)
                continue;

            auto it = material_index_to_id.find(material_index);
            if (it != material_index_to_id.end())
                submeshes[i].material = it->second;
        }

        const std::string cooked_path = request.cooked_path.empty() ? buildDefaultCookedPath(request.source_path)
                                                                    : request.cooked_path;
        auto cooked_native = vfs.resolveForWrite(cooked_path);
        if (!cooked_native)
        {
            out.message = "MeshImporter: cannot resolve cooked path";
            return out;
        }

        std::vector<char> cooked_bytes;
        if (!HmeshEncode(obj_build.mesh, cooked_bytes))
        {
            out.message = "MeshImporter: encode cooked mesh failed";
            return out;
        }

        std::error_code ec;
        std::filesystem::create_directories(cooked_native->parent_path(), ec);
        if (ec)
        {
            out.message = "MeshImporter: create cooked directory failed";
            return out;
        }

        {
            std::ofstream ofs(*cooked_native, std::ios::binary | std::ios::trunc);
            if (!ofs)
            {
                out.message = "MeshImporter: open cooked file failed";
                return out;
            }

            ofs.write(cooked_bytes.data(), static_cast<std::streamsize>(cooked_bytes.size()));
            if (!ofs.good())
            {
                out.message = "MeshImporter: write cooked file failed";
                return out;
            }
        }

        AssetMetadata mesh_meta{};
        if (const auto* existing = registry.findByPath(request.source_path))
            mesh_meta = *existing;
        else
            mesh_meta.id = mesh_asset_id;

        mesh_meta.type = AssetType::Mesh;
        mesh_meta.source_path = request.source_path;
        mesh_meta.cooked_path = cooked_path;
        mesh_meta.hash = request.hash;
        mesh_meta.parent_id = {};
        mesh_meta.subasset_key.clear();
        mesh_meta.is_valid = true;
        mesh_meta.hard_deps.clear();
        mesh_meta.soft_deps.clear();
        for (int material_index : sorted_material_indices)
        {
            auto it = material_index_to_id.find(material_index);
            if (it != material_index_to_id.end())
                mesh_meta.hard_deps.push_back(it->second);
        }

        out.success = true;
        out.primary_id = mesh_meta.id;
        out.assets.push_back(std::move(mesh_meta));
        for (auto& asset_meta : generated_assets)
            out.assets.push_back(std::move(asset_meta));

        HBD_CORE_INFO("MeshImporter OBJ: generated {} material sub-assets for {}",
                      material_index_to_id.size(),
                      request.source_path);
        return out;
    }
} // namespace Hybrid
