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

    namespace BuiltinAssets
    {
        const char* meshPath(BuiltinMesh mesh);
        const char* meshHash(BuiltinMesh mesh);
        std::shared_ptr<Mesh> createMesh(BuiltinMesh mesh);
    } // namespace BuiltinAssets
} // namespace Hybrid
