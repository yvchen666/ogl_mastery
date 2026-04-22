#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Particle {
    glm::vec4 position;   // xyz + w=age
    glm::vec4 velocity;   // xyz + w=life_max
    glm::vec4 color;      // rgba（生命周期内线性变化）
};

class ParticleSystem {
public:
    int max_particles{100000};
    GLuint ssbo{0};     // 粒子数据 SSBO
    GLuint vao{0};      // 空 VAO（instanced billboard）

    void init(int n = 100000);

    // Compute Shader 更新（delta_time 秒）
    void update(float dt);

    // 渲染（Billboard，面向 camera_pos）
    void render(const glm::mat4& view, const glm::mat4& proj,
                glm::vec3 camera_pos);

    void destroy();

private:
    GLuint update_prog_{0};  // Compute Shader 程序
    GLuint render_prog_{0};  // 顶点+片段着色器程序
    float time_{0.0f};
};
