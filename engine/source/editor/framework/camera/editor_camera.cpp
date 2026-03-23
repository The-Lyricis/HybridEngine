#include "editor_camera.h"

#include "runtime/core/base/math_util.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Hybrid {

    EditorCamera::EditorCamera(float fovDeg, float aspect, float nearClip, float farClip)
        : m_FovDeg(fovDeg), m_Aspect(aspect), m_Near(nearClip), m_Far(farClip)
    {
        recalcProj();
        syncFocalPointFromPosition();
        recalcView();
    }

    void EditorCamera::setViewportSize(float width, float height)
    {
        if (width < 1.0f || height < 1.0f)
            return;

        m_ViewportWidth = width;
        m_ViewportHeight = height;
        m_Aspect = width / height;
        recalcProj();
    }

    void EditorCamera::update(float dt, bool enableControl,
        float mouseDeltaX, float mouseDeltaY, float scrollDelta,
        bool lmbDown, bool mmbDown, bool rmbDown,
        bool keyW, bool keyA, bool keyS, bool keyD, bool keyQ, bool keyE,
        bool keyShift, bool keyCtrl, bool keyAlt)
    {
        if (!enableControl)
        {
            recalcView();
            return;
        }

        const bool flyActive = rmbDown && !keyAlt;
        const bool orbitRotate = (keyAlt && lmbDown) || flyActive;
        const bool orbitPan = mmbDown;
        const bool orbitDolly = keyAlt && rmbDown;
        const bool orbitByScroll = (std::abs(scrollDelta) > 1e-4f);
        const bool orbitActive = orbitRotate || orbitPan || orbitDolly || orbitByScroll;

        if (orbitRotate)
        {
            m_YawDeg += mouseDeltaX * m_OrbitSensitivity;
            m_PitchDeg -= mouseDeltaY * m_OrbitSensitivity;
            m_PitchDeg = std::clamp(m_PitchDeg, -89.0f, 89.0f);
        }

        if (orbitPan)
        {
            const float d = std::max(m_Distance, 0.001f);
            const float vFovRad = MathUtil::degToRad(m_FovDeg);
            const float worldPerPixel =
                (2.0f * d * std::tan(vFovRad * 0.5f)) / std::max(m_ViewportHeight, 1.0f);

            m_FocalPoint += (-getRight() * mouseDeltaX + getUp() * mouseDeltaY) * worldPerPixel;
        }

        if (orbitDolly)
        {
            const float dollyScale = std::max(0.01f, m_Distance * m_DollySensitivity);
            m_Distance = std::clamp(m_Distance + mouseDeltaY * dollyScale, m_MinDistance, m_MaxDistance);
        }

        if (orbitByScroll)
        {
            const float zoomFactor = std::pow(0.85f, scrollDelta);
            m_Distance = std::clamp(m_Distance * zoomFactor, m_MinDistance, m_MaxDistance);
        }

        if (orbitActive)
        {
            updatePositionFromFocalPoint();
            if (!flyActive)
            {
                recalcView();
                return;
            }
        }

        if (flyActive)
        {
            float speed = m_MoveSpeed;
            if (keyShift) speed *= m_FastMultiplier;
            if (keyCtrl)  speed *= m_SlowMultiplier;

            const float v = speed * dt;
            const glm::vec3 forward = getForward();
            const glm::vec3 right = getRight();
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

            if (keyW) m_Position += forward * v;
            if (keyS) m_Position -= forward * v;
            if (keyD) m_Position += right * v;
            if (keyA) m_Position -= right * v;
            if (keyE) m_Position += worldUp * v;
            if (keyQ) m_Position -= worldUp * v;
        }

        syncFocalPointFromPosition();
        recalcView();
    }

    void EditorCamera::focusOnSelection()
    {
        // TODO: hook this to editor selection once selection data is available.
    }

    EditorCameraState EditorCamera::dumpState() const
    {
        EditorCameraState state{};
        state.position = m_Position;
        state.focal_point = m_FocalPoint;
        state.distance = m_Distance;
        state.yaw_deg = m_YawDeg;
        state.pitch_deg = m_PitchDeg;
        return state;
    }

    void EditorCamera::loadState(const EditorCameraState& state)
    {
        m_Position = state.position;
        m_FocalPoint = state.focal_point;
        m_Distance = std::clamp(state.distance, m_MinDistance, m_MaxDistance);
        m_YawDeg = state.yaw_deg;
        m_PitchDeg = std::clamp(state.pitch_deg, -89.0f, 89.0f);
        recalcView();
    }

    void EditorCamera::recalcView()
    {
        m_View = glm::lookAt(
            m_Position,
            m_Position + getForward(),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        m_ViewProj = m_Proj * m_View;
    }

    void EditorCamera::recalcProj()
    {
        m_Proj = glm::perspective(MathUtil::degToRad(m_FovDeg), m_Aspect, m_Near, m_Far);
        m_ViewProj = m_Proj * m_View;
    }

    glm::vec3 EditorCamera::getForward() const
    {
        const float yawR = MathUtil::degToRad(m_YawDeg);
        const float pitR = MathUtil::degToRad(m_PitchDeg);

        glm::vec3 forward;
        forward.x = std::cos(yawR) * std::cos(pitR);
        forward.y = std::sin(pitR);
        forward.z = std::sin(yawR) * std::cos(pitR);
        return MathUtil::normalize(forward);
    }

    glm::vec3 EditorCamera::getRight() const
    {
        return MathUtil::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 EditorCamera::getUp() const
    {
        return MathUtil::normalize(glm::cross(getRight(), getForward()), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void EditorCamera::syncFocalPointFromPosition()
    {
        m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);
        m_FocalPoint = m_Position + getForward() * m_Distance;
    }

    void EditorCamera::updatePositionFromFocalPoint()
    {
        m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);
        m_Position = m_FocalPoint - getForward() * m_Distance;
    }

} // namespace Hybrid
