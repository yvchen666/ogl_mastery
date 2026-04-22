#version 460 core

// ── 顶点着色器：接收 NDC/世界坐标，通过 MVP 变换后输出裁剪空间 ──────────
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;   // 若已在 NDC，传 identity

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
}
