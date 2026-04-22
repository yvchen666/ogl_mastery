#version 460 core

in  vec4 vColor;
in  vec2 vUV;
out vec4 FragColor;

void main() {
    // ─── 圆形粒子：距中心大于 0.5 则 discard ──────────────
    vec2  d    = vUV - vec2(0.5);
    float dist = dot(d, d) * 4.0;  // 0..1，圆形
    if (dist > 1.0) discard;

    // 边缘柔化（软粒子简化版：基于 uv 距离渐变，无深度纹理）
    float alpha = (1.0 - dist) * vColor.a;

    // 加法混合模式下 alpha 控制亮度
    FragColor = vec4(vColor.rgb * alpha, 1.0);
}
