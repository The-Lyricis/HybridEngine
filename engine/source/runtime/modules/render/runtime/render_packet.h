#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/render/runtime/editor_render_ext.h"

namespace Hybrid
{
    class Scene;

    struct RenderFrameData
    {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
        glm::mat4 viewProj{1.0f};
        glm::vec3 cameraPos{0.0f};
        glm::vec4 clearColor{0.1f, 0.1f, 0.12f, 1.0f};
        bool useSkyboxClear = false;
        float time = 0.0f;
    };

    struct RenderDirLightData
    {
        glm::vec3 color{1.0f};
        float intensity = 0.0f;
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        float pad = 0.0f;
    };

    struct RenderPointLightData
    {
        glm::vec3 color{1.0f};
        float intensity = 0.0f;
        glm::vec3 position{0.0f};
        float range = 1.0f;
    };

    struct RenderLightData
    {
        RenderDirLightData dir;
        std::vector<RenderPointLightData> points;
    };

    struct RenderDrawItem
    {
        AssetID meshId{};
        AssetID materialId{};
        glm::mat4 model{1.0f};
        glm::vec4 tint{1.0f};
        uint32_t entityID = 0;
    };

    struct RenderPacket
    {
        RenderFrameData frame;
        RenderLightData lights;
        std::vector<RenderDrawItem> items;
        bool showColliderDebug = false;
        uint32_t activeEntityID = kInvalidEntityID;
        std::shared_ptr<Scene> scene;
    };
} // namespace Hybrid
