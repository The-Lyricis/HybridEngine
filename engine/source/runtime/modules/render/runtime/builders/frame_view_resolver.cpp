#include "frame_view_resolver.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "runtime/modules/scene/components.h"
#include "runtime/modules/scene/scene.h"

namespace Hybrid
{
    namespace
    {
        bool getSceneCameraMatrices(Scene& scene,
                                    float aspect,
                                    glm::mat4& out_view,
                                    glm::mat4& out_proj,
                                    glm::vec3& out_cam_pos,
                                    glm::vec4* out_clear_color = nullptr,
                                    bool* out_use_skybox_clear = nullptr)
        {
            auto& reg = scene.getRegistry();
            auto view = reg.view<TransformComponent, CameraComponent>();

            entt::entity main_cam = entt::null;
            for (auto e : view)
            {
                auto& cam = view.get<CameraComponent>(e);
                if (!cam.Enabled)
                    continue;
                if (cam.Primary)
                {
                    main_cam = e;
                    break;
                }
            }
            if (main_cam == entt::null)
            {
                for (auto e : view)
                {
                    auto& cam = view.get<CameraComponent>(e);
                    if (cam.Enabled)
                    {
                        main_cam = e;
                        break;
                    }
                }
            }
            if (main_cam == entt::null)
                return false;

            const auto& tr = reg.get<TransformComponent>(main_cam);
            const auto& cam = reg.get<CameraComponent>(main_cam);

            out_proj = glm::perspective(glm::radians(cam.FovY), aspect, cam.Near, cam.Far);
            out_view = glm::inverse(tr.WorldMatrix);
            out_cam_pos = glm::vec3(tr.WorldMatrix[3]);
            if (out_clear_color)
                *out_clear_color = cam.ClearColor;
            if (out_use_skybox_clear)
                *out_use_skybox_clear = (cam.ClearMode == CameraClearMode::Skybox);
            return true;
        }

        glm::vec3 lightDirectionFromTransform(const TransformComponent& tr)
        {
            const glm::vec3 dir = glm::vec3(tr.WorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            const float len = glm::length(dir);
            if (len < 1e-4f)
                return glm::vec3(0.0f, -1.0f, 0.0f);
            return dir / len;
        }

        void resolveMainDirectionalLight(const std::shared_ptr<Scene>& scene, RenderDirLightData& out_light)
        {
            out_light = {};
            if (!scene)
                return;

            auto& registry = scene->getRegistry();
            auto dir_view = registry.view<TransformComponent, DirectionalLightComponent>();
            for (auto entity : dir_view)
            {
                const auto& transform = dir_view.get<TransformComponent>(entity);
                const auto& light = dir_view.get<DirectionalLightComponent>(entity);
                if (!light.Enabled)
                    continue;

                out_light.color = light.Color;
                out_light.intensity = light.Intensity;
                out_light.direction = lightDirectionFromTransform(transform);
                return;
            }
        }
    } // namespace

    FrameViewResolveResult FrameViewResolver::resolve(const FrameViewResolveInput& input) const
    {
        FrameViewResolveResult result{};
        result.view.flags = input.flags;
        result.view.frame.clearColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
        result.view.frame.useSkyboxClear = false;

        const glm::vec2 viewport_size = input.frame ? input.frame->viewport_size : glm::vec2(0.0f);
        const float aspect = (viewport_size.y > 0.0f) ? (viewport_size.x / viewport_size.y) : 1.0f;

        glm::mat4 view_m(1.0f), proj_m(1.0f);
        glm::vec3 camera_pos(0.0f, 0.0f, 3.0f);

        bool use_game_camera = true;
        if (input.editor_ext)
            use_game_camera = input.editor_ext->use_game_camera;

        if (use_game_camera && input.scene)
        {
            result.has_camera = getSceneCameraMatrices(*input.scene,
                                                       aspect,
                                                       view_m,
                                                       proj_m,
                                                       camera_pos,
                                                       &result.view.frame.clearColor,
                                                       &result.view.frame.useSkyboxClear);
        }
        else if (!use_game_camera && input.editor_ext && input.editor_ext->has_editor_camera)
        {
            view_m = input.editor_ext->editor_view;
            proj_m = input.editor_ext->editor_proj;
            camera_pos = input.editor_ext->editor_camera_pos;
            result.has_camera = true;
            if (input.scene && input.scene->environment().skybox_cubemap.value != 0)
                result.view.frame.useSkyboxClear = true;
        }

        if (!result.has_camera && input.scene)
            result.has_camera = getSceneCameraMatrices(*input.scene, aspect, view_m, proj_m, camera_pos);

        if (!result.has_camera)
        {
            view_m = glm::mat4(1.0f);
            proj_m = glm::mat4(1.0f);
            camera_pos = glm::vec3(0.0f, 0.0f, 3.0f);
        }

        result.view.frame.view = view_m;
        result.view.frame.proj = proj_m;
        result.view.frame.viewProj = proj_m * view_m;
        result.view.frame.cameraPos = camera_pos;
        result.view.frame.time = input.frame ? input.frame->dt : 0.0f;
        if (input.editor_ext && input.editor_ext->game_viewport_size.x > 1.0f && input.editor_ext->game_viewport_size.y > 1.0f)
            result.view.frame.gameAspect = input.editor_ext->game_viewport_size.x / input.editor_ext->game_viewport_size.y;
        else
            result.view.frame.gameAspect = (aspect > 0.0f) ? aspect : (16.0f / 9.0f);

        resolveMainDirectionalLight(input.scene, result.view.mainDirectionalLight);

        if (input.scene)
        {
            const SceneEnvironmentSettings& environment = input.scene->environment();
            result.environment.skyboxCubemap = environment.skybox_cubemap;
            result.environment.skyboxIntensity = environment.skybox_intensity;
            result.environment.skyboxRotationDegrees = environment.skybox_rotation_degrees;
            if (environment.skybox_cubemap.value != 0 && input.resolve_cubemap)
                result.environment.skyboxTexture = input.resolve_cubemap(environment.skybox_cubemap);
        }
        if (result.view.frame.useSkyboxClear && !result.environment.skyboxTexture && input.resolve_cubemap)
            result.environment.skyboxTexture = input.resolve_cubemap({});

        return result;
    }
} // namespace Hybrid
