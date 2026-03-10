#include "builtin_assets.h"

#include "mesh.h"

namespace Hybrid
{
    namespace BuiltinAssets
    {
        const char* meshPath(BuiltinMesh mesh)
        {
            switch (mesh)
            {
            case BuiltinMesh::Cube:
                return "builtin:Cube";
            default:
                return "";
            }
        }

        const char* meshHash(BuiltinMesh mesh)
        {
            switch (mesh)
            {
            case BuiltinMesh::Cube:
                return "builtin_cube_v1";
            default:
                return "";
            }
        }

        std::shared_ptr<Mesh> createMesh(BuiltinMesh mesh)
        {
            switch (mesh)
            {
            case BuiltinMesh::Cube:
                return Mesh::CreateCube();
            default:
                return nullptr;
            }
        }
    } // namespace BuiltinAssets
} // namespace Hybrid
