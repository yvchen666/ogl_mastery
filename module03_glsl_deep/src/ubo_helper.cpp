#include "ubo_helper.h"
#include <iostream>

// UboHelper::create
// 创建 OpenGL 缓冲对象，分配 size_bytes 字节的 GPU 端存储
// GL_DYNAMIC_DRAW 提示驱动：数据会频繁更新（每帧），优化为可 CPU 快速写入的内存区域
void UboHelper::create(size_t size_bytes) {
    // 分配缓冲对象名称
    glGenBuffers(1, &id);

    // 绑定到 GL_UNIFORM_BUFFER 目标（UBO 专用绑定点）
    glBindBuffer(GL_UNIFORM_BUFFER, id);

    // 分配 GPU 内存，初始数据为 nullptr（内容未定义）
    // GL_DYNAMIC_DRAW：应用每帧写入一次，GPU 读取多次
    // 对比：GL_STATIC_DRAW（写一次读多次）、GL_STREAM_DRAW（每帧写每帧读，如粒子）
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size_bytes), nullptr, GL_DYNAMIC_DRAW);

    // 解绑（可选，良好习惯）
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// UboHelper::bind_to_point
// 将 UBO 绑定到指定的 uniform 缓冲绑定点（binding point）
//
// OpenGL 中 UBO 有两级绑定：
//   Level 1: UBO → GL_UNIFORM_BUFFER 目标（只有一个，类似 VBO 的 GL_ARRAY_BUFFER）
//   Level 2: UBO → 带编号的绑定点（binding point 0..GL_MAX_UNIFORM_BUFFER_BINDINGS-1）
//
// 着色器通过 binding_point 找到 UBO，而不是直接通过 UBO id：
//   layout(std140, binding = 0) uniform FrameData { ... }
//   → 使用绑定点 0 的 UBO
//
// glBindBufferBase 将 UBO 同时绑定到"通用"目标和带编号的绑定点
void UboHelper::bind_to_point(GLuint binding_point) const {
    // glBindBufferBase(target, binding_point_index, buffer_id)
    // 等价于：
    //   glBindBuffer(GL_UNIFORM_BUFFER, id);
    //   glUniformBlockBinding(program, block_index, binding_point);  // 但更通用
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, id);
}

// UboHelper::update
// 用 glBufferSubData 更新 UBO 的部分数据
//
// glBufferSubData vs glBufferData：
//   glBufferData   : 重新分配 + 上传（可能导致 GPU 同步，因为驱动需要确认旧数据不再使用）
//   glBufferSubData: 仅更新已分配的内存的一部分（更高效，驱动可以做 orphaning 优化）
//
// 注意：如果 GPU 正在读取此 UBO（例如上一帧的 draw call 还未完成），
//   glBufferSubData 可能导致隐式同步！
//   对于每帧更新的数据，更好的方案是使用 persistent mapping + fence，
//   或者多缓冲（对应不同帧使用不同 UBO）。
void UboHelper::update(const void* data, size_t offset, size_t size_bytes) {
    glBindBuffer(GL_UNIFORM_BUFFER, id);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        static_cast<GLintptr>(offset),   // 字节偏移
        static_cast<GLsizeiptr>(size_bytes), // 字节数
        data                               // CPU 端数据指针
    );
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// UboHelper::destroy
// 删除 GPU 缓冲对象
void UboHelper::destroy() {
    if (id != 0) {
        glDeleteBuffers(1, &id);
        id = 0;
    }
}
