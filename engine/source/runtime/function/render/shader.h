#pragma once
#include <string>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>

namespace Hybrid {

    class Shader {
    public:
        Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
        ~Shader();

        void Bind() const;
        void Unbind() const;
        void SetMat4(const std::string& name, const glm::mat4& m); // 4x4 matrix

    private:
        uint32_t m_RendererID = 0;
        uint32_t Compile(uint32_t type, const std::string& src);
    };

} // namespace Hybrid
