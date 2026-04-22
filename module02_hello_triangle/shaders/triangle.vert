#version 460 core

// ── 输入：来自 VAO/VBO 的顶点属性 ─────────────────────────────────────────
// layout(location = X) 必须与 glVertexAttribPointer 的第一个参数一致
layout(location = 0) in vec3 aPos;    // 顶点位置（模型空间，本例即 NDC）
layout(location = 1) in vec3 aColor;  // 顶点颜色（RGB）

// ── 输出：传递给片元着色器的 varying 变量 ──────────────────────────────────
// 光栅化阶段会对 vColor 在三角形面上进行透视正确插值
out vec3 vColor;

void main() {
    // gl_Position 是内置输出变量，必须赋值，单位是裁剪空间齐次坐标
    // 本例没有投影矩阵，顶点已经是 NDC 坐标，w=1.0 时裁剪空间 == NDC
    gl_Position = vec4(aPos, 1.0);

    // 将颜色透传到片元着色器
    vColor = aColor;
}
