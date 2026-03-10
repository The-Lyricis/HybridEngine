#pragma once

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Hybrid
{
    struct Ray
    {
        glm::vec3 origin{0.0f, 0.0f, 0.0f};
        glm::vec3 dir{0.0f, 0.0f, -1.0f};
    };

    struct AABB
    {
        glm::vec3 Min{0.0f, 0.0f, 0.0f};
        glm::vec3 Max{0.0f, 0.0f, 0.0f};
    };

    struct AABBIntersection
    {
        bool Hit = false;
        glm::vec3 Normal{0.0f, 0.0f, 0.0f};
        float Penetration = 0.0f;
    };

    inline Ray MakeRayFromInvViewProjection(const glm::mat4& inv_view_proj, float ndc_x, float ndc_y)
    {
        glm::vec4 near_world = inv_view_proj * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
        glm::vec4 far_world = inv_view_proj * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);

        if (near_world.w != 0.0f)
            near_world /= near_world.w;
        if (far_world.w != 0.0f)
            far_world /= far_world.w;

        Ray ray{};
        ray.origin = glm::vec3(near_world);

        const glm::vec3 direction = glm::vec3(far_world - near_world);
        const float direction_len_sq = glm::dot(direction, direction);
        ray.dir = (direction_len_sq > 1e-8f) ? glm::normalize(direction) : glm::vec3(0.0f, -1.0f, 0.0f);
        return ray;
    }

    inline bool IntersectPlane(const Ray& ray,
                               const glm::vec3& plane_normal,
                               const glm::vec3& plane_point,
                               float& out_t,
                               glm::vec3& out_hit,
                               float parallel_epsilon = 1e-4f)
    {
        const float normal_len_sq = glm::dot(plane_normal, plane_normal);
        if (normal_len_sq <= 1e-8f)
            return false;

        const glm::vec3 normal = glm::normalize(plane_normal);
        const float denom = glm::dot(ray.dir, normal);
        if (std::abs(denom) < parallel_epsilon)
            return false;

        const float t = glm::dot(plane_point - ray.origin, normal) / denom;
        if (t < 0.0f)
            return false;

        out_t = t;
        out_hit = ray.origin + ray.dir * t;
        return true;
    }

    inline bool IntersectPlaneY0(const Ray& ray,
                                 glm::vec3& out_hit,
                                 float* out_t = nullptr,
                                 float parallel_epsilon = 1e-4f)
    {
        float t = 0.0f;
        if (!IntersectPlane(ray,
                            glm::vec3(0.0f, 1.0f, 0.0f),
                            glm::vec3(0.0f, 0.0f, 0.0f),
                            t,
                            out_hit,
                            parallel_epsilon))
        {
            return false;
        }

        if (out_t)
            *out_t = t;
        return true;
    }

    inline AABBIntersection IntersectAABB(const AABB& a, const AABB& b)
    {
        AABBIntersection hit{};

        const float overlap_x = std::min(a.Max.x, b.Max.x) - std::max(a.Min.x, b.Min.x);
        if (overlap_x <= 0.0f)
            return hit;

        const float overlap_y = std::min(a.Max.y, b.Max.y) - std::max(a.Min.y, b.Min.y);
        if (overlap_y <= 0.0f)
            return hit;

        const float overlap_z = std::min(a.Max.z, b.Max.z) - std::max(a.Min.z, b.Min.z);
        if (overlap_z <= 0.0f)
            return hit;

        hit.Hit = true;

        const glm::vec3 center_a = 0.5f * (a.Min + a.Max);
        const glm::vec3 center_b = 0.5f * (b.Min + b.Max);
        const glm::vec3 delta = center_b - center_a;

        hit.Penetration = overlap_x;
        hit.Normal = glm::vec3((delta.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);

        if (overlap_y < hit.Penetration)
        {
            hit.Penetration = overlap_y;
            hit.Normal = glm::vec3(0.0f, (delta.y >= 0.0f) ? 1.0f : -1.0f, 0.0f);
        }

        if (overlap_z < hit.Penetration)
        {
            hit.Penetration = overlap_z;
            hit.Normal = glm::vec3(0.0f, 0.0f, (delta.z >= 0.0f) ? 1.0f : -1.0f);
        }

        return hit;
    }
} // namespace Hybrid
