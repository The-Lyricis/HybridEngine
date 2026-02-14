#include "asset_type.h"

namespace Hybrid
{
    AssetType AssetTypeFromString(const std::string& name)
    {
        if (name == "Texture2D") return AssetType::Texture2D;
        if (name == "TextureCube") return AssetType::TextureCube;
        if (name == "Mesh") return AssetType::Mesh;
        if (name == "Material") return AssetType::Material;
        if (name == "Shader") return AssetType::Shader;
        if (name == "Scene") return AssetType::Scene;
        if (name == "Audio") return AssetType::Audio;
        if (name == "Animation") return AssetType::Animation;
        if (name == "Script") return AssetType::Script;
        return AssetType::Unknown;
    }

    std::string AssetTypeToString(AssetType type)
    {
        switch (type)
        {
        case AssetType::Texture2D: return "Texture2D";
        case AssetType::TextureCube: return "TextureCube";
        case AssetType::Mesh: return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Shader: return "Shader";
        case AssetType::Scene: return "Scene";
        case AssetType::Audio: return "Audio";
        case AssetType::Animation: return "Animation";
        case AssetType::Script: return "Script";
        default: return "Unknown";
        }
    }
} // namespace Hybrid

