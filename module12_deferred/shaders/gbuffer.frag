#version 460 core
// Geometry pass — writes to three render targets (MRT)
in vec3 FragPos;
in vec3 Normal;

layout(location = 0) out vec3 gPosition;   // GL_COLOR_ATTACHMENT0
layout(location = 1) out vec3 gNormal;     // GL_COLOR_ATTACHMENT1
layout(location = 2) out vec4 gAlbedoSpec; // GL_COLOR_ATTACHMENT2

uniform vec3  uAlbedo;
uniform float uSpecular;

void main() {
    gPosition   = FragPos;
    gNormal     = normalize(Normal);
    gAlbedoSpec = vec4(uAlbedo, uSpecular);
}
