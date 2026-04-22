#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vLocalPos;

void main() {
    vLocalPos   = aPos;
    // Remove translation for cube projection
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
