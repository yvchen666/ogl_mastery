#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// Minimal mesh descriptor (only geometry, no materials)
struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};
    GLenum  index_type{GL_UNSIGNED_INT};
};

// 实例化渲染封装
// 用 SSBO 存储每个实例的 mat4 变换矩阵
// 着色器通过 gl_InstanceID 从 SSBO 读取对应矩阵
class InstanceRenderer {
public:
    void init(const Mesh& mesh, int max_instances);

    // 上传实例变换矩阵到 SSBO（binding = 0）
    void upload_transforms(const std::vector<glm::mat4>& transforms);

    // glDrawElementsInstanced(GL_TRIANGLES, ..., count)
    void draw(int count) const;

    // 释放 SSBO
    void destroy();

    // 获取 SSBO 句柄（用于 indirect draw / compute 共享）
    GLuint ssbo() const { return ssbo_; }

private:
    GLuint ssbo_{0};
    int    max_instances_{0};
    const Mesh* mesh_{nullptr};
};
