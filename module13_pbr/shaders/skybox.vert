#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vTexCoords;

void main() {
    vTexCoords  = aPos;
    vec4 pos    = uProjection * uView * vec4(aPos, 1.0);
    // Set z = w so depth is always 1.0 (background)
    gl_Position = pos.xyww;
}
