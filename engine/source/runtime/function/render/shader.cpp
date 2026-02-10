#include "shader.h"
#include <glad/gl.h>
#include <iostream>

namespace Hybrid {

    static void CheckLink(uint32_t program) {
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            GLint len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
            std::string msg(len, '\0');
            glGetProgramInfoLog(program, len, &len, msg.data());
            std::cerr << "[Shader] Link failed:\n" << msg << std::endl;
        }
    }

    static void CheckCompile(uint32_t shader, const char* stage) {
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE) {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string msg(len, '\0');
            glGetShaderInfoLog(shader, len, &len, msg.data());
            std::cerr << "[Shader] Compile failed (" << stage << "):\n" << msg << std::endl;
        }
    }

    uint32_t Shader::compile(uint32_t type, const std::string& src) {
        uint32_t id = glCreateShader(type);
        const char* csrc = src.c_str();
        glShaderSource(id, 1, &csrc, nullptr);
        glCompileShader(id);
        CheckCompile(id, type == GL_VERTEX_SHADER ? "VS" : "FS");
        return id;
    }

    Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
        uint32_t vs = compile(GL_VERTEX_SHADER, vertexSrc);
        uint32_t fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vs);
        glAttachShader(m_RendererID, fs);
        glLinkProgram(m_RendererID);
        CheckLink(m_RendererID);

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    Shader::~Shader() {
        glDeleteProgram(m_RendererID);
    }

    void Shader::bind() const {
        glUseProgram(m_RendererID);
    }

    void Shader::unbind() const {
        glUseProgram(0);
    }

    void Shader::setMat4(const std::string& name, const glm::mat4& m) {
        GLint loc = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
    }


} // namespace Hybrid
