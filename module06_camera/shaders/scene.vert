#version 460 core
// ── 场景顶点着色器（含法向量变换）────────────────────────────────────────────

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;   // transpose(inverse(mat3(uModel)))

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vTexCoords;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos      = worldPos.xyz;
    vNormal       = normalize(uNormalMatrix * aNormal);
    vTexCoords    = aTexCoords;
    gl_Position   = uProjection * uView * worldPos;
}
