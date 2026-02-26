#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hybrid
{
    namespace MathUtil
    {
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
            return glm::degrees(eulerRadiansFromQuat(rotation));
        }

        inline glm::quat quatFromEulerDegrees(const glm::vec3& euler_deg)
        {
            return quatFromEulerRadians(glm::radians(euler_deg));
        }
    } // namespace MathUtil
} // namespace Hybrid
