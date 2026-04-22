#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "ecs.h"

// ─────────────────────────────────────────────────────────────────────────────
// 基于 FBO 的对象拾取
// 原理：渲染一遍场景，但将每个物体的 EntityId 编码到颜色缓冲中
// 然后读取鼠标点击处的像素颜色，解码出 EntityId
// ─────────────────────────────────────────────────────────────────────────────
class Picking {
public:
    void init(int w, int h);
    void resize(int w, int h);

    // 渲染 EntityId 编码帧（不显示到屏幕）
    void render_ids(World& world, const glm::mat4& vp);

    // 读取像素处的 EntityId（0 = 无物体）
    EntityId read_entity(int x, int y);

    void destroy();

private:
    GLuint fbo_{0};
    GLuint color_tex_{0};  // R32UI 纹理，存 EntityId
    GLuint depth_rb_{0};
    GLuint prog_{0};
    int    width_{0}, height_{0};

    bool build_shader();
};
