#pragma once
#include <glad/glad.h>
#include <vector>

// DrawElementsIndirectCommand — 与 GPU 端 glDrawElementsIndirect 格式匹配
// 对应 OpenGL 规范中的 DrawElementsIndirectCommand 结构体
struct DrawElementsIndirectCommand {
    GLuint count;           // 索引数量
    GLuint instance_count;  // 实例数量
    GLuint first_index;     // 索引缓冲中的起始偏移（单位：索引个数）
    GLuint base_vertex;     // 基础顶点偏移（加到每个索引上）
    GLuint base_instance;   // 基础实例 ID（加到 gl_InstanceID）
};

// 间接绘制缓冲：Draw Command 存储在 GPU 端
// CPU 只需一次 glMultiDrawElementsIndirect 即可触发多个 draw call
class IndirectDrawBuffer {
public:
    GLuint buffer{0};

    void create(int max_commands);
    void upload(const std::vector<DrawElementsIndirectCommand>& cmds);
    void bind() const;   // 绑定到 GL_DRAW_INDIRECT_BUFFER
    void destroy();
};
