#version 460 core
// Separable Gaussian blur — vertical pass
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uImage;

const float WEIGHTS[5] = float[](0.227027, 0.194595, 0.121622, 0.054054, 0.016216);

void main() {
    vec2 texOffset = 1.0 / textureSize(uImage, 0);
    vec3 result = texture(uImage, vTexCoord).rgb * WEIGHTS[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(uImage, vTexCoord + vec2(0.0, texOffset.y * i)).rgb * WEIGHTS[i];
        result += texture(uImage, vTexCoord - vec2(0.0, texOffset.y * i)).rgb * WEIGHTS[i];
    }
    FragColor = vec4(result, 1.0);
}
