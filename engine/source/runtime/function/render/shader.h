#pragma once
#include <string>
#include <memory>
#include <glm/ext/matrix_float4x4.hpp>

namespace Hybrid {

    // Shader: encapsulates GPU program plus uniform setters.
    class Shader {
    public:
        virtual ~Shader() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual void setMat4(const std::string& name, const glm::mat4& m) = 0;

        static std::shared_ptr<Shader> Create(const std::string& vsSource, const std::string& fsSource);
    };

} // namespace Hybrid
