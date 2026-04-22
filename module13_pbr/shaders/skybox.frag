#version 460 core

in vec3 vTexCoords;
out vec4 FragColor;

uniform samplerCube uEnvMap;

void main() {
    vec3 color = texture(uEnvMap, vTexCoords).rgb;
    // Tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
