#version 460 core

// ── 片段着色器：纯色输出 ────────────────────────────────────────────────
uniform vec3 uColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(uColor, 1.0);
}
