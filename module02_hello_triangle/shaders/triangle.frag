#version 460 core

// ── 输入：来自顶点着色器的插值变量 ────────────────────────────────────────
// 光栅化阶段已对 vColor 做透视正确插值（smooth 是默认限定符）
in vec3 vColor;

// ── 输出：最终写入帧缓冲的片元颜色 ────────────────────────────────────────
// location = 0 对应 GL_COLOR_ATTACHMENT0（默认帧缓冲的颜色缓冲）
layout(location = 0) out vec4 FragColor;

void main() {
    // 直接输出插值后的颜色，alpha = 1.0（完全不透明）
    // 三角形角顶点颜色（红/绿/蓝）在内部平滑插值，产生彩虹渐变效果
    FragColor = vec4(vColor, 1.0);
}
