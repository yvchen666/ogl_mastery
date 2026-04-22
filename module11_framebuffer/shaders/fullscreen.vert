#version 460 core
// Full-screen quad vertex shader
// Generates a full-screen triangle from gl_VertexID (no VBO needed)

out vec2 vTexCoord;

void main() {
    // Two triangles spanning NDC [-1,1] using vertex index trick
    vec2 pos = vec2((gl_VertexID & 1) * 2.0 - 1.0,
                    (gl_VertexID >> 1) * 2.0 - 1.0);
    vTexCoord   = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
