#version 460 core
// ── MVP 顶点着色器 ────────────────────────────────────────────────────────────
//   输入:  aPos(0)  aNormal(1)  aTexCoords(2)
//   输出:  vNormal(世界空间), vFragPos(世界空间), vTexCoords

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

// ── uniform ──────────────────────────────────────────────────────────────────
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;   // = transpose(inverse(mat3(uModel)))

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vTexCoords;

void main()
{
    // 局部空间 → 世界空间
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    vFragPos    = worldPos.xyz;

    // 法向量需要经过 (M⁻¹)ᵀ 变换，保证非均匀缩放时方向仍垂直于表面
    vNormal     = normalize(uNormalMatrix * aNormal);

    vTexCoords  = aTexCoords;

    // 世界空间 → 裁剪空间
    gl_Position = uProjection * uView * worldPos;
}
