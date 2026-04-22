#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

// ─── 变换组件 ─────────────────────────────────────────────────────────────────
struct TransformComponent {
    glm::vec3 pos   = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::quat rot   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // identity

    glm::mat4 matrix() const {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, pos);
        m = m * glm::mat4_cast(rot);
        m = glm::scale(m, scale);
        return m;
    }
};

// ─── 网格组件 ─────────────────────────────────────────────────────────────────
struct MeshComponent {
    GLuint vao         = 0;
    int    index_count = 0;
    GLuint albedo_tex  = 0;  // 0 = 使用默认白色
};

// ─── 光源组件 ─────────────────────────────────────────────────────────────────
struct LightComponent {
    glm::vec3 color        = glm::vec3(1.0f);
    float     intensity    = 1.0f;
    bool      is_directional = false;  // true=方向光，false=点光源
};

// ─── 名称组件 ─────────────────────────────────────────────────────────────────
struct NameComponent {
    std::string name = "Entity";
};
