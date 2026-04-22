#version 460 core
// 全屏三角形（无 VBO，直接在 VS 中生成坐标）
void main() {
    // 用 gl_VertexID 生成覆盖全屏的大三角形
    // VertexID 0: (-1,-1)  1: (3,-1)  2: (-1, 3)
    vec2 pos = vec2((gl_VertexID & 1) * 4 - 1,
                    (gl_VertexID >> 1) * 4 - 1);
    gl_Position = vec4(pos, 0.0, 1.0);
}
