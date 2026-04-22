#version 460 core

// ── 输入：来自 VAO 的顶点属性 ─────────────────────────────────────────────
layout(location = 0) in vec2 aPos;  // NDC 位置（x, y），范围 [-1, 1]
layout(location = 1) in vec2 aUV;   // UV 坐标，范围 [0, 1]

// ── 输出：传递给片元着色器的 varying 变量 ─────────────────────────────────
out vec2 vUV;          // UV 坐标（透视正确插值，但矩形+正交投影时等价于线性插值）
out vec2 vScreenPos;   // 屏幕位置（归一化到 [0,1]，用于在 FS 中做屏幕空间效果）

void main() {
    // 矩形已经是 NDC 坐标，直接输出
    gl_Position = vec4(aPos, 0.0, 1.0);

    // 传递 UV（已在 [0,1] 范围）
    vUV = aUV;

    // 将 NDC [-1,1] 映射到屏幕空间 [0,1]（用于 FS 中的屏幕空间效果）
    vScreenPos = aPos * 0.5 + 0.5;
}
