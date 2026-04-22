#include "shader.h"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// compile  —  从文件读取 GLSL 源码并编译
// ─────────────────────────────────────────────────────────────────────────────
GLuint Shader::compile(GLenum type, const char* path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error(std::string("Cannot open shader: ") + path);

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();
    const char* c   = src.c_str();

    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(
            std::string("Shader compile error [") + path + "]: " + log);
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// 构造函数：编译两个阶段并链接
// ─────────────────────────────────────────────────────────────────────────────
Shader::Shader(const char* vert_path, const char* frag_path)
{
    GLuint vs = compile(GL_VERTEX_SHADER,   vert_path);
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag_path);

    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(id, 1024, nullptr, log);
        glDeleteProgram(id);
        id = 0;
        throw std::runtime_error(std::string("Program link error: ") + log);
    }
}

Shader::~Shader()
{
    if (id) glDeleteProgram(id);
}

void Shader::use() const
{
    glUseProgram(id);
}

// ─── uniform setter 实现 ─────────────────────────────────────────────────────

void Shader::set_bool(const char* name, bool v) const
{
    glUniform1i(glGetUniformLocation(id, name), (int)v);
}

void Shader::set_int(const char* name, int v) const
{
    glUniform1i(glGetUniformLocation(id, name), v);
}

void Shader::set_float(const char* name, float v) const
{
    glUniform1f(glGetUniformLocation(id, name), v);
}

void Shader::set_vec3(const char* name, const glm::vec3& v) const
{
    glUniform3fv(glGetUniformLocation(id, name), 1, glm::value_ptr(v));
}

void Shader::set_mat3(const char* name, const glm::mat3& m) const
{
    glUniformMatrix3fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::set_mat4(const char* name, const glm::mat4& m) const
{
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(m));
}
