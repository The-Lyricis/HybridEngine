#pragma once
#include "runtime/modules/render/public/shader.h"
#include <string>

namespace Hybrid {

    // GLShader: GLSL program wrapper with uniform helpers.
    class GLShader final : public Shader {
    public:
        GLShader(const std::string& vsSource, const std::string& fsSource);
        ~GLShader() override;

        void bind() const override;
        void unbind() const override;

        void setMat4(const std::string& name, const glm::mat4& m) override;
        void setVec4(const std::string& name, const glm::vec4& v) override;
        void setFloat(const std::string& name, float v) override;
        void setVec3(const std::string& name, const glm::vec3& v) override;
        void setInt(const std::string& name, int v) override;
        void setUInt(const std::string& name, uint32_t v) override;
        void setUniformBlockBinding(const std::string& block_name, uint32_t binding) override;

    private:
        uint32_t m_RendererID = 0;
    };

} // namespace Hybrid


