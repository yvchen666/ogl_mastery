#version 460 core
// Full-screen quad vertex shader (vertex-ID trick, no VBO needed)
out vec2 vTexCoord;
void main() {
    vec2 pos  = vec2((gl_VertexID & 1)*2.0 - 1.0,
                     (gl_VertexID >> 1)*2.0 - 1.0);
    vTexCoord = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
