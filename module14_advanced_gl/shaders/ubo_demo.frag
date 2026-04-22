#version 460 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

// Camera UBO (binding = 0)
layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    vec3 uCamPos;
    float _pad;
};

// Lights UBO (binding = 1)
// 注意 std140 中：vec3 数组元素占 16 字节（不是 12），所以用 vec4
layout(std140, binding = 1) uniform LightBlock {
    vec4 uLightPositions[4];  // xyz = position, w ignored
    vec4 uLightColors[4];     // rgb = color,    w ignored
    int  uLightCount;
    float _lpad[3];
};

void main() {
    vec3 N    = normalize(vNormal);
    vec3 V    = normalize(uCamPos - vWorldPos);
    vec3 base = vec3(0.6, 0.7, 0.9);  // 蓝灰色

    vec3 result = vec3(0.0);
    for (int i = 0; i < uLightCount; ++i) {
        vec3  L    = normalize(uLightPositions[i].xyz - vWorldPos);
        float diff = max(dot(N, L), 0.0);
        vec3  H    = normalize(V + L);
        float spec = pow(max(dot(N, H), 0.0), 32.0);
        float dist = length(uLightPositions[i].xyz - vWorldPos);
        float att  = 1.0 / (1.0 + 0.01 * dist * dist);
        result += att * uLightColors[i].rgb * (base * diff + vec3(spec));
    }
    result += base * 0.05; // ambient

    FragColor = vec4(result, 1.0);
}
