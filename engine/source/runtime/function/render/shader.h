#pragma once
#include <string>
#include <cstdint>

namespace Hybrid {

    class Shader {
    public:
        Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
        ~Shader();

        void Bind() const;
        void Unbind() const;

    private:
        uint32_t m_RendererID = 0;
        uint32_t Compile(uint32_t type, const std::string& src);
    };

} // namespace Hybrid
