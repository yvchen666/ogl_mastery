#version 460 core

// ─── 每粒子数据来自 SSBO ─────────────────────────────────────
struct Particle {
    vec4 position;  // xyz=pos, w=age
    vec4 velocity;  // xyz=vel, w=life_max
    vec4 color;     // rgba
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;
uniform int  uParticleCount;

out vec4 vColor;
out vec2 vUV;

// billboard 四个顶点的局部偏移（Triangle Strip）
const vec2 QUAD_OFFSETS[4] = vec2[](
    vec2(-0.5,  0.5),   // 顶点 0：左上
    vec2(-0.5, -0.5),   // 顶点 1：左下
    vec2( 0.5,  0.5),   // 顶点 2：右上
    vec2( 0.5, -0.5)    // 顶点 3：右下
);
const vec2 QUAD_UVS[4] = vec2[](
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

void main() {
    int particle_idx = gl_VertexID / 4;
    int corner_idx   = gl_VertexID % 4;

    if (particle_idx >= uParticleCount) {
        gl_Position = vec4(0.0);
        vColor = vec4(0.0);
        return;
    }

    Particle p = particles[particle_idx];
    vColor = p.color;
    vUV    = QUAD_UVS[corner_idx];

    // ─── Billboard：用相机右向量和上向量展开四边形 ──────────
    // 从 View 矩阵中直接提取相机的右和上方向（View 矩阵的转置是相机的世界方向）
    vec3 cam_right = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cam_up    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    float size = 0.15;  // 粒子世界空间大小（米）
    vec2 offset = QUAD_OFFSETS[corner_idx];
    vec3 world_pos = p.position.xyz
                   + cam_right * offset.x * size
                   + cam_up    * offset.y * size;

    gl_Position = uProj * uView * vec4(world_pos, 1.0);
}
