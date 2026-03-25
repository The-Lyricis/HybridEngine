#include "shadow_frame_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/core/base/math_util.h"

namespace Hybrid
{
    namespace
    {
        std::array<glm::vec3, 8> buildCameraFrustumSliceCorners(const glm::mat4& view,
                                                                const glm::mat4& proj,
                                                                float near_distance,
                                                                float far_distance)
        {
            std::array<glm::vec3, 8> corners{};
            const glm::mat4 inv_view = glm::inverse(view);
            const float tan_half_fov = 1.0f / std::max(proj[1][1], 1e-6f);
            const float aspect = proj[1][1] / std::max(proj[0][0], 1e-6f);

            const auto write_plane = [&](float distance, size_t base_index)
            {
                const float half_height = distance * tan_half_fov;
                const float half_width = half_height * aspect;

                const std::array<glm::vec3, 4> local = {
                    glm::vec3(-half_width, -half_height, -distance),
                    glm::vec3(half_width, -half_height, -distance),
                    glm::vec3(half_width, half_height, -distance),
                    glm::vec3(-half_width, half_height, -distance),
                };

                for (size_t i = 0; i < local.size(); ++i)
                    corners[base_index + i] = glm::vec3(inv_view * glm::vec4(local[i], 1.0f));
            };

            write_plane(near_distance, 0);
            write_plane(far_distance, 4);
            return corners;
        }

        void computeLightSpaceBounds(const std::array<glm::vec3, 8>& corners,
                                     const glm::mat4& light_view,
                                     glm::vec3& out_min_ls,
                                     glm::vec3& out_max_ls)
        {
            out_min_ls = glm::vec3(std::numeric_limits<float>::max());
            out_max_ls = glm::vec3(std::numeric_limits<float>::lowest());
            for (const glm::vec3& corner : corners)
            {
                const glm::vec3 ls = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                out_min_ls = glm::min(out_min_ls, ls);
                out_max_ls = glm::max(out_max_ls, ls);
            }
        }
    } // namespace

    void ShadowFrameBuilder::build(const ShadowFrameBuildInput& input, RenderShadowData& out_shadow) const
    {
        out_shadow = {};

        if (!input.view || !input.settings)
            return;

        if (!HasFlag(input.view->flags, RenderFlags::Shadow))
            return;

        if (input.view->mainDirectionalLight.intensity <= 0.0f)
            return;

        const DirectionalShadowSettings& settings = *input.settings;
        const glm::vec3 light_dir =
            MathUtil::normalize(input.view->mainDirectionalLight.direction, glm::vec3(0.0f, -1.0f, 0.0f));

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(up, light_dir)) > 0.98f)
            up = glm::vec3(0.0f, 0.0f, 1.0f);

        out_shadow.enabled = true;
        out_shadow.lightDirection = light_dir;
        out_shadow.strength = settings.strength;
        out_shadow.biasConstant = settings.bias_constant;
        out_shadow.biasSlope = settings.bias_slope;

        const uint32_t cascade_count = std::clamp(settings.cascade_count, 1u, kMaxDirectionalShadowCascades);
        out_shadow.cascadeCount = cascade_count;

        float cascade_near = settings.near_distance;
        for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index)
        {
            const float split_ratio = std::clamp(settings.cascade_split_ratios[cascade_index], 0.0f, 1.0f);
            float cascade_far =
                settings.near_distance + (settings.far_distance - settings.near_distance) * split_ratio;
            if (cascade_index == cascade_count - 1)
                cascade_far = settings.far_distance;
            cascade_far = std::max(cascade_far, cascade_near + 0.01f);

            const auto frustum_corners = buildCameraFrustumSliceCorners(input.view->frame.view,
                                                                        input.view->frame.proj,
                                                                        cascade_near,
                                                                        cascade_far);
            std::array<glm::vec3, 8> extruded_corners = frustum_corners;
            for (glm::vec3& corner : extruded_corners)
                corner -= light_dir * settings.caster_back_padding;

            glm::vec3 frustum_center(0.0f);
            for (const glm::vec3& corner : frustum_corners)
                frustum_center += corner;
            frustum_center /= static_cast<float>(frustum_corners.size());

            const glm::vec3 eye = frustum_center - light_dir * settings.light_distance;
            glm::mat4 light_view = glm::lookAt(eye, frustum_center, up);

            const glm::vec3 receiver_center_ls = glm::vec3(light_view * glm::vec4(frustum_center, 1.0f));
            float receiver_radius_ls = 0.0f;
            for (const glm::vec3& corner : frustum_corners)
            {
                const glm::vec3 corner_ls = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                const glm::vec2 delta = glm::vec2(corner_ls) - glm::vec2(receiver_center_ls);
                receiver_radius_ls = std::max(receiver_radius_ls, std::max(std::abs(delta.x), std::abs(delta.y)));
            }

            receiver_radius_ls += settings.receiver_margin_xy;
            receiver_radius_ls += settings.projection_margin_xy;
            receiver_radius_ls = std::max(receiver_radius_ls, 0.5f);

            const float ortho_extent = receiver_radius_ls * 2.0f;
            const float texel_size = ortho_extent / static_cast<float>(settings.map_resolution);

            glm::vec3 snapped_center_ls = receiver_center_ls;
            snapped_center_ls.x = std::round(snapped_center_ls.x / texel_size) * texel_size;
            snapped_center_ls.y = std::round(snapped_center_ls.y / texel_size) * texel_size;

            const glm::vec3 snap_offset_ls = snapped_center_ls - receiver_center_ls;
            light_view = glm::translate(glm::mat4(1.0f), glm::vec3(snap_offset_ls.x, snap_offset_ls.y, 0.0f)) *
                         light_view;

            glm::vec3 receiver_min_ls{};
            glm::vec3 receiver_max_ls{};
            computeLightSpaceBounds(frustum_corners, light_view, receiver_min_ls, receiver_max_ls);

            glm::vec3 snapped_receiver_min_ls(receiver_min_ls);
            glm::vec3 snapped_receiver_max_ls(receiver_max_ls);
            snapped_receiver_min_ls.x = snapped_center_ls.x - receiver_radius_ls;
            snapped_receiver_max_ls.x = snapped_center_ls.x + receiver_radius_ls;
            snapped_receiver_min_ls.y = snapped_center_ls.y - receiver_radius_ls;
            snapped_receiver_max_ls.y = snapped_center_ls.y + receiver_radius_ls;

            glm::vec3 extruded_min_ls{};
            glm::vec3 extruded_max_ls{};
            computeLightSpaceBounds(extruded_corners, light_view, extruded_min_ls, extruded_max_ls);

            glm::vec3 snapped_caster_min_ls = glm::min(snapped_receiver_min_ls, extruded_min_ls);
            glm::vec3 snapped_caster_max_ls = glm::max(snapped_receiver_max_ls, extruded_max_ls);
            snapped_caster_max_ls.z += settings.receiver_front_padding;

            const glm::mat4 light_proj = glm::ortho(snapped_receiver_min_ls.x,
                                                    snapped_receiver_max_ls.x,
                                                    snapped_receiver_min_ls.y,
                                                    snapped_receiver_max_ls.y,
                                                    -snapped_caster_max_ls.z,
                                                    -snapped_caster_min_ls.z);

            auto& cascade = out_shadow.cascades[cascade_index];
            cascade.valid = true;
            cascade.lightViewProjection = light_proj * light_view;
            cascade.receiverMinLS = snapped_receiver_min_ls;
            cascade.receiverMaxLS = snapped_receiver_max_ls;
            cascade.casterMinLS = snapped_caster_min_ls;
            cascade.casterMaxLS = snapped_caster_max_ls;
            cascade.receiverCornersWS = frustum_corners;
            cascade.casterExtrudedCornersWS = extruded_corners;
            cascade.splitNear = cascade_near;
            cascade.splitFar = cascade_far;

            cascade_near = cascade_far;
        }
    }
} // namespace Hybrid
