#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;  // transpose(inverse(mat3(uModel)))

out vec3 vFragPos;   // world space
out vec3 vNormal;    // world space
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos  = vec3(worldPos);
    vNormal   = normalize(uNormalMatrix * aNormal);
    vUV       = aUV;
    gl_Position = uProjection * uView * worldPos;
}
