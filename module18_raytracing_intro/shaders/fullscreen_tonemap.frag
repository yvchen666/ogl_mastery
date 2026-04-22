#version 460 core

uniform sampler2D uAccumTex;

out vec4 FragColor;

// ─── ACES Filmic Tone Mapping ────────────────────────────────────────────────
// 来源：Krzysztof Narkowicz，ACES Filmic Tone Mapping Curve
vec3 aces_tonemap(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a*x + b)) / (x * (c*x + d) + e), 0.0, 1.0);
}

void main() {
    // 全屏：直接用 gl_FragCoord 计算 UV（本 shader 用于全屏三角形）
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(uAccumTex, 0));

    vec3 hdr = texture(uAccumTex, uv).rgb;

    // Gamma 矫正（linear → sRGB）
    vec3 ldr = aces_tonemap(hdr);
    vec3 gamma_corrected = pow(ldr, vec3(1.0 / 2.2));

    FragColor = vec4(gamma_corrected, 1.0);
}
