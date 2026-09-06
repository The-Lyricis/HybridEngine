#pragma once

#include <cstdint>
#include <memory>
#include <array>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/modules/asset/asset_type.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/render/runtime/render_frame_request.h"
#include "runtime/modules/render/runtime/material_system.h"
#include "runtime/modules/render/runtime/mesh_gpu.h"
#include "runtime/modules/render/runtime/render_shadow_settings.h"

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
        float gameAspect = 16.0f / 9.0f;
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

    struct RenderEnvironmentData
    {
        AssetID skyboxCubemap{};
        TexturePtr skyboxTexture;
        float skyboxIntensity = 1.0f;
        float skyboxRotationDegrees = 0.0f;
    };

    struct RenderShadowData
    {
        struct Cascade
        {
            bool valid = false;
            glm::mat4 lightViewProjection{1.0f};
            glm::vec3 receiverMinLS{0.0f};
            glm::vec3 receiverMaxLS{0.0f};
            glm::vec3 casterMinLS{0.0f};
            glm::vec3 casterMaxLS{0.0f};
            std::array<glm::vec3, 8> receiverCornersWS{};
            std::array<glm::vec3, 8> casterExtrudedCornersWS{};
            float splitNear = 0.0f;
            float splitFar = 0.0f;
        };

        bool enabled = false;
        uint32_t cascadeCount = 0;
        std::array<Cascade, kMaxDirectionalShadowCascades> cascades{};
        glm::vec3 lightDirection{0.0f, -1.0f, 0.0f};
        float strength = 1.0f;
        float biasConstant = 0.0005f;
        float biasSlope = 0.0015f;
    };

    struct RenderDrawItem
    {
        AssetID meshId{};
        AssetID materialId{};
        MeshGPU* meshGPU = nullptr;
        const MaterialSystem::MaterialGPU* materialGPU = nullptr;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        glm::mat4 model{1.0f};
        glm::vec4 tint{1.0f};
        uint32_t entityID = 0;
    };

    struct RenderPacket
    {
        RenderFrameData frame;
        RenderLightData lights;
        RenderEnvironmentData environment;
        RenderShadowData shadow;
        std::vector<RenderDrawItem> opaque_items;
        std::vector<RenderDrawItem> transparent_items;
        std::vector<RenderDrawItem> shadow_caster_items;
        uint32_t scene_renderers = 0;
        uint32_t scene_submeshes = 0;
        uint32_t tested_items = 0;
        uint32_t culled_items = 0;
        bool showColliderDebug = false;
        bool showShadowDebug = false;
        uint32_t activeEntityID = kInvalidEntityID;
        std::shared_ptr<Scene> scene;
    };
} // namespace Hybrid
