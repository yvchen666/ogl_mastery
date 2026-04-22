#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Shader
//   从文件加载并编译 GLSL 着色器，提供 uniform setter 接口
// ─────────────────────────────────────────────────────────────────────────────
class Shader {
public:
    GLuint id{0};

    // 从文件路径加载并链接顶点/片段着色器
    Shader(const char* vert_path, const char* frag_path);
    ~Shader();

    // 不可复制，可移动
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept : id(o.id) { o.id = 0; }

    void use() const;

    // ── uniform setter ────────────────────────────────────────────────────
    void set_bool (const char* name, bool          v) const;
    void set_int  (const char* name, int           v) const;
    void set_float(const char* name, float         v) const;
    void set_vec3 (const char* name, const glm::vec3& v) const;
    void set_mat3 (const char* name, const glm::mat3& m) const;
    void set_mat4 (const char* name, const glm::mat4& m) const;

private:
    // 从文件编译单个着色器阶段，返回 shader 对象 ID
    static GLuint compile(GLenum type, const char* path);
};
