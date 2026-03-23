#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
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

        static AABB Empty()
        {
            const float inf = std::numeric_limits<float>::infinity();
            return {{ inf,  inf,  inf}, {-inf, -inf, -inf}};
        }

        bool isValid() const
        {
            return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z;
        }

        void normalize()
        {
            if (Min.x > Max.x)
                std::swap(Min.x, Max.x);
            if (Min.y > Max.y)
                std::swap(Min.y, Max.y);
            if (Min.z > Max.z)
                std::swap(Min.z, Max.z);
        }

        AABB normalized() const
        {
            AABB result = *this;
            result.normalize();
            return result;
        }

        glm::vec3 center() const
        {
            return 0.5f * (Min + Max);
        }

        glm::vec3 size() const
        {
            if (!isValid())
                return glm::vec3(0.0f);
            return Max - Min;
        }

        glm::vec3 extents() const
        {
            return 0.5f * size();
        }

        void expand(const glm::vec3& point)
        {
            if (!isValid())
            {
                Min = point;
                Max = point;
                return;
            }

            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }

        void expand(const AABB& other)
        {
            if (!other.isValid())
                return;

            if (!isValid())
            {
                *this = other;
                return;
            }

            Min = glm::min(Min, other.Min);
            Max = glm::max(Max, other.Max);
        }

        bool contains(const glm::vec3& point) const
        {
            if (!isValid())
                return false;

            return point.x >= Min.x && point.x <= Max.x &&
                   point.y >= Min.y && point.y <= Max.y &&
                   point.z >= Min.z && point.z <= Max.z;
        }
    };

    struct AABBIntersection
    {
        bool Hit = false;
        glm::vec3 Normal{0.0f, 0.0f, 0.0f};
        float Penetration = 0.0f;
    };

    struct Plane
    {
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        float Distance = 0.0f;
    };

    struct Frustum
    {
        enum PlaneIndex : int
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count
        };

        Plane Planes[Count]{};
        bool Valid = false;
    };

    struct ConvexVolume
    {
        static constexpr uint32_t MaxPlanes = 32;

        std::array<Plane, MaxPlanes> Planes{};
        uint32_t PlaneCount = 0;
        bool Valid = false;
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

    inline AABB TransformAABB(const AABB& local_bounds, const glm::mat4& transform)
    {
        if (!local_bounds.isValid())
            return AABB{};

        const glm::vec3 local_center = local_bounds.center();
        const glm::vec3 local_extents = local_bounds.extents();
        const glm::vec3 world_center = glm::vec3(transform * glm::vec4(local_center, 1.0f));

        const glm::mat3 linear(transform);
        const glm::mat3 abs_linear(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
        const glm::vec3 world_extents = abs_linear * local_extents;

        return {world_center - world_extents, world_center + world_extents};
    }

    inline Plane NormalizePlane(const glm::vec4& plane)
    {
        const glm::vec3 normal(plane.x, plane.y, plane.z);
        const float len = glm::length(normal);
        if (len <= 1e-8f)
            return {};
        return {normal / len, plane.w / len};
    }

    inline Plane BuildPlaneFromPoints(const glm::vec3& a,
                                      const glm::vec3& b,
                                      const glm::vec3& c,
                                      const glm::vec3& inside_point)
    {
        glm::vec3 normal = glm::cross(b - a, c - a);
        const float len = glm::length(normal);
        if (len <= 1e-8f)
            return {};

        normal /= len;
        float distance = -glm::dot(normal, a);
        if (glm::dot(normal, inside_point) + distance < 0.0f)
        {
            normal = -normal;
            distance = -distance;
        }

        return {normal, distance};
    }

    inline ConvexVolume BuildExtrudedQuadPrism(const std::array<glm::vec3, 4>& receiver_quad,
                                               const std::array<glm::vec3, 4>& extruded_quad)
    {
        ConvexVolume volume{};
        glm::vec3 center(0.0f);
        for (const glm::vec3& point : receiver_quad)
            center += point;
        for (const glm::vec3& point : extruded_quad)
            center += point;
        center /= 8.0f;

        volume.Planes[0] = BuildPlaneFromPoints(receiver_quad[0], receiver_quad[1], receiver_quad[2], center);
        volume.Planes[1] = BuildPlaneFromPoints(extruded_quad[0], extruded_quad[3], extruded_quad[2], center);
        volume.Planes[2] = BuildPlaneFromPoints(receiver_quad[0], receiver_quad[1], extruded_quad[1], center);
        volume.Planes[3] = BuildPlaneFromPoints(receiver_quad[1], receiver_quad[2], extruded_quad[2], center);
        volume.Planes[4] = BuildPlaneFromPoints(receiver_quad[2], receiver_quad[3], extruded_quad[3], center);
        volume.Planes[5] = BuildPlaneFromPoints(receiver_quad[3], receiver_quad[0], extruded_quad[0], center);
        volume.PlaneCount = 6;
        volume.Valid = true;

        for (uint32_t i = 0; i < volume.PlaneCount; ++i)
        {
            if (glm::dot(volume.Planes[i].Normal, volume.Planes[i].Normal) <= 1e-8f)
            {
                volume.Valid = false;
                break;
            }
        }

        return volume;
    }

    template <size_t N>
    inline ConvexVolume BuildConvexHullVolume(const std::array<glm::vec3, N>& points,
                                              float epsilon = 1e-4f)
    {
        ConvexVolume volume{};
        if constexpr (N < 4)
            return volume;

        auto plane_exists = [&](const Plane& plane)
        {
            for (uint32_t i = 0; i < volume.PlaneCount; ++i)
            {
                const Plane& existing = volume.Planes[i];
                if (glm::dot(existing.Normal, plane.Normal) > 0.999f &&
                    std::abs(existing.Distance - plane.Distance) < 1e-3f)
                {
                    return true;
                }
            }
            return false;
        };

        for (size_t i = 0; i < N; ++i)
        {
            for (size_t j = i + 1; j < N; ++j)
            {
                for (size_t k = j + 1; k < N; ++k)
                {
                    glm::vec3 normal = glm::cross(points[j] - points[i], points[k] - points[i]);
                    const float len = glm::length(normal);
                    if (len <= 1e-6f)
                        continue;
                    normal /= len;

                    float distance = -glm::dot(normal, points[i]);
                    bool has_positive = false;
                    bool has_negative = false;
                    for (size_t p = 0; p < N; ++p)
                    {
                        const float signed_distance = glm::dot(normal, points[p]) + distance;
                        if (signed_distance > epsilon)
                            has_positive = true;
                        else if (signed_distance < -epsilon)
                            has_negative = true;

                        if (has_positive && has_negative)
                            break;
                    }

                    if (has_positive && has_negative)
                        continue;

                    if (!has_positive)
                    {
                        normal = -normal;
                        distance = -distance;
                    }

                    const Plane candidate{normal, distance};
                    if (plane_exists(candidate))
                        continue;

                    if (volume.PlaneCount >= ConvexVolume::MaxPlanes)
                    {
                        volume.Valid = false;
                        return volume;
                    }

                    volume.Planes[volume.PlaneCount++] = candidate;
                }
            }
        }

        volume.Valid = volume.PlaneCount >= 4;
        return volume;
    }

    inline Frustum BuildFrustum(const glm::mat4& view_proj)
    {
        Frustum frustum{};
        const glm::mat4 rows = glm::transpose(view_proj);

        frustum.Planes[Frustum::Left]   = NormalizePlane(rows[3] + rows[0]);
        frustum.Planes[Frustum::Right]  = NormalizePlane(rows[3] - rows[0]);
        frustum.Planes[Frustum::Bottom] = NormalizePlane(rows[3] + rows[1]);
        frustum.Planes[Frustum::Top]    = NormalizePlane(rows[3] - rows[1]);
        frustum.Planes[Frustum::Near]   = NormalizePlane(rows[3] + rows[2]);
        frustum.Planes[Frustum::Far]    = NormalizePlane(rows[3] - rows[2]);

        frustum.Valid = true;
        for (const Plane& plane : frustum.Planes)
        {
            if (glm::dot(plane.Normal, plane.Normal) <= 1e-8f)
            {
                frustum.Valid = false;
                break;
            }
        }

        return frustum;
    }

    inline bool IntersectsFrustum(const Frustum& frustum, const AABB& bounds)
    {
        if (!frustum.Valid || !bounds.isValid())
            return true;

        const glm::vec3 center = bounds.center();
        const glm::vec3 extents = bounds.extents();

        for (const Plane& plane : frustum.Planes)
        {
            const glm::vec3 abs_normal = glm::abs(plane.Normal);
            const float projected_radius = glm::dot(abs_normal, extents);
            const float signed_distance = glm::dot(plane.Normal, center) + plane.Distance;
            if (signed_distance + projected_radius < 0.0f)
                return false;
        }

        return true;
    }

    inline bool IntersectsConvexVolume(const ConvexVolume& volume, const AABB& bounds)
    {
        if (!volume.Valid || !bounds.isValid())
            return true;

        const glm::vec3 center = bounds.center();
        const glm::vec3 extents = bounds.extents();

        for (uint32_t i = 0; i < volume.PlaneCount; ++i)
        {
            const Plane& plane = volume.Planes[i];
            const glm::vec3 abs_normal = glm::abs(plane.Normal);
            const float projected_radius = glm::dot(abs_normal, extents);
            const float signed_distance = glm::dot(plane.Normal, center) + plane.Distance;
            if (signed_distance + projected_radius < 0.0f)
                return false;
        }

        return true;
    }

    inline AABBIntersection IntersectAABB(const AABB& a, const AABB& b)
    {
        AABBIntersection hit{};

        const AABB lhs = a.normalized();
        const AABB rhs = b.normalized();

        if (!lhs.isValid() || !rhs.isValid())
            return hit;

        const float overlap_x = std::min(lhs.Max.x, rhs.Max.x) - std::max(lhs.Min.x, rhs.Min.x);
        if (overlap_x <= 0.0f)
            return hit;

        const float overlap_y = std::min(lhs.Max.y, rhs.Max.y) - std::max(lhs.Min.y, rhs.Min.y);
        if (overlap_y <= 0.0f)
            return hit;

        const float overlap_z = std::min(lhs.Max.z, rhs.Max.z) - std::max(lhs.Min.z, rhs.Min.z);
        if (overlap_z <= 0.0f)
            return hit;

        hit.Hit = true;

        const glm::vec3 center_a = lhs.center();
        const glm::vec3 center_b = rhs.center();
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
