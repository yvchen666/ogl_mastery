#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

// Camera UBO (binding = 0)
// std140: 每个基础类型 4 字节，vec4 是 16 字节，mat4 是 64 字节
// 成员对齐到其 base alignment 的倍数
layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;           // offset 0,   size 64
    mat4 uProjection;     // offset 64,  size 64
    vec3 uCamPos;         // offset 128, size 12
    float _pad;           // offset 140, size 4  → total 144 bytes
};

uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos   = world.xyz;
    vNormal     = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProjection * uView * world;
}
