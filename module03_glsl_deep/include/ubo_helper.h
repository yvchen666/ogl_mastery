#pragma once
#include <glad/glad.h>
#include <cstddef>

// UboHelper：封装 OpenGL Uniform Buffer Object 的创建、绑定和更新操作
//
// 使用示例：
//   UboHelper ubo;
//   ubo.create(sizeof(FrameData));         // 分配 GPU 端缓冲
//   ubo.bind_to_point(0);                  // 绑定到 binding=0（对应着色器中 binding=0）
//   ubo.update(&frame_data, 0, sizeof(FrameData));  // 每帧更新数据
//   ubo.destroy();                         // 程序结束时释放
struct UboHelper {
    GLuint id{0};  // OpenGL 缓冲对象名称，0 表示未创建

    // 创建 UBO 并分配 size_bytes 字节的 GPU 内存（初始内容未定义）
    void create(size_t size_bytes);

    // 将此 UBO 绑定到指定的 uniform 缓冲绑定点
    // binding_point 必须与着色器中 layout(std140, binding = X) 的 X 一致
    void bind_to_point(GLuint binding_point) const;

    // 用 glBufferSubData 更新 UBO 的部分内容
    // offset      : 相对于 UBO 起点的字节偏移
    // size_bytes  : 要更新的字节数
    // data        : CPU 端数据指针
    void update(const void* data, size_t offset, size_t size_bytes);

    // 删除 GPU 缓冲对象，释放显存
    void destroy();
};
