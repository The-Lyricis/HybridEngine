#pragma once
#include <glm/glm.hpp>

namespace Hybrid {

    // Editor camera used by the viewport. Supports fly and orbit style controls.
    class EditorCamera {
    public:

        EditorCamera()
            : EditorCamera(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f) {
        }

        EditorCamera(float fovDeg, float aspect, float nearClip, float farClip);

        void setViewportSize(float width, float height);
        void update(float dt, bool enableControl,
            float mouseDeltaX, float mouseDeltaY, float scrollDelta,
            bool lmbDown, bool mmbDown, bool rmbDown,
            bool keyW, bool keyA, bool keyS, bool keyD, bool keyQ, bool keyE,
            bool keyShift, bool keyCtrl, bool keyAlt);

        // Reserved for future selection-based focus flow. Not wired yet.
        void focusOnSelection();

        const glm::mat4& getViewMatrix() const { return m_View; }
        const glm::mat4& getProjMatrix() const { return m_Proj; }
        const glm::mat4& getView() const { return m_View; }
        const glm::mat4& getProjection() const { return m_Proj; }
        const glm::mat4& getViewProj() const { return m_ViewProj; }

        glm::vec3 getPosition() const { return m_Position; }

    private:
        void recalcView();
        void recalcProj();

        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        glm::vec3 getUp() const;
        void syncFocalPointFromPosition();
        void updatePositionFromFocalPoint();

    private:
        float m_FovDeg = 45.0f;
        float m_Aspect = 16.0f / 9.0f;
        float m_Near = 0.1f;
        float m_Far = 1000.0f;
        float m_ViewportWidth = 1280.0f;
        float m_ViewportHeight = 720.0f;

        glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
        glm::vec3 m_FocalPoint{ 0.0f, 0.0f, 0.0f };
        float m_Distance = 5.0f;

        float m_YawDeg = -90.0f;
        float m_PitchDeg = 0.0f;

        float m_MoveSpeed = 5.0f;
        float m_MinMoveSpeed = 0.2f;
        float m_MaxMoveSpeed = 200.0f;

        float m_MouseSensitivity = 0.12f;
        float m_OrbitSensitivity = 0.18f;
        float m_DollySensitivity = 0.05f;

        float m_MinDistance = 0.4f;
        float m_MaxDistance = 5000.0f;

        float m_FastMultiplier = 4.0f;
        float m_SlowMultiplier = 0.25f;

        glm::mat4 m_View{ 1.0f };
        glm::mat4 m_Proj{ 1.0f };
        glm::mat4 m_ViewProj{ 1.0f };
    };

} // namespace Hybrid
