#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// UBO 布局（std140，binding = 0）
struct CameraBlock {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cam_pos;
    float     _pad{0.0f};  // std140: vec3 后需要 4 字节对齐
};

// 简单光源（std140，binding = 1）
struct LightBlock {
    glm::vec4 positions[4];  // vec3 在 std140 中占 16 字节，用 vec4 避免对齐问题
    glm::vec4 colors[4];
    int       count{0};
    float     _pad[3]{};
};

// UBO 管理：相机参数和光照参数统一管理
// 所有共享此 binding 的着色器会自动获取最新数据
class UboManager {
public:
    GLuint camera_ubo{0};  // binding = 0
    GLuint lights_ubo{0};  // binding = 1

    void init();
    void update_camera(const glm::mat4& view, const glm::mat4& proj, glm::vec3 cam_pos);
    void update_lights(const glm::vec4* positions, const glm::vec4* colors, int count);
    void destroy();
};
