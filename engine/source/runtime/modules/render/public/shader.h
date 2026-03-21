#pragma once
#include <string>
#include <memory>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>

namespace Hybrid {

    // Shader: encapsulates GPU program plus uniform setters.
    class Shader {
    public:
        virtual ~Shader() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual void setMat4(const std::string& name, const glm::mat4& m) = 0;
        virtual void setVec4(const std::string& name, const glm::vec4& v) = 0;
        virtual void setFloat(const std::string& name, float v) = 0;
        virtual void setVec3(const std::string& name, const glm::vec3& v) = 0;
        virtual void setInt(const std::string& name, int v) = 0;
        virtual void setUInt(const std::string& name, uint32_t v) = 0;
        virtual void setUniformBlockBinding(const std::string& block_name, uint32_t binding) = 0;

        static std::shared_ptr<Shader> Create(const std::string& vsSource, const std::string& fsSource);
    };

} // namespace Hybrid
