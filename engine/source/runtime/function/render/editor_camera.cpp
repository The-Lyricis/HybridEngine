#include "editor_camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Hybrid {

    static float radians(float deg) { return deg * 0.01745329251994329577f; }

    static glm::vec3 calculateForward(float yawDeg, float pitchDeg) {
        const float yawR = radians(yawDeg);
        const float pitR = radians(pitchDeg);

        glm::vec3 forward;
        forward.x = std::cos(yawR) * std::cos(pitR);
        forward.y = std::sin(pitR);
        forward.z = std::sin(yawR) * std::cos(pitR);
        return glm::normalize(forward);
    }

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
        if (enableControl && rmbDown) {
            m_YawDeg += mouseDeltaX * m_MouseSensitivity;
            m_PitchDeg -= mouseDeltaY * m_MouseSensitivity;
            m_PitchDeg = std::clamp(m_PitchDeg, -89.0f, 89.0f);
        }

        const glm::vec3 forward = calculateForward(m_YawDeg, m_PitchDeg);

        if (enableControl) {
            m_Position += forward * (scrollDelta * m_ScrollSpeed);
        }

        if (!(enableControl && rmbDown)) {
            recalcView();
            return;
        }

        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

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
        const glm::vec3 forward = calculateForward(m_YawDeg, m_PitchDeg);
        m_View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0, 1, 0));
    }

    void EditorCamera::recalcProj() {
        m_Proj = glm::perspective(radians(m_FovDeg), m_Aspect, m_Near, m_Far);
    }

} // namespace Hybrid
