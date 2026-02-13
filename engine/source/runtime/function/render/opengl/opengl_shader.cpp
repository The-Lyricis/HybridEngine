#include "opengl_shader.h"
#include <glad/gl.h>
#include <vector>

namespace Hybrid {

    static void checkCompile(uint32_t shader) {
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE) {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> msg(static_cast<size_t>(len) + 1);
            glGetShaderInfoLog(shader, len, &len, msg.data());
            // TODO: log message
        }
    }

    static void checkLink(uint32_t program) {
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            GLint len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> msg(static_cast<size_t>(len) + 1);
            glGetProgramInfoLog(program, len, &len, msg.data());
            // TODO: log message
        }
    }

    OpenGLShader::OpenGLShader(const std::string& vsSource, const std::string& fsSource) {
        auto compile = [](const std::string& src, GLenum type) {
            uint32_t id = glCreateShader(type);
            const char* csrc = src.c_str();
            glShaderSource(id, 1, &csrc, nullptr);
            glCompileShader(id);
            checkCompile(id);
            return id;
        };

        uint32_t vs = compile(vsSource, GL_VERTEX_SHADER);
        uint32_t fs = compile(fsSource, GL_FRAGMENT_SHADER);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vs);
        glAttachShader(m_RendererID, fs);
        glLinkProgram(m_RendererID);
        checkLink(m_RendererID);

        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    OpenGLShader::~OpenGLShader() {
        glDeleteProgram(m_RendererID);
    }

    void OpenGLShader::bind() const {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::unbind() const {
        glUseProgram(0);
    }

    void OpenGLShader::setMat4(const std::string& name, const glm::mat4& m) {
        GLint loc = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
    }

} // namespace Hybrid
