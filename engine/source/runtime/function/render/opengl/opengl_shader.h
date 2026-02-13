#pragma once
#include "../shader.h"
#include <string>

namespace Hybrid {

    // OpenGLShader: GLSL program wrapper with uniform helpers.
    class OpenGLShader final : public Shader {
    public:
        OpenGLShader(const std::string& vsSource, const std::string& fsSource);
        ~OpenGLShader() override;

        void bind() const override;
        void unbind() const override;

        void setMat4(const std::string& name, const glm::mat4& m) override;

    private:
        uint32_t m_RendererID = 0;
    };

} // namespace Hybrid
