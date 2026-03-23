#pragma once

#include <memory>

#include "asset_type.h"

namespace Hybrid
{
    class Mesh;

    enum class BuiltinMesh : uint8_t
    {
        Cube = 0
    };

    enum class BuiltinCubemap : uint8_t
    {
        DefaultSky = 0
    };

    namespace BuiltinAssets
    {
        const char* meshPath(BuiltinMesh mesh);
        const char* meshHash(BuiltinMesh mesh);
        std::shared_ptr<Mesh> createMesh(BuiltinMesh mesh);
        const char* cubemapPath(BuiltinCubemap cubemap);
        const char* cubemapHash(BuiltinCubemap cubemap);
    } // namespace BuiltinAssets
} // namespace Hybrid
