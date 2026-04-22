#include "camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// FpsCamera 实现
// ─────────────────────────────────────────────────────────────────────────────

void FpsCamera::update_vectors()
{
    // 从欧拉角（Yaw, Pitch）重建世界空间方向向量
    // Yaw=0   → front = +X
    // Yaw=-90 → front = -Z（OpenGL 惯例初始朝向）
    float yawR   = glm::radians(yaw);
    float pitchR = glm::radians(pitch);

    glm::vec3 f;
    f.x = std::cos(pitchR) * std::cos(yawR);
    f.y = std::sin(pitchR);
    f.z = std::cos(pitchR) * std::sin(yawR);
    front_ = glm::normalize(f);

    // Right = front × worldUp（世界 up=(0,1,0)）
    right_ = glm::normalize(glm::cross(front_, glm::vec3{0,1,0}));
    // Up = Right × Front（重新正交化）
    up_    = glm::normalize(glm::cross(right_, front_));
}

void FpsCamera::process_keyboard(int key, float dt)
{
    float vel = move_speed * dt;

    if (key == GLFW_KEY_W)             position += vel * front_;
    if (key == GLFW_KEY_S)             position -= vel * front_;
    if (key == GLFW_KEY_A)             position -= vel * right_;
    if (key == GLFW_KEY_D)             position += vel * right_;
    if (key == GLFW_KEY_SPACE)         position += vel * glm::vec3{0,1,0};
    if (key == GLFW_KEY_LEFT_CONTROL)  position -= vel * glm::vec3{0,1,0};
}

void FpsCamera::process_mouse(float dx, float dy)
{
    yaw   += dx * mouse_sensitivity;
    pitch -= dy * mouse_sensitivity;   // dy 向上为负（屏幕坐标）

    // 限制俯仰角，避免万向锁（pitch = ±90° 时会退化）
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    update_vectors();
}

void FpsCamera::process_scroll(float dy)
{
    fov -= dy;
    fov  = std::clamp(fov, 5.0f, 120.0f);
}

glm::mat4 FpsCamera::view_matrix() const
{
    return glm::lookAt(position, position + front_, up_);
}

glm::mat4 FpsCamera::proj_matrix(float aspect) const
{
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// OrbitCamera 实现
// ─────────────────────────────────────────────────────────────────────────────

// 球坐标 → 笛卡尔偏移（以 target 为原点）
// x = r * sin(pitch) * sin(yaw) → 注意：这里 pitch 从水平面向上为正
// y = r * sin(pitch)
// z = r * cos(pitch) * cos(yaw)
glm::vec3 OrbitCamera::position() const
{
    float pitchR = glm::radians(pitch);
    float yawR   = glm::radians(yaw);

    glm::vec3 offset;
    offset.x = distance * std::cos(pitchR) * std::sin(yawR);
    offset.y = distance * std::sin(pitchR);
    offset.z = distance * std::cos(pitchR) * std::cos(yawR);

    return target + offset;
}

void OrbitCamera::process_mouse_drag(float dx, float dy)
{
    yaw   += dx * mouse_sensitivity;
    pitch += dy * mouse_sensitivity;   // 向上拖动抬高相机
    pitch  = std::clamp(pitch, -89.0f, 89.0f);
}

void OrbitCamera::process_scroll(float dy)
{
    distance -= dy * scroll_speed;
    distance  = std::max(distance, 0.5f);   // 最近距离 0.5
}

glm::mat4 OrbitCamera::view_matrix() const
{
    glm::vec3 pos = this->position();
    // world up：pitch 接近 ±90° 时用 (0,1,0) 也能工作，
    // 因为 lookAt 会自动重正交化
    return glm::lookAt(pos, target, glm::vec3{0,1,0});
}

glm::mat4 OrbitCamera::proj_matrix(float aspect) const
{
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 500.0f);
}
