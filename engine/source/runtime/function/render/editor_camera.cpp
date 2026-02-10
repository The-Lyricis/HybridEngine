#include "editor_camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Hybrid {

    static float radians(float deg) { return deg * 0.01745329251994329577f; }


    EditorCamera::EditorCamera(float fovDeg, float aspect, float nearClip, float farClip)
        : m_FovDeg(fovDeg), m_Aspect(aspect), m_Near(nearClip), m_Far(farClip) {
        recalcProj();
        recalcView();
    }

    void EditorCamera::setViewportSize(float width, float height) {
        if (width < 1.0f || height < 1.0f) return;
        m_Aspect = width / height;
        recalcProj();
    }

    void EditorCamera::update(float dt, bool enableControl, bool rmbDown,
        float mouseDeltaX, float mouseDeltaY,
        bool keyW, bool keyA, bool keyS, bool keyD, bool keyQ, bool keyE,
        float scrollDelta) {
        // 允许滚轮缩放（即使不按 RMB，也可根据你偏好调整）
        if (enableControl) {
            m_Position += getViewMatrix()[2] * (-scrollDelta) * m_ScrollSpeed; // view[2] 是 -forward 的近似
        }

        if (!(enableControl && rmbDown)) {
            // 不控制时只更新投影（若 viewport 改变）
            recalcView();
            return;
        }

        // 1) 鼠标旋转
        m_YawDeg += mouseDeltaX * m_MouseSensitivity;
        m_PitchDeg -= mouseDeltaY * m_MouseSensitivity;
        m_PitchDeg = std::clamp(m_PitchDeg, -89.0f, 89.0f);

        // 2) 方向向量
        const float yawR = radians(m_YawDeg);
        const float pitR = radians(m_PitchDeg);

        glm::vec3 forward;
        forward.x = std::cos(yawR) * std::cos(pitR);
        forward.y = std::sin(pitR);
        forward.z = std::sin(yawR) * std::cos(pitR);
        forward = glm::normalize(forward);

        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // 3) 键盘平移（WASD + QE 上下）
        const float v = m_MoveSpeed * dt;
        if (keyW) m_Position += forward * v;
        if (keyS) m_Position -= forward * v;
        if (keyD) m_Position += right * v;
        if (keyA) m_Position -= right * v;
        if (keyE) m_Position += up * v;
        if (keyQ) m_Position -= up * v;

        recalcView();
    }

    void EditorCamera::recalcView() {
        const float yawR = radians(m_YawDeg);
        const float pitR = radians(m_PitchDeg);

        glm::vec3 forward;
        forward.x = std::cos(yawR) * std::cos(pitR);
        forward.y = std::sin(pitR);
        forward.z = std::sin(yawR) * std::cos(pitR);
        forward = glm::normalize(forward);

        m_View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0, 1, 0));
    }

    void EditorCamera::recalcProj() {
        m_Proj = glm::perspective(radians(m_FovDeg), m_Aspect, m_Near, m_Far);
    }

} // namespace Hybrid
