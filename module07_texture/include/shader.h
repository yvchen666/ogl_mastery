#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
public:
    GLuint id{0};

    Shader() = default;

    Shader(const char* vert_path, const char* frag_path) {
        std::string vert_src = read_file(vert_path);
        std::string frag_src = read_file(frag_path);
        compile(vert_src.c_str(), frag_src.c_str());
    }

    void use() const { glUseProgram(id); }

    void set_int(const char* name, int v) const {
        glUniform1i(glGetUniformLocation(id, name), v);
    }
    void set_float(const char* name, float v) const {
        glUniform1f(glGetUniformLocation(id, name), v);
    }
    void set_bool(const char* name, bool v) const {
        glUniform1i(glGetUniformLocation(id, name), (int)v);
    }
    void set_vec2(const char* name, const glm::vec2& v) const {
        glUniform2fv(glGetUniformLocation(id, name), 1, glm::value_ptr(v));
    }
    void set_vec3(const char* name, const glm::vec3& v) const {
        glUniform3fv(glGetUniformLocation(id, name), 1, glm::value_ptr(v));
    }
    void set_vec4(const char* name, const glm::vec4& v) const {
        glUniform4fv(glGetUniformLocation(id, name), 1, glm::value_ptr(v));
    }
    void set_mat3(const char* name, const glm::mat3& m) const {
        glUniformMatrix3fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(m));
    }
    void set_mat4(const char* name, const glm::mat4& m) const {
        glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(m));
    }

    void destroy() {
        if (id) { glDeleteProgram(id); id = 0; }
    }

private:
    static std::string read_file(const char* path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[Shader] Cannot open: " << path << "\n";
            return "";
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void compile(const char* vert_src, const char* frag_src) {
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vert_src, nullptr);
        glCompileShader(vert);
        check_errors(vert, "VERTEX");

        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &frag_src, nullptr);
        glCompileShader(frag);
        check_errors(frag, "FRAGMENT");

        id = glCreateProgram();
        glAttachShader(id, vert);
        glAttachShader(id, frag);
        glLinkProgram(id);
        check_errors(id, "PROGRAM");

        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    static void check_errors(GLuint obj, const char* type) {
        GLint ok = 0;
        char log[1024];
        if (std::string(type) != "PROGRAM") {
            glGetShaderiv(obj, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                glGetShaderInfoLog(obj, 1024, nullptr, log);
                std::cerr << "[Shader] " << type << " compile error:\n" << log << "\n";
            }
        } else {
            glGetProgramiv(obj, GL_LINK_STATUS, &ok);
            if (!ok) {
                glGetProgramInfoLog(obj, 1024, nullptr, log);
                std::cerr << "[Shader] LINK error:\n" << log << "\n";
            }
        }
    }
};
