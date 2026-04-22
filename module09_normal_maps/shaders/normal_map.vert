#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;  // transpose(inverse(mat3(uModel)))

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;   // 切线空间中的光源位置
    vec3 TangentViewPos;    // 切线空间中的摄像机位置
    vec3 TangentFragPos;    // 切线空间中的片段位置
} vs_out;

uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vs_out.FragPos   = vec3(worldPos);
    vs_out.TexCoords = aUV;

    // Build TBN in world space then transpose to go world→tangent
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(uNormalMatrix * aNormal);
    // Re-orthogonalize T w.r.t. N (Gram-Schmidt in case of precision loss)
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    // Transpose of TBN = inverse (because TBN is orthonormal)
    // Multiplying by transpose transforms world→tangent
    mat3 TBN_inv = transpose(mat3(T, B, N));

    vs_out.TangentLightPos = TBN_inv * uLightPos;
    vs_out.TangentViewPos  = TBN_inv * uViewPos;
    vs_out.TangentFragPos  = TBN_inv * vs_out.FragPos;

    gl_Position = uProjection * uView * worldPos;
}
