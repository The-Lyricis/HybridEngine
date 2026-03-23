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

        const char* cubemapPath(BuiltinCubemap cubemap)
        {
            switch (cubemap)
            {
            case BuiltinCubemap::DefaultSky:
                return "engine:cubemaps/default_sky/default_sky.hcube.json";
            default:
                return "";
            }
        }

        const char* cubemapHash(BuiltinCubemap cubemap)
        {
            switch (cubemap)
            {
            case BuiltinCubemap::DefaultSky:
                return "builtin_default_sky_v1";
            default:
                return "";
            }
        }
    } // namespace BuiltinAssets
} // namespace Hybrid
