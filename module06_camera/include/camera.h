#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// FpsCamera
//   第一人称相机：WASD 平移，鼠标旋转（Yaw/Pitch）
// ─────────────────────────────────────────────────────────────────────────────
class FpsCamera {
public:
    glm::vec3 position          {0.0f, 0.0f,  3.0f};
    float     yaw               {-90.0f};   // 水平角：-90° 时面向 -Z
    float     pitch             {  0.0f};   // 垂直角：[-89, 89]

    float move_speed            {5.0f};     // 单位/秒
    float mouse_sensitivity     {0.1f};     // 度/像素
    float fov                   {45.0f};    // 垂直视角（度）

    // ── 处理键盘输入 ─────────────────────────────────────────────────────
    // key : GLFW_KEY_W / S / A / D / SPACE / LEFT_CONTROL 等
    // dt  : 帧时间（秒）
    void process_keyboard(int key, float dt);

    // ── 处理鼠标位移（像素）─────────────────────────────────────────────
    void process_mouse(float dx, float dy);

    // ── 处理滚轮（调整 fov）─────────────────────────────────────────────
    void process_scroll(float dy);

    // ── 矩阵 ─────────────────────────────────────────────────────────────
    glm::mat4 view_matrix() const;
    glm::mat4 proj_matrix(float aspect) const;

    // ── 方向向量 ──────────────────────────────────────────────────────────
    glm::vec3 front() const { return front_; }
    glm::vec3 right() const { return right_; }
    glm::vec3 up()    const { return up_;    }

private:
    // 根据 yaw/pitch 重建 front/right/up
    void update_vectors();

    glm::vec3 front_{0, 0, -1};
    glm::vec3 right_{1, 0,  0};
    glm::vec3 up_   {0, 1,  0};
};

// ─────────────────────────────────────────────────────────────────────────────
// OrbitCamera
//   轨道相机：围绕目标点，球坐标系 (distance, yaw, pitch)
//   鼠标拖拽旋转，滚轮缩放
// ─────────────────────────────────────────────────────────────────────────────
class OrbitCamera {
public:
    glm::vec3 target   {0.0f, 0.0f, 0.0f};
    float     distance {5.0f};   // 到目标的距离
    float     yaw      {0.0f};   // 水平角（度）
    float     pitch    {30.0f};  // 垂直角（度），正值向上

    float mouse_sensitivity {0.3f};
    float scroll_speed      {0.5f};
    float fov               {45.0f};

    // 鼠标拖拽旋转（dx/dy 为像素）
    void process_mouse_drag(float dx, float dy);

    // 滚轮缩放
    void process_scroll(float dy);

    // 矩阵
    glm::mat4 view_matrix() const;
    glm::mat4 proj_matrix(float aspect) const;

    // 相机当前世界坐标
    glm::vec3 position() const;
};
