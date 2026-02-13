#pragma once
#include <glm/glm.hpp>

namespace Hybrid {

    // EditorCamera: free-fly camera used by the editor viewport.
    class EditorCamera {
    public:

        EditorCamera()
            : EditorCamera(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f) {
        }

        EditorCamera(float fovDeg, float aspect, float nearClip, float farClip);

        void setViewportSize(float width, float height);
        void update(float dt, bool enableControl, bool rmbDown,
            float mouseDeltaX, float mouseDeltaY,
            bool keyW, bool keyA, bool keyS, bool keyD, bool keyQ, bool keyE,
            float scrollDelta);

        const glm::mat4& getViewMatrix() const { return m_View; }
        const glm::mat4& getProjMatrix() const { return m_Proj; }
        glm::mat4 getViewProj() const { return m_Proj * m_View; }

        glm::vec3 getPosition() const { return m_Position; }

    private:
        void recalcView();
        void recalcProj();

    private:
        float m_FovDeg = 45.0f;
        float m_Aspect = 16.0f / 9.0f;
        float m_Near = 0.1f;
        float m_Far = 1000.0f;

        glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
        float m_YawDeg = -90.0f;   // 朝 -Z
        float m_PitchDeg = 0.0f;

        float m_MoveSpeed = 3.5f;      // units/s
        float m_MouseSensitivity = 0.12f; // deg per pixel
        float m_ScrollSpeed = 2.0f;

        glm::mat4 m_View{ 1.0f };
        glm::mat4 m_Proj{ 1.0f };
    };

} // namespace Hybrid
