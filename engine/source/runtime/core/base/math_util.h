#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hybrid
{
    namespace MathUtil
    {
        inline float degToRad(float degrees)
        {
            return glm::radians(degrees);
        }

        inline glm::vec3 degToRad(const glm::vec3& degrees)
        {
            return glm::radians(degrees);
        }

        inline float radToDeg(float radians)
        {
            return glm::degrees(radians);
        }

        inline glm::vec3 radToDeg(const glm::vec3& radians)
        {
            return glm::degrees(radians);
        }

        inline bool nearlyZero(float value, float epsilon = 1e-5f)
        {
            return std::abs(value) <= epsilon;
        }

        inline bool nearlyEqual(float lhs, float rhs, float epsilon = 1e-5f)
        {
            return std::abs(lhs - rhs) <= epsilon;
        }

        inline glm::vec3 normalize(const glm::vec3& value, const glm::vec3& fallback = glm::vec3(0.0f, 0.0f, -1.0f))
        {
            const float len2 = glm::dot(value, value);
            if (len2 <= 1e-8f)
                return fallback;
            return glm::normalize(value);
        }

        inline glm::quat normalizeQuat(const glm::quat& rotation)
        {
            const float len2 = glm::dot(rotation, rotation);
            if (len2 <= 0.0f)
            {
                return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
            }
            return glm::normalize(rotation);
        }

        inline glm::mat4 mat4FromQuat(const glm::quat& rotation)
        {
            return glm::mat4_cast(normalizeQuat(rotation));
        }

        // Engine convention: euler = (pitch X, yaw Y, roll Z), composed as Y * X * Z.
        inline glm::quat quatFromEulerRadians(const glm::vec3& euler_rad)
        {
            const glm::quat yaw = glm::angleAxis(euler_rad.y, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::quat pitch = glm::angleAxis(euler_rad.x, glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::quat roll = glm::angleAxis(euler_rad.z, glm::vec3(0.0f, 0.0f, 1.0f));
            return normalizeQuat(yaw * pitch * roll);
        }

        inline glm::vec3 eulerRadiansFromQuat(const glm::quat& rotation)
        {
            return glm::eulerAngles(normalizeQuat(rotation));
        }

        inline glm::vec3 eulerDegreesFromQuat(const glm::quat& rotation)
        {
            return radToDeg(eulerRadiansFromQuat(rotation));
        }

        inline glm::quat quatFromEulerDegrees(const glm::vec3& euler_deg)
        {
            return quatFromEulerRadians(degToRad(euler_deg));
        }

        inline glm::vec3 forwardFromRotation(const glm::quat& rotation)
        {
            return normalize(normalizeQuat(rotation) * glm::vec3(0.0f, 0.0f, -1.0f));
        }

        inline glm::vec3 rightFromRotation(const glm::quat& rotation)
        {
            return normalize(normalizeQuat(rotation) * glm::vec3(1.0f, 0.0f, 0.0f));
        }

        inline glm::vec3 upFromRotation(const glm::quat& rotation)
        {
            return normalize(normalizeQuat(rotation) * glm::vec3(0.0f, 1.0f, 0.0f));
        }
    } // namespace MathUtil
} // namespace Hybrid
