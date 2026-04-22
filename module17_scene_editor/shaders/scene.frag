#version 460 core
in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTexCoords;

uniform sampler2D uAlbedo;
uniform bool      uHasTexture;
uniform vec3      uLightPos;
uniform vec3      uLightColor;
uniform float     uLightIntensity;
uniform vec3      uCamPos;
uniform bool      uSelected;  // 选中高亮

out vec4 FragColor;

void main() {
    vec3 albedo = uHasTexture ? texture(uAlbedo, vTexCoords).rgb : vec3(0.8);

    // ── Blinn-Phong 简单光照 ───────────────────────────────────────────
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vFragPos);
    vec3 V = normalize(uCamPos   - vFragPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3  ambient = albedo * 0.15;
    vec3  color = ambient
                + albedo * diff * uLightColor * uLightIntensity
                + vec3(0.3) * spec * uLightColor * uLightIntensity;

    // 选中描边（简化：叠加一层橙色）
    if (uSelected) color = mix(color, vec3(1.0, 0.5, 0.0), 0.4);

    FragColor = vec4(color, 1.0);
}
