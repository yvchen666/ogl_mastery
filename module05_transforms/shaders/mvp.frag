#version 460 core
// ── 简单漫反射片段着色器 ──────────────────────────────────────────────────────

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTexCoords;

// ── uniform ──────────────────────────────────────────────────────────────────
uniform vec3 uLightPos;    // 点光源世界坐标
uniform vec3 uLightColor;  // 光源颜色（通常为 vec3(1)）
uniform vec3 uObjectColor; // 物体基础颜色

out vec4 FragColor;

void main()
{
    // 环境光
    float ambientStrength = 0.15;
    vec3  ambient = ambientStrength * uLightColor;

    // 漫反射（Lambert 余弦定律）
    vec3  norm      = normalize(vNormal);
    vec3  lightDir  = normalize(uLightPos - vFragPos);
    float diff      = max(dot(norm, lightDir), 0.0);
    vec3  diffuse   = diff * uLightColor;

    // 最终颜色 = (ambient + diffuse) * 物体颜色
    vec3 result = (ambient + diffuse) * uObjectColor;
    FragColor   = vec4(result, 1.0);
}
