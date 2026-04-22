#version 460 core
// ── 场景片段着色器：Blinn-Phong 光照 ─────────────────────────────────────────

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTexCoords;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform vec3 uViewPos;

out vec4 FragColor;

void main()
{
    // 环境光
    float ambientStrength = 0.15;
    vec3  ambient = ambientStrength * uLightColor;

    // 漫反射
    vec3  norm      = normalize(vNormal);
    vec3  lightDir  = normalize(uLightPos - vFragPos);
    float diff      = max(dot(norm, lightDir), 0.0);
    vec3  diffuse   = diff * uLightColor;

    // 高光（Blinn-Phong）
    float specStrength = 0.5;
    vec3  viewDir   = normalize(uViewPos - vFragPos);
    vec3  halfDir   = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(norm, halfDir), 0.0), 64.0);
    vec3  specular  = specStrength * spec * uLightColor;

    vec3 result = (ambient + diffuse + specular) * uObjectColor;
    FragColor   = vec4(result, 1.0);
}
