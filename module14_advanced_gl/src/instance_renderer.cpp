#include "instance_renderer.h"
#include <iostream>

void InstanceRenderer::init(const Mesh& mesh, int max_instances) {
    mesh_          = &mesh;
    max_instances_ = max_instances;

    // 创建 SSBO，预分配最大容量
    glGenBuffers(1, &ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 max_instances * sizeof(glm::mat4),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void InstanceRenderer::upload_transforms(const std::vector<glm::mat4>& transforms) {
    if ((int)transforms.size() > max_instances_) {
        std::cerr << "[InstanceRenderer] transforms.size() exceeds max_instances\n";
    }
    int count = std::min((int)transforms.size(), max_instances_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    count * sizeof(glm::mat4), transforms.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void InstanceRenderer::draw(int count) const {
    if (!mesh_) return;
    // 绑定 SSBO 到 binding = 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
    glBindVertexArray(mesh_->vao);
    glDrawElementsInstanced(GL_TRIANGLES, mesh_->index_count,
                            mesh_->index_type, nullptr, count);
    glBindVertexArray(0);
}

void InstanceRenderer::destroy() {
    if (ssbo_) { glDeleteBuffers(1, &ssbo_); ssbo_ = 0; }
}
