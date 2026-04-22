#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec4 FragPosLightSpace;
} vs_out;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vs_out.FragPos         = worldPos.xyz;
    vs_out.Normal          = mat3(transpose(inverse(uModel))) * aNormal;
    vs_out.FragPosLightSpace = uLightSpaceMatrix * worldPos;
    gl_Position = uProjection * uView * worldPos;
}
