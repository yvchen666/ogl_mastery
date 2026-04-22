#version 460 core

in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHdrBuffer;
uniform float     uThreshold;  // default 1.0

void main() {
    vec3 color = texture(uHdrBuffer, vTexCoord).rgb;
    // Luminance-based extraction: keep pixels brighter than threshold
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > uThreshold)
        FragColor = vec4(color, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
