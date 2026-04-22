#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

// SSBO: per-instance transform matrices (binding = 0)
// std430 layout: mat4 はそのまま 64 bytes，配列要素も 64 bytes
layout(std430, binding = 0) readonly buffer InstanceData {
    mat4 uModelMatrices[];
};

// Camera UBO (binding = 0)
layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    vec3 uCamPos;
    float _pad;
};

out vec3 vNormal;
out vec3 vWorldPos;
flat out int vInstanceID;

void main() {
    // gl_InstanceID: 当前实例索引（0 到 instance_count-1）
    // base_instance 非零时：gl_InstanceID + base_instance = gl_BaseInstance + gl_InstanceID
    // 实际上 gl_InstanceID 已包含 base_instance 偏移（OpenGL 4.2+）
    mat4 model = uModelMatrices[gl_InstanceID];

    vec4 world_pos = model * vec4(aPos, 1.0);
    vWorldPos      = world_pos.xyz;
    vNormal        = mat3(transpose(inverse(model))) * aNormal;
    vInstanceID    = gl_InstanceID;

    gl_Position = uProjection * uView * world_pos;
}
