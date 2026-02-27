#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mesh.h"

namespace Hybrid
{
    bool HmeshEncode(const Mesh& mesh, std::vector<char>& out_bytes);
    bool HmeshDecode(const std::vector<char>& bytes, Mesh& out_mesh, std::string* out_error = nullptr);
    bool HmeshLooksLikeFile(const std::vector<char>& bytes);
} // namespace Hybrid

